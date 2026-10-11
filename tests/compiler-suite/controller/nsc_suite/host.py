"""Process adapter for one case in the consolidated host test executable."""

from __future__ import annotations

import json
import subprocess
import time
from pathlib import Path

from .manifest import CaseManifest
from .result import CaseResult, CaseStatus
from .workspace import CaseWorkspace


class HostExecutor:
    def __init__(
        self, command: tuple[str, ...], executor: str = "nsc_component_tests"
    ) -> None:
        if not command or any(not part for part in command):
            raise ValueError("host executor command cannot be empty")
        if executor not in {
            "nsc_component_tests",
            "nsc_component_tests_destructive",
            "nsc_component_tests_source_extended",
        }:
            raise ValueError(f"unsupported host executor identity: {executor}")
        self.command = command
        self.executor = executor

    def __call__(self, case: CaseManifest, workspace: CaseWorkspace) -> CaseResult:
        if case.executor != self.executor:
            return _infrastructure_error(
                case.case_id,
                f"{self.executor} adapter cannot run {case.executor}",
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
                check=False,
            )
        except OSError as error:
            _write_logs(workspace, "", str(error))
            return _infrastructure_error(case.case_id, f"cannot launch host executable: {error}")

        duration = time.monotonic() - started
        _write_logs(workspace, completed.stdout, completed.stderr)
        lines = tuple(line for line in completed.stdout.splitlines() if line.strip())
        if len(lines) != 1:
            try:
                abnormal_detail = _expected_abnormal_termination_detail(
                    case,
                    completed.returncode,
                    completed.stdout + "\n" + completed.stderr,
                )
            except ValueError as error:
                return _infrastructure_error(
                    case.case_id,
                    f"host executable emitted {len(lines)} non-empty result lines; expected 1; {error}",
                    duration,
                )
            if abnormal_detail is not None:
                return CaseResult(
                    case.case_id,
                    CaseStatus.KNOWN_FAILURE,
                    duration_seconds=duration,
                    failures=(abnormal_detail,),
                )
            return _infrastructure_error(
                case.case_id,
                f"host executable emitted {len(lines)} non-empty result lines; expected 1",
                duration,
            )
        try:
            payload = json.loads(lines[0], object_pairs_hook=_unique_json_object)
        except (json.JSONDecodeError, ValueError) as error:
            try:
                abnormal_detail = _expected_abnormal_termination_detail(
                    case,
                    completed.returncode,
                    completed.stdout + "\n" + completed.stderr,
                )
            except ValueError as contract_error:
                return _infrastructure_error(
                    case.case_id,
                    f"host executable emitted invalid JSON: {error}; {contract_error}",
                    duration,
                )
            if abnormal_detail is not None:
                return CaseResult(
                    case.case_id,
                    CaseStatus.KNOWN_FAILURE,
                    duration_seconds=duration,
                    failures=(abnormal_detail,),
                )
            return _infrastructure_error(
                case.case_id, f"host executable emitted invalid JSON: {error}", duration
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
        duration_us = payload.get("duration_us")
        if (
            status_text not in {"PASS", "FAIL"}
            or type(duration_us) is not int
            or duration_us < 0
            or not isinstance(failures, list)
            or any(not isinstance(item, str) or not item for item in failures)
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
                if "abnormal_termination" in case.oracle:
                    return CaseResult(
                        case.case_id,
                        CaseStatus.FAIL,
                        duration_seconds=duration,
                        failures=(
                            "known defect emitted a structured failure instead of its exact "
                            "abnormal terminal",
                        ),
                    )
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


def _expected_abnormal_termination_detail(
    case: CaseManifest, exit_code: int, captured: str
) -> str | None:
    """Accept only the exact terminal kind, code, and evidence declared by one KPD."""
    abnormal = case.oracle.get("abnormal_termination")
    if abnormal is None:
        return None
    if case.expected_outcome != "KNOWN_FAILURE":
        raise ValueError("abnormal termination contract belongs to a non-defect case")
    if not {"known-defect", "unsafe-boundary"} <= set(case.tags):
        raise ValueError("abnormal termination contract lacks required defect tags")
    if not isinstance(abnormal, dict) or set(abnormal) != {
        "kind",
        "exit_code",
        "output_contains",
    }:
        raise ValueError("invalid abnormal termination contract")
    expected_kind = abnormal["kind"]
    expected_code = int(abnormal["exit_code"], 16)
    actual_code = exit_code & 0xFFFFFFFF
    actual_kind = "windows-status" if (actual_code & 0xC0000000) == 0xC0000000 else "process-exit"
    if actual_kind != expected_kind or actual_code != expected_code:
        raise ValueError(
            "host terminal mismatch: expected "
            f"{expected_kind}/{abnormal['exit_code']}, got {actual_kind}/0x{actual_code:08X}"
        )
    fragments = abnormal["output_contains"]
    missing = [fragment for fragment in fragments if fragment not in captured]
    if missing:
        raise ValueError(
            "host terminal evidence omits: " + ", ".join(repr(value) for value in missing)
        )
    return (
        "known product defect matched exact terminal "
        f"{actual_kind}/0x{actual_code:08X}"
        + ("; " + "; ".join(f"observed {value!r}" for value in fragments) if fragments else "")
    )


def _write_logs(workspace: CaseWorkspace, stdout: str, stderr: str) -> None:
    workspace.logs.joinpath("stdout.log").write_text(stdout, encoding="utf-8", newline="\n")
    workspace.logs.joinpath("stderr.log").write_text(stderr, encoding="utf-8", newline="\n")


def _unique_json_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    """Build one host object only when every key has one definition."""
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def _infrastructure_error(
    case_id: str, message: str, duration_seconds: float = 0.0
) -> CaseResult:
    return CaseResult(
        case_id,
        CaseStatus.INFRASTRUCTURE_ERROR,
        duration_seconds=duration_seconds,
        failures=(message,),
    )
