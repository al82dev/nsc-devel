"""Process execution with complete capture and no automatic deadline."""

from __future__ import annotations

import os
import signal
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ProcessRequest:
    command: tuple[str, ...]
    working_directory: Path
    stdin: str | None = None
    environment: dict[str, str] | None = None


@dataclass(frozen=True)
class ProcessResult:
    command: tuple[str, ...]
    working_directory: Path
    exit_code: int | None
    stdout: str
    stderr: str
    duration_seconds: float


class ProcessExecutor:
    """Runs one command without a shell and captures all observable output."""

    def execute(self, request: ProcessRequest) -> ProcessResult:
        if not request.command:
            raise ValueError("process command cannot be empty")
        working_directory = request.working_directory.resolve()
        if not working_directory.is_dir():
            raise ValueError(f"working directory does not exist: {working_directory}")

        environment = os.environ.copy()
        if request.environment is not None:
            environment.update(request.environment)
        creation_flags = 0
        start_new_session = False
        if os.name == "nt":
            creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP
        else:
            start_new_session = True

        started = time.perf_counter()
        process = subprocess.Popen(
            list(request.command),
            cwd=working_directory,
            stdin=subprocess.PIPE if request.stdin is not None else subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=environment,
            shell=False,
            creationflags=creation_flags,
            start_new_session=start_new_session,
        )
        stdout, stderr = process.communicate(input=request.stdin)
        duration = time.perf_counter() - started
        return ProcessResult(
            command=request.command,
            working_directory=working_directory,
            exit_code=process.returncode,
            stdout=stdout,
            stderr=stderr,
            duration_seconds=duration,
        )

    @staticmethod
    def _terminate_tree(process: subprocess.Popen[str]) -> None:
        if process.poll() is not None:
            return
        if os.name == "nt":
            subprocess.run(
                ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                shell=False,
            )
            return
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            return
