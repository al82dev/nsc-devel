"""Process-owned executors for observable NSC product contracts."""

from __future__ import annotations

import json
import re
import subprocess
import time
from pathlib import Path
from typing import Any

from .manifest import CaseManifest, TIMEOUT_SECONDS
from .oracle import compare_structured
from .result import CaseResult, CaseStatus
from .workspace import CaseWorkspace


BUILD_ID = re.compile(r"^[0-9a-f]{16}$")
SEMVER = re.compile(
    r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
    r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?"
    r"(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$"
)


class ProductContractError(RuntimeError):
    """A launched NSC process violated the selected observable contract."""


class ExecutorConfigurationError(RuntimeError):
    """A selected case lacks an explicit tool or build artifact."""


class CompilerIdentityExecutor:
    """Execute the four CB-BAS-006 cases against explicit compiler artifacts."""

    def __init__(
        self,
        primary_command: tuple[str, ...],
        comparison_command: tuple[str, ...] | None = None,
        build_id_header: Path | None = None,
    ) -> None:
        if not primary_command or any(not part for part in primary_command):
            raise ValueError("primary NSC command cannot be empty")
        if comparison_command is not None and (
            not comparison_command or any(not part for part in comparison_command)
        ):
            raise ValueError("comparison NSC command cannot be empty")
        self.primary_command = primary_command
        self.comparison_command = comparison_command
        self.build_id_header = build_id_header

    def __call__(self, case: CaseManifest, workspace: CaseWorkspace) -> CaseResult:
        if case.executor != "nsc_suite_controller+nsc":
            return _infrastructure_error(
                case.case_id, f"compiler process executor cannot run {case.executor}"
            )
        operation = case.inputs.get("operation")
        if operation not in {
            "compiler-version-constants",
            "generated-identity-exports",
            "distinct-build-identities",
            "semantic-version",
        }:
            return _infrastructure_error(
                case.case_id, f"unsupported compiler identity operation: {operation!r}"
            )

        started = time.monotonic()
        try:
            failures = self._execute_operation(operation, case, workspace)
        except subprocess.TimeoutExpired as error:
            return _infrastructure_error(
                case.case_id,
                f"compiler identity command timed out after {error.timeout} seconds",
                time.monotonic() - started,
            )
        except OSError as error:
            return _infrastructure_error(
                case.case_id,
                f"cannot access compiler identity input: {error}",
                time.monotonic() - started,
            )
        except ExecutorConfigurationError as error:
            return _infrastructure_error(
                case.case_id, str(error), time.monotonic() - started
            )
        except ProductContractError as error:
            failures = (str(error),)

        duration = time.monotonic() - started
        if failures:
            return CaseResult(
                case.case_id,
                CaseStatus.FAIL,
                duration_seconds=duration,
                failures=tuple(failures),
            )
        return CaseResult(case.case_id, CaseStatus.PASS, duration_seconds=duration)

    def _execute_operation(
        self,
        operation: str,
        case: CaseManifest,
        workspace: CaseWorkspace,
    ) -> tuple[str, ...]:
        primary = self._identity(self.primary_command, workspace, "primary-identity", case)
        if operation == "compiler-version-constants":
            matrix = self._key_values(
                self._invoke(
                    self.primary_command,
                    ("--print-compatibility-matrix",),
                    workspace,
                    "compatibility-matrix",
                    case,
                ),
                "NSCOMPAT-MATRIX 1",
                repeated_keys=frozenset({"row"}),
            )
            actual = {
                "compiler_release": primary.get("compiler_release"),
                "language": primary.get("language"),
                "previous_release": matrix.get("compiler.N-1"),
                "stage": primary.get("stage"),
            }
            return _compare(case.oracle, actual)

        if operation == "generated-identity-exports":
            if self.build_id_header is None:
                raise ExecutorConfigurationError(
                    "generated build identity header was not configured"
                )
            generated = _read_generated_identity(self.build_id_header)
            actual = {
                "build_id_is_lower_hex_16": BUILD_ID.fullmatch(primary.get("build", ""))
                is not None,
                "build_matches_generated": primary.get("build") == generated["build"],
                "version_matches_generated": primary.get("compiler_version")
                == generated["compiler_version"],
                "version_uses_release": primary.get("compiler_version", "").startswith(
                    primary.get("compiler_release", "") + "+"
                ),
            }
            return _compare(case.oracle, actual)

        if operation == "distinct-build-identities":
            if self.comparison_command is None:
                raise ExecutorConfigurationError(
                    "comparison NSC command was not configured"
                )
            comparison = self._identity(
                self.comparison_command, workspace, "comparison-identity", case
            )
            actual = {
                "build_ids_distinct": primary.get("build") != comparison.get("build"),
                "versions_distinct": primary.get("compiler_version")
                != comparison.get("compiler_version"),
            }
            return _compare(case.oracle, actual)

        version = primary.get("compiler_version", "")
        release = primary.get("compiler_release", "")
        match = SEMVER.fullmatch(version)
        actual = {
            "release": release,
            "release_matches_semver_core": match is not None
            and ".".join(match.groups()[:3]) == release,
            "semver_valid": match is not None,
        }
        return _compare(case.oracle, actual)

    def _identity(
        self,
        command: tuple[str, ...],
        workspace: CaseWorkspace,
        stem: str,
        case: CaseManifest,
    ) -> dict[str, str]:
        return self._key_values(
            self._invoke(
                command,
                ("--print-compiler-identity",),
                workspace,
                stem,
                case,
            ),
            "NSCOMPILER 1",
        )

    def _invoke(
        self,
        command: tuple[str, ...],
        arguments: tuple[str, ...],
        workspace: CaseWorkspace,
        stem: str,
        case: CaseManifest,
    ) -> str:
        full_command = (*command, *arguments)
        workspace.logs.joinpath(f"{stem}.command.json").write_text(
            json.dumps(list(full_command), ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        completed = subprocess.run(
            full_command,
            cwd=workspace.outputs,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=TIMEOUT_SECONDS[case.timeout_class],
            check=False,
        )
        workspace.logs.joinpath(f"{stem}.stdout.log").write_text(
            completed.stdout, encoding="utf-8", newline="\n"
        )
        workspace.logs.joinpath(f"{stem}.stderr.log").write_text(
            completed.stderr, encoding="utf-8", newline="\n"
        )
        if completed.returncode != 0:
            raise ProductContractError(
                f"{stem} exited {completed.returncode}: {completed.stderr.strip()}"
            )
        return completed.stdout

    @staticmethod
    def _key_values(
        output: str,
        header: str,
        *,
        repeated_keys: frozenset[str] = frozenset(),
    ) -> dict[str, Any]:
        lines = output.splitlines()
        if not lines or lines[0] != header:
            raise ProductContractError(f"expected protocol header {header!r}")
        values: dict[str, Any] = {}
        for line in lines[1:]:
            if not line:
                continue
            if "=" not in line:
                raise ProductContractError(f"protocol line has no '=': {line!r}")
            key, value = line.split("=", 1)
            if not key:
                raise ProductContractError(f"invalid protocol key: {key!r}")
            if key in values and key not in repeated_keys:
                raise ProductContractError(f"invalid or duplicate protocol key: {key!r}")
            if key not in repeated_keys:
                values[key] = value
            elif key not in values:
                values[key] = [value]
            else:
                values[key].append(value)
        return values


def _read_generated_identity(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    patterns = {
        "build": r'kGeneratedCompilerBuildId\s*=\s*"([^"]+)"',
        "compiler_version": r'kGeneratedCompilerVersion\s*=\s*"([^"]+)"',
    }
    values: dict[str, str] = {}
    for key, pattern in patterns.items():
        match = re.search(pattern, text)
        if match is None:
            raise ProductContractError(f"generated identity header lacks {key}")
        values[key] = match.group(1)
    return values


def _compare(expected: dict[str, Any], actual: dict[str, Any]) -> tuple[str, ...]:
    return tuple(
        f"{failure.path}: {failure.reason}; expected={failure.expected!r}; actual={failure.actual!r}"
        for failure in compare_structured(expected, actual)
    )


def _infrastructure_error(
    case_id: str, message: str, duration_seconds: float = 0.0
) -> CaseResult:
    return CaseResult(
        case_id,
        CaseStatus.INFRASTRUCTURE_ERROR,
        duration_seconds=duration_seconds,
        failures=(message,),
    )
