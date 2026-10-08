"""Adapters for existing multi-step NSC verification suites.

The adapter deliberately preserves each owner's command and ordering.  It only
rebases mutable paths and freezes the selected compiler/linker for the run.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .process import ProcessRequest


NON_O0 = re.compile(r"(?:^|[^A-Za-z0-9])-?O[123](?:$|[^A-Za-z0-9])", re.IGNORECASE)
PROHIBITED_IDENTIFIERS = (
    "baseline",
    "certification",
    "distribution-final",
    "promotion",
)
SELFHOST_GENERATION_ORDER = (
    "nsc.selfhost.self-hosted-generation",
    "nsc.selfhost.functional-equivalence",
    "nsc.selfhost.repeated-fixed-point",
    "nsc.selfhost.cross-machine-determinism",
    "nsc.selfhost.low-level-parity",
    "nsc.selfhost.interop-parity",
    "nsc.selfhost.runtime-library",
    "nsc.selfhost.advanced-toolchain",
    "nsc.selfhost.developer-experience",
    "nsc.selfhost.robustness-final",
)


class SuiteError(ValueError):
    """Raised when a specialized suite cannot be adapted safely."""


@dataclass(frozen=True)
class SuiteDefinition:
    identifier: str
    component: str
    owner: Path
    command: tuple[str, ...]
    timeout_seconds: float
    blocked_reason: str | None

    @property
    def runnable(self) -> bool:
        return self.blocked_reason is None


@dataclass(frozen=True)
class SuiteCatalog:
    matrix: Path
    repo_root: Path
    suites: tuple[SuiteDefinition, ...]

    @staticmethod
    def load(matrix: Path, repo_root: Path) -> "SuiteCatalog":
        resolved_matrix = matrix.resolve()
        resolved_repo = repo_root.resolve()
        try:
            payload = json.loads(resolved_matrix.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise SuiteError(f"cannot read suite matrix {resolved_matrix}: {error}") from error
        rows = payload.get("tests") if isinstance(payload, dict) else None
        if not isinstance(rows, list):
            raise SuiteError(f"suite matrix has no tests array: {resolved_matrix}")

        suites: list[SuiteDefinition] = []
        identifiers: set[str] = set()
        for row in rows:
            if not isinstance(row, dict) or row.get("destination") != "KEEP_SPECIALIZED":
                continue
            identifier = _required_text(row, "ctest_id", resolved_matrix)
            if identifier in identifiers:
                raise SuiteError(f"duplicate specialized suite id: {identifier}")
            identifiers.add(identifier)
            component = _required_text(row, "primary_component", resolved_matrix)
            owner_text = _required_text(row, "owner_path", resolved_matrix)
            owner = (resolved_repo / Path(owner_text)).resolve()
            if not owner.is_relative_to(resolved_repo) or not owner.is_file():
                raise SuiteError(f"suite owner does not exist inside repository: {owner_text}")
            command_value = row.get("command")
            if not isinstance(command_value, list) or any(
                not isinstance(item, str) or not item for item in command_value
            ):
                raise SuiteError(f"suite command is invalid: {identifier}")
            command = tuple(command_value)
            timeout_value = row.get("timeout_seconds", 900.0)
            if not isinstance(timeout_value, (int, float)) or isinstance(timeout_value, bool):
                raise SuiteError(f"suite timeout is invalid: {identifier}")
            blocked = _scope_block(identifier, command, owner)
            suites.append(
                SuiteDefinition(
                    identifier=identifier,
                    component=component,
                    owner=owner,
                    command=command,
                    timeout_seconds=float(timeout_value),
                    blocked_reason=blocked,
                )
            )
        return SuiteCatalog(
            matrix=resolved_matrix,
            repo_root=resolved_repo,
            suites=tuple(suites),
        )

    def require(self, identifier: str) -> SuiteDefinition:
        for suite in self.suites:
            if suite.identifier == identifier:
                return suite
        raise SuiteError(f"unknown specialized suite: {identifier}")

    def by_component(self, component: str) -> tuple[SuiteDefinition, ...]:
        selected = tuple(suite for suite in self.suites if suite.component == component)
        return _ordered_suites(selected)

    def runnable(self) -> tuple[SuiteDefinition, ...]:
        return _ordered_suites(tuple(suite for suite in self.suites if suite.runnable))


@dataclass(frozen=True)
class SuiteAdapter:
    nsc: Path
    nslink: Path | None
    repo_root: Path
    build_dir: Path
    prefix: Path
    evidence_root: Path

    def __post_init__(self) -> None:
        for field in ("nsc", "repo_root", "build_dir", "prefix"):
            value = getattr(self, field).resolve()
            if field == "nsc" and not value.is_file():
                raise SuiteError(f"NSC does not exist: {value}")
            if field != "nsc" and not value.is_dir():
                raise SuiteError(f"suite {field} does not exist: {value}")
            object.__setattr__(self, field, value)
        if self.nslink is not None:
            resolved_nslink = self.nslink.resolve()
            if not resolved_nslink.is_file():
                raise SuiteError(f"NSLink does not exist: {resolved_nslink}")
            object.__setattr__(self, "nslink", resolved_nslink)
        evidence = self.evidence_root.resolve()
        if evidence.drive.upper() != "A:":
            raise SuiteError(f"suite evidence must stay on A: {evidence}")
        evidence.mkdir(parents=True, exist_ok=True)
        object.__setattr__(self, "evidence_root", evidence)

    def build(self, suite: SuiteDefinition) -> ProcessRequest:
        if not suite.runnable:
            raise SuiteError(f"suite is blocked by scope: {suite.identifier}: {suite.blocked_reason}")
        command = list(suite.command)
        command[0] = self._tool(command[0])
        adapted = [command[0]]
        index = 1
        while index < len(command):
            argument = command[index]
            adapted.append(argument)
            # Some historical Python owners receive tools as positional option
            # values instead of CMake -D definitions.  Freeze both values here;
            # otherwise a passing suite could silently exercise an installed NSC
            # or write into the legacy build tree.
            if argument == "--compiler" and index + 1 < len(command):
                adapted.append(self.nsc.as_posix())
                index += 2
                continue
            if argument == "--work-root" and index + 1 < len(command):
                suite_work = (self.evidence_root / suite.identifier / "work").resolve()
                if not suite_work.is_relative_to(self.evidence_root):
                    raise SuiteError(f"suite work root escapes evidence root: {suite.identifier}")
                adapted.append(suite_work.as_posix())
                index += 2
                continue
            adapted[-1] = self._argument(argument)
            index += 1
        environment = {
            "NSC_LINKER": str(self.nslink) if self.nslink is not None else "",
            "NS_ROOT": str(self.prefix),
            "NS_LIB_ROOT": str(self.prefix / "lib"),
            "NSC_SELFHOST_PROFILES": "O0",
            "NSC_TEST_SUITE_ID": suite.identifier,
            "NSC_TEST_EVIDENCE_ROOT": str(self.evidence_root),
        }
        return ProcessRequest(
            command=tuple(adapted),
            working_directory=self.repo_root,
            timeout_seconds=suite.timeout_seconds,
            environment=environment,
        )

    @staticmethod
    def _tool(original: str) -> str:
        name = Path(original).name.lower()
        if name in {"python.exe", "python3.exe"}:
            return sys.executable
        if name in {"cmake.exe", "cmake"}:
            return shutil.which("cmake") or original
        return original

    def _argument(self, argument: str) -> str:
        if argument.startswith("-D") and "=" in argument:
            name, value = argument[2:].split("=", 1)
            return f"-D{name}={self._definition_value(name, value)}"
        return self._rebase_repo(argument)

    def _definition_value(self, name: str, value: str) -> str:
        upper = name.upper()
        if upper == "NSC_EXECUTABLE":
            return self.nsc.as_posix()
        if upper == "NSLINK_EXECUTABLE":
            if self.nslink is None:
                raise SuiteError("specialized suite requires an explicit NSLink path")
            return self.nslink.as_posix()
        if upper == "PYTHON_EXECUTABLE":
            return Path(sys.executable).resolve().as_posix()
        if upper == "CTEST_EXECUTABLE":
            return (shutil.which("ctest") or value).replace("\\", "/")
        if upper == "BUILD_DIR":
            return self.build_dir.as_posix()

        normalized = value.replace("\\", "/")
        marker = "/build-test-nslink/"
        position = normalized.lower().find(marker)
        if position >= 0:
            relative = normalized[position + len(marker) :]
            if upper in {"RUNTIME_LIBRARY", "RUNTIME_ABI"}:
                return (self.build_dir / Path(relative)).as_posix()
            if upper.endswith("OUTPUT_DIR") or upper == "OUTPUT_DIR" or upper == "SH41_OUTPUT_DIR":
                output = (self.evidence_root / Path(relative)).resolve()
                if not output.is_relative_to(self.evidence_root):
                    raise SuiteError(f"suite output escapes evidence root: {value}")
                output.mkdir(parents=True, exist_ok=True)
                return output.as_posix()
            return (self.build_dir / Path(relative)).as_posix()
        return self._rebase_repo(value)

    def _rebase_repo(self, value: str) -> str:
        normalized = value.replace("\\", "/")
        old_root = "A:/nsc-devel"
        if normalized.lower() == old_root.lower():
            return self.repo_root.as_posix()
        prefix = old_root + "/"
        if normalized.lower().startswith(prefix.lower()):
            relative = normalized[len(prefix) :]
            return (self.repo_root / Path(relative)).as_posix()
        return value


def _required_text(row: dict[str, Any], key: str, matrix: Path) -> str:
    value = row.get(key)
    if not isinstance(value, str) or not value:
        raise SuiteError(f"{key} is missing in {matrix}")
    return value


def _scope_block(identifier: str, command: tuple[str, ...], owner: Path) -> str | None:
    lowered = identifier.lower()
    if any(token in lowered for token in PROHIBITED_IDENTIFIERS):
        return "baseline/certification/distribution/promotion is outside migration scope"
    if "performance" in lowered:
        return "performance verifier executes an O0/O1/O2/O3 matrix"
    if any(argument.upper().startswith("-DCTEST_EXECUTABLE=") for argument in command):
        return "owner delegates to the legacy CTest tree outside isolated suite execution"
    owner_text = owner.read_text(encoding="utf-8", errors="replace")
    if NON_O0.search(" ".join(command)):
        return "owner contains an O1/O2/O3 execution path"
    # Python unittest owners may assert profile names as data without invoking
    # the compiler. CMake owners, in contrast, use those tokens in commands.
    if owner.suffix.lower() != ".py" and NON_O0.search(owner_text):
        return "owner contains an O1/O2/O3 execution path"
    return None


def _ordered_suites(suites: tuple[SuiteDefinition, ...]) -> tuple[SuiteDefinition, ...]:
    """Keep matrix order except for the explicit SH41 consumer chain."""

    ranks = {identifier: index for index, identifier in enumerate(SELFHOST_GENERATION_ORDER)}
    independent = [suite for suite in suites if suite.identifier not in ranks]
    generation = [suite for suite in suites if suite.identifier in ranks]
    generation.sort(key=lambda suite: ranks[suite.identifier])
    insertion = len(independent)
    for index, suite in enumerate(independent):
        if suite.component == "selfhost":
            insertion = index
            break
    return tuple(independent[:insertion] + generation + independent[insertion:])
