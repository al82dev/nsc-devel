"""Stable JSON and full-text reports for raw process executions."""

from __future__ import annotations

import json
import os
import subprocess
from dataclasses import dataclass
from pathlib import Path

from .process import ProcessResult
from .verify import TestResult


@dataclass(frozen=True)
class TestRunRecord:
    case_id: str
    component: str
    process: ProcessResult


class ReportWriter:
    def __init__(self, root: Path) -> None:
        self.root = root.resolve()
        if self.root.drive.upper() != "A:":
            raise ValueError(f"report root must be on A: {self.root}")

    def write(
        self,
        run_id: str,
        nsc: Path,
        records: tuple[TestRunRecord, ...],
    ) -> Path:
        output = self.root / run_id
        output.mkdir(parents=True, exist_ok=False)
        payload = {
            "schema": 1,
            "run_id": run_id,
            "nsc": str(nsc.resolve()),
            "selected": len(records),
            "results": [self._record_payload(record) for record in records],
        }
        self._atomic_text(
            output / "summary.json",
            json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        )
        self._atomic_text(output / "report.log", self._log_text(run_id, nsc, records))
        return output

    def write_verified(
        self,
        run_id: str,
        nsc: Path,
        results: tuple[TestResult, ...],
    ) -> Path:
        output = self.root / run_id
        output.mkdir(parents=True, exist_ok=False)
        payload = {
            "schema": 1,
            "run_id": run_id,
            "nsc": str(nsc.resolve()),
            "selected": len(results),
            "passed": sum(1 for result in results if result.passed),
            "failed": sum(1 for result in results if not result.passed),
            "results": [
                {
                    **self._record_payload(
                        TestRunRecord(
                            case_id=result.case_id,
                            component=result.component,
                            process=result.process,
                        )
                    ),
                    "passed": result.passed,
                    "workspace": str(result.workspace),
                    "failures": [
                        {"kind": failure.kind.value, "message": failure.message}
                        for failure in result.failures
                    ],
                }
                for result in results
            ],
        }
        self._atomic_text(
            output / "summary.json",
            json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        )
        lines = [
            f"run_id={run_id}",
            f"nsc={nsc.resolve()}",
            f"selected={len(results)}",
            f"passed={payload['passed']}",
            f"failed={payload['failed']}",
            "",
        ]
        for result in results:
            lines.extend(
                [
                    f"case={result.case_id}",
                    f"component={result.component}",
                    f"passed={str(result.passed).lower()}",
                    f"command={subprocess.list2cmdline(list(result.process.command))}",
                    f"exit_code={result.process.exit_code}",
                    f"timed_out={str(result.process.timed_out).lower()}",
                ]
            )
            for failure in result.failures:
                lines.append(f"failure={failure.kind.value}: {failure.message}")
            lines.extend(
                [
                    "--- stdout ---",
                    result.process.stdout,
                    "--- stderr ---",
                    result.process.stderr,
                    "",
                ]
            )
        self._atomic_text(output / "report.log", "\n".join(lines))
        return output

    @staticmethod
    def _record_payload(record: TestRunRecord) -> dict[str, object]:
        process = record.process
        return {
            "case_id": record.case_id,
            "component": record.component,
            "command": list(process.command),
            "working_directory": str(process.working_directory),
            "exit_code": process.exit_code,
            "stdout": process.stdout,
            "stderr": process.stderr,
            "duration_seconds": process.duration_seconds,
            "timed_out": process.timed_out,
        }

    @staticmethod
    def _log_text(run_id: str, nsc: Path, records: tuple[TestRunRecord, ...]) -> str:
        lines = [f"run_id={run_id}", f"nsc={nsc.resolve()}", f"selected={len(records)}", ""]
        for record in records:
            process = record.process
            lines.extend(
                [
                    f"case={record.case_id}",
                    f"component={record.component}",
                    f"command={subprocess.list2cmdline(list(process.command))}",
                    f"working_directory={process.working_directory}",
                    f"exit_code={process.exit_code}",
                    f"timed_out={str(process.timed_out).lower()}",
                    f"duration_seconds={process.duration_seconds:.6f}",
                    "--- stdout ---",
                    process.stdout,
                    "--- stderr ---",
                    process.stderr,
                    "",
                ]
            )
        return "\n".join(lines)

    @staticmethod
    def _atomic_text(path: Path, content: str) -> None:
        temporary = path.with_suffix(path.suffix + ".tmp")
        temporary.write_text(content, encoding="utf-8", newline="\n")
        os.replace(temporary, path)
