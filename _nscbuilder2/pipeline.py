"""Sequential build/test pipeline for NSC Builder 2."""

from __future__ import annotations

import hashlib
import os
import subprocess
import threading
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable

from .model import RunPlan, RunStep
from .storage import BuilderStorage
from .summary import condensed_errors, format_condensed


OutputCallback = Callable[[str], None]
ProgressCallback = Callable[[int, int, str], None]


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


@dataclass(frozen=True)
class StepResult:
    identifier: str
    title: str
    command: tuple[str, ...]
    exit_code: int
    duration_seconds: float
    log: str

    def to_dict(self) -> dict[str, object]:
        return {
            "identifier": self.identifier,
            "title": self.title,
            "command": list(self.command),
            "exit_code": self.exit_code,
            "duration_seconds": self.duration_seconds,
            "log": self.log,
        }


@dataclass(frozen=True)
class RunResult:
    run_id: str
    status: str
    exit_code: int
    started_at: str
    finished_at: str
    duration_seconds: float
    compiler: str
    compiler_sha256: str | None
    steps: tuple[StepResult, ...]
    errors: tuple[str, ...]
    history_dir: str

    def to_dict(self) -> dict[str, object]:
        return {
            "run_id": self.run_id,
            "status": self.status,
            "exit_code": self.exit_code,
            "started_at": self.started_at,
            "finished_at": self.finished_at,
            "duration_seconds": self.duration_seconds,
            "compiler": self.compiler,
            "compiler_sha256": self.compiler_sha256,
            "steps": [step.to_dict() for step in self.steps],
            "errors": list(self.errors),
            "history_dir": self.history_dir,
        }


class PipelineRunner:
    def __init__(self) -> None:
        self._cancel = threading.Event()
        self._process_lock = threading.Lock()
        self._process: subprocess.Popen[str] | None = None

    def cancel(self) -> None:
        self._cancel.set()
        with self._process_lock:
            process = self._process
        if process is None or process.poll() is not None:
            return
        if os.name == "nt":
            subprocess.run(
                ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
            )
        else:
            process.terminate()

    def run(
        self,
        plan: RunPlan,
        on_output: OutputCallback | None = None,
        on_progress: ProgressCallback | None = None,
    ) -> RunResult:
        self._cancel.clear()
        output = on_output or (lambda _line: None)
        progress = on_progress or (lambda _done, _total, _title: None)
        started_at = _utc_now()
        started = time.perf_counter()
        combined_log = plan.history_dir / "output.log"
        step_results: list[StepResult] = []
        compiler_digest: str | None = getattr(plan, "expected_compiler_sha256", None)
        status = "passed"
        exit_code = 0
        record: dict[str, object] = {
            **plan.to_dict(),
            "status": "running",
            "started_at": started_at,
            "steps_completed": [],
            "output_log": str(combined_log),
        }
        BuilderStorage.write_run(plan.history_dir, record)
        with combined_log.open("w", encoding="utf-8", newline="\n") as combined:
            if compiler_digest is not None:
                if not plan.compiler.is_file() or _sha256(plan.compiler) != compiler_digest:
                    status = "failed"
                    exit_code = 4
                    output("Builder 2 error: compiler receipt is stale before execution\n")
            total = len(plan.steps)
            for index, step in enumerate(plan.steps):
                if status != "passed":
                    break
                if self._cancel.is_set():
                    status = "cancelled"
                    exit_code = 130
                    break
                progress(index, total, step.title)
                result = self._run_step(step, plan.history_dir, combined, output)
                step_results.append(result)
                record["steps_completed"] = [item.to_dict() for item in step_results]
                BuilderStorage.write_run(plan.history_dir, record)
                if result.exit_code != 0:
                    status = "cancelled" if self._cancel.is_set() else "failed"
                    exit_code = 130 if status == "cancelled" else result.exit_code
                    break
                if step.freezes_compiler:
                    if not plan.compiler.is_file():
                        status = "failed"
                        exit_code = 2
                        output(f"Builder 2 error: built compiler is missing: {plan.compiler}\n")
                        break
                    compiler_digest = _sha256(plan.compiler)
                    output(f"Builder 2 froze compiler SHA-256: {compiler_digest}\n")
                progress(index + 1, total, step.title)

            if status == "passed" and compiler_digest is not None:
                observed = _sha256(plan.compiler)
                if observed != compiler_digest:
                    status = "failed"
                    exit_code = 3
                    output("Builder 2 error: compiler changed after it was frozen\n")

        full_text = combined_log.read_text(encoding="utf-8", errors="replace")
        errors = condensed_errors(full_text)
        (plan.history_dir / "errors.log").write_text(
            format_condensed(errors) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        result = RunResult(
            run_id=plan.run_id,
            status=status,
            exit_code=exit_code,
            started_at=started_at,
            finished_at=_utc_now(),
            duration_seconds=round(time.perf_counter() - started, 6),
            compiler=str(plan.compiler),
            compiler_sha256=compiler_digest,
            steps=tuple(step_results),
            errors=tuple(errors),
            history_dir=str(plan.history_dir),
        )
        final_record = {**plan.to_dict(), **result.to_dict()}
        BuilderStorage.write_run(plan.history_dir, final_record)
        progress(len(step_results), len(plan.steps), status)
        return result

    def _run_step(
        self,
        step: RunStep,
        history_dir: Path,
        combined,
        output: OutputCallback,
    ) -> StepResult:
        step_log = history_dir / f"{step.identifier}.log"
        header = f"\n=== {step.title} ===\n$ {subprocess.list2cmdline(step.command)}\n"
        combined.write(header)
        combined.flush()
        output(header)
        environment = os.environ.copy()
        environment["NSC_BUILDER2"] = "1"
        creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
        started = time.perf_counter()
        with step_log.open("w", encoding="utf-8", newline="\n") as log:
            process = subprocess.Popen(
                list(step.command),
                cwd=step.cwd,
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                creationflags=creation_flags,
            )
            with self._process_lock:
                self._process = process
            assert process.stdout is not None
            try:
                for line in process.stdout:
                    log.write(line)
                    log.flush()
                    combined.write(line)
                    combined.flush()
                    output(line)
            finally:
                process.stdout.close()
            code = process.wait()
            with self._process_lock:
                self._process = None
        return StepResult(
            identifier=step.identifier,
            title=step.title,
            command=step.command,
            exit_code=code,
            duration_seconds=round(time.perf_counter() - started, 6),
            log=str(step_log),
        )
