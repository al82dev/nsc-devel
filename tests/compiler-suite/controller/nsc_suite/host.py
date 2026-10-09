"""Process adapter for one case in the consolidated host test executable."""

from __future__ import annotations

import json
import subprocess
import time
from pathlib import Path

from .manifest import CaseManifest
from .result import CaseResult, CaseStatus
from .workspace import CaseWorkspace


TIMEOUT_SECONDS = {
    "tiny": 5.0,
    "normal": 30.0,
    "extended": 180.0,
    "self-host": 900.0,
    "packaging": 900.0,
}


class HostExecutor:
    def __init__(self, command: tuple[str, ...]) -> None:
        if not command or any(not part for part in command):
            raise ValueError("host executor command cannot be empty")
        self.command = command

    def __call__(self, case: CaseManifest, workspace: CaseWorkspace) -> CaseResult:
        if case.executor != "nsc_component_tests":
            return _infrastructure_error(
                case.case_id, f"host executor cannot run {case.executor}"
            )
        command = (*self.command, "--case", case.case_id)
        started = time.monotonic()
        try:
            completed = subprocess.run(
                command,
                cwd=workspace.outputs,
                text=True,
                encoding="utf-8",
                errors="replace",
                capture_output=True,
                timeout=TIMEOUT_SECONDS[case.timeout_class],
                check=False,
            )
        except subprocess.TimeoutExpired as error:
            duration = time.monotonic() - started
            _write_logs(workspace, error.stdout or "", error.stderr or "")
            return CaseResult(
                case.case_id,
                CaseStatus.INFRASTRUCTURE_ERROR,
                duration_seconds=duration,
                failures=(f"host case timed out after {TIMEOUT_SECONDS[case.timeout_class]} seconds",),
            )
        except OSError as error:
            _write_logs(workspace, "", str(error))
            return _infrastructure_error(case.case_id, f"cannot launch host executable: {error}")

        duration = time.monotonic() - started
        _write_logs(workspace, completed.stdout, completed.stderr)
        lines = tuple(line for line in completed.stdout.splitlines() if line.strip())
        if len(lines) != 1:
            if case.expected_outcome == "KNOWN_FAILURE" and completed.returncode != 0 and not lines:
                return CaseResult(
                    case.case_id,
                    CaseStatus.KNOWN_FAILURE,
                    duration_seconds=duration,
                    failures=(
                        "known product defect terminated the isolated host process with "
                        f"exit code {completed.returncode}",
                    ),
                )
            return _infrastructure_error(
                case.case_id,
                f"host executable emitted {len(lines)} non-empty result lines; expected 1",
                duration,
            )
        try:
            payload = json.loads(lines[0])
        except json.JSONDecodeError as error:
            return _infrastructure_error(
                case.case_id, f"host executable emitted invalid JSON: {error.msg}", duration
            )
        if not isinstance(payload, dict) or payload.get("case_id") != case.case_id:
            return _infrastructure_error(
                case.case_id, "host result identity does not match selected case", duration
            )
        if payload.get("contract_id") != case.contract_id:
            return _infrastructure_error(
                case.case_id, "host result contract does not match selected case", duration
            )
        if payload.get("component") != case.component:
            return _infrastructure_error(
                case.case_id, "host result component does not match selected case", duration
            )
        status_text = payload.get("status")
        failures = payload.get("failures")
        if status_text not in {"PASS", "FAIL"} or not isinstance(failures, list) or any(
            not isinstance(item, str) for item in failures
        ):
            return _infrastructure_error(case.case_id, "host result schema is invalid", duration)
        expected_exit = 0 if status_text == "PASS" else 1
        if completed.returncode != expected_exit:
            return _infrastructure_error(
                case.case_id,
                f"host exit {completed.returncode} contradicts status {status_text}",
                duration,
            )
        if status_text == "PASS" and failures:
            return _infrastructure_error(
                case.case_id, "host PASS result contains failures", duration
            )
        if status_text == "FAIL" and not failures:
            return _infrastructure_error(
                case.case_id, "host FAIL result lacks failure evidence", duration
            )
        if case.expected_outcome == "KNOWN_FAILURE":
            if status_text == "FAIL":
                return CaseResult(
                    case.case_id,
                    CaseStatus.KNOWN_FAILURE,
                    duration_seconds=duration,
                    failures=tuple(failures),
                )
            return CaseResult(
                case.case_id,
                CaseStatus.UNEXPECTED_PASS,
                duration_seconds=duration,
                failures=("known product defect passed; remove its quarantine and update tracking",),
            )
        return CaseResult(
            case.case_id,
            CaseStatus(status_text),
            duration_seconds=duration,
            failures=tuple(failures),
        )


def _write_logs(workspace: CaseWorkspace, stdout: str, stderr: str) -> None:
    workspace.logs.joinpath("stdout.log").write_text(stdout, encoding="utf-8", newline="\n")
    workspace.logs.joinpath("stderr.log").write_text(stderr, encoding="utf-8", newline="\n")


def _infrastructure_error(
    case_id: str, message: str, duration_seconds: float = 0.0
) -> CaseResult:
    return CaseResult(
        case_id,
        CaseStatus.INFRASTRUCTURE_ERROR,
        duration_seconds=duration_seconds,
        failures=(message,),
    )
