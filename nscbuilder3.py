#!/usr/bin/env python3
"""Single-file graphical orchestrator for the NativeSharp compiler suite.

NSC Builder 3 deliberately keeps all of its implementation in this file. It
reads the Phase 2 inventories without rewriting them, uses an externally built
NSC, and separates test-infrastructure construction from test execution.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
import math
import os
import queue
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
import uuid
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable, Sequence


APP_NAME = "NSC Builder 3"
SOURCE_ROOT = Path(__file__).resolve().parent
SUITE_ROOT = SOURCE_ROOT / "tests" / "compiler-suite"
INVENTORY_ROOT = SOURCE_ROOT / "docs" / "compiler-testing-reengineering" / "inventory"
STORAGE_ROOT = SOURCE_ROOT / "_nscbuilder3"
HISTORY_ROOT = STORAGE_ROOT / "history"
WORK_ROOT = STORAGE_ROOT / "work"
CONFIG_PATH = STORAGE_ROOT / "config.json"
MANIFEST_PATH = SUITE_ROOT / "manifest" / "cases.jsonl"
DESIGN_PATH = INVENTORY_ROOT / "test-case-design.csv"
ALLOCATION_PATH = INVENTORY_ROOT / "suite-case-allocation.csv"
WAVES_PATH = INVENTORY_ROOT / "implementation-waves.csv"
COMPONENTS_PATH = INVENTORY_ROOT / "components.csv"
CONTRACTS_PATH = INVENTORY_ROOT / "contracts.csv"
CONTROLLER_ROOT = SUITE_ROOT / "controller"
RUN_SUITE_PATH = CONTROLLER_ROOT / "run_suite.py"

PROFILE_RANK = {"PR": 0, "NIGHTLY": 1, "WEEKLY": 2}
TIMEOUT_SECONDS = {
    "tiny": 5.0,
    "short": 15.0,
    "normal": 30.0,
    "medium": 60.0,
    "extended": 180.0,
    "self-host": 900.0,
    "packaging": 900.0,
}
TERMINAL_FAILURES = {
    "FAIL",
    "UNEXPECTED_PASS",
    "BLOCKED",
    "INFRASTRUCTURE_ERROR",
    "CANCELLED",
}
RESULT_STATUSES = TERMINAL_FAILURES | {"PASS", "KNOWN_FAILURE"}
NON_EXECUTABLE = {
    "InfrastructurePending",
    "HistoricalDelegated",
    "PlanContradiction",
    "MetadataConflict",
    "UNCLASSIFIED",
}
DISPOSITION_VALUES = NON_EXECUTABLE | {"Executable", "KnownProductDefect"}
DISPOSITION_METADATA_ONLY_NAMES = frozenset(
    {
        "PackagingPlanContradictions.jsonl",
        "SelfHostBootstrapInfrastructurePlanContradictions.jsonl",
        "SelfHostContractsPlanContradictions.jsonl",
        "SelfHostProjectPlanContradictions.jsonl",
    }
)
INFERRED_DISPOSITIONS = {
    "SelfHostBootstrapInfrastructureCatalog.jsonl": "InfrastructurePending"
}
CATALOG_CASE_ID = re.compile(r"^[A-Z][A-Z0-9-]*-[0-9]{3,}$")
NINJA_PROGRESS_PATTERN = re.compile(r"^\[(\d+)/(\d+)\]\s*(.*)$")
CONFIG_STRING_FIELDS = (
    "nsc_executable",
    "nslink_executable",
    "compiler_build_root",
    "install_prefix",
    "build_root",
    "llvm_dir",
    "archive_executable",
    "comparison_nsc",
    "build_id_header",
)
WAVE_CATALOG_FIELDS = ("wave", "purpose", "component")
DESIGN_CATALOG_FIELDS = ("case_id", "contract_id", "component", "objective")
ALLOCATION_CATALOG_FIELDS = (
    "case_id",
    "component",
    "layer",
    "planned_executor",
    "default_profile",
    "supported_build_modes",
    "supported_optimizations",
    "isolation",
)
ALLOCATION_LAYERS = frozenset(
    {
        "unit",
        "component",
        "robustness",
        "integration",
        "process",
        "self-host",
        "build-contract",
        "packaging",
        "historical-weekly",
    }
)
ALLOCATION_EXECUTORS = frozenset(
    {"nsc_component_tests", "nsc_suite_controller+nsc", "legacy_historical_suite"}
)
ALLOCATION_ISOLATIONS = frozenset(
    {"RESET_PER_CASE", "FRESH_TEMP_PER_CASE", "LEGACY_OWNED"}
)
ALLOCATION_BUILD_MODES = frozenset({"Debug", "Release", "FROZEN_CONTRACT"})
ALLOCATION_OPTIMIZATIONS = frozenset(
    {"N/A", "O0", "O1", "O2", "O3", "FROZEN_CONTRACT"}
)


def utc_now() -> str:
    """Return an ISO-8601 timestamp used by durable evidence records."""
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def read_csv(path: Path) -> list[dict[str, str]]:
    """Read one canonical UTF-8 CSV file without mutating its source."""
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def read_catalog_csv(
    path: Path, required_fields: Sequence[str]
) -> list[dict[str, str]]:
    """Read a catalogue CSV while preserving extensible named columns."""
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        headers = reader.fieldnames
        if headers is None:
            raise ValueError(f"{path}: missing CSV header")
        if any(header is None or not header.strip() for header in headers):
            raise ValueError(f"{path}: CSV header contains an empty name")
        if len(headers) != len(set(headers)):
            raise ValueError(f"{path}: CSV header contains duplicate names")
        missing = [field for field in required_fields if field not in headers]
        if missing:
            raise ValueError(
                f"{path}: CSV header is missing required fields: {', '.join(missing)}"
            )
        rows: list[dict[str, str]] = []
        for row_number, row in enumerate(reader, start=2):
            if None in row:
                raise ValueError(f"{path}: row {row_number} contains extra cells")
            blank = [
                field
                for field in required_fields
                if row.get(field) is None or not row[field].strip()
            ]
            if blank:
                raise ValueError(
                    f"{path}: row {row_number} has blank required fields: "
                    + ", ".join(blank)
                )
            rows.append(row)
        return rows


def parse_wave_number(value: str) -> int:
    """Parse one canonical positive decimal implementation-wave number."""
    if re.fullmatch(r"[1-9][0-9]*", value) is None:
        raise ValueError(f"invalid implementation wave: {value!r}")
    return int(value)


def validate_allocation_domains(rows: Sequence[dict[str, str]]) -> None:
    """Reject allocation values that selection or expansion cannot represent."""
    scalar_domains = {
        "layer": ALLOCATION_LAYERS,
        "planned_executor": ALLOCATION_EXECUTORS,
        "default_profile": frozenset(PROFILE_RANK),
        "isolation": ALLOCATION_ISOLATIONS,
    }
    list_domains = {
        "supported_build_modes": (ALLOCATION_BUILD_MODES, {"FROZEN_CONTRACT"}),
        "supported_optimizations": (
            ALLOCATION_OPTIMIZATIONS,
            {"N/A", "FROZEN_CONTRACT"},
        ),
    }
    for row_number, row in enumerate(rows, start=2):
        case_id = row.get("case_id", f"row {row_number}")
        for field_name, allowed in scalar_domains.items():
            value = row[field_name]
            if value not in allowed:
                raise ValueError(
                    f"allocation {case_id} has unsupported {field_name}: {value!r}"
                )
        for field_name, (allowed, exclusive) in list_domains.items():
            values = split_semicolon(row[field_name])
            if not values or len(values) != len(set(values)):
                raise ValueError(
                    f"allocation {case_id} has invalid {field_name}: {row[field_name]!r}"
                )
            unknown = sorted(set(values) - allowed)
            if unknown:
                raise ValueError(
                    f"allocation {case_id} has unsupported {field_name}: "
                    + ", ".join(unknown)
                )
            if len(values) > 1 and set(values) & exclusive:
                raise ValueError(
                    f"allocation {case_id} mixes sentinel in {field_name}: "
                    f"{row[field_name]!r}"
                )


def expand_catalog_case_range(path: Path, value: Any) -> tuple[str, ...]:
    """Expand one canonical ascending case range without silent empty output."""
    if not isinstance(value, str) or value.count("..") != 1:
        raise ValueError(f"{path}: invalid case range: {value!r}")
    start, end = value.split("..", maxsplit=1)
    left = re.fullmatch(r"(.*?)([0-9]+)", start)
    right = re.fullmatch(r"(.*?)([0-9]+)", end)
    if (
        left is None
        or right is None
        or left.group(1) != right.group(1)
        or len(left.group(2)) != len(right.group(2))
        or CATALOG_CASE_ID.fullmatch(start) is None
        or CATALOG_CASE_ID.fullmatch(end) is None
    ):
        raise ValueError(f"{path}: invalid case range: {value!r}")
    first = int(left.group(2))
    last = int(right.group(2))
    if first > last:
        raise ValueError(f"{path}: descending case range: {value!r}")
    width = len(left.group(2))
    return tuple(
        f"{left.group(1)}{number:0{width}d}" for number in range(first, last + 1)
    )


def parse_disposition_row(
    path: Path, row: dict[str, Any]
) -> tuple[tuple[str, ...], str] | None:
    """Parse one case disposition row or one explicitly inventoried metadata row."""
    identity_fields = [
        field
        for field in ("case_id", "case_ids", "affected_case_ids", "case_range")
        if field in row
    ]
    if not identity_fields:
        if path.name in DISPOSITION_METADATA_ONLY_NAMES:
            return None
        raise ValueError(f"{path}: disposition row has no supported case identity")
    if len(identity_fields) != 1:
        raise ValueError(f"{path}: disposition row has ambiguous case identity")
    identity = identity_fields[0]
    value = row[identity]
    if identity == "case_id":
        if not isinstance(value, str) or CATALOG_CASE_ID.fullmatch(value) is None:
            raise ValueError(f"{path}: invalid case identity in case_id")
        case_ids = (value,)
    elif identity == "case_range":
        case_ids = expand_catalog_case_range(path, value)
    else:
        if (
            not isinstance(value, list)
            or not value
            or not all(
                isinstance(item, str) and CATALOG_CASE_ID.fullmatch(item) is not None
                for item in value
            )
            or len(value) != len(set(value))
        ):
            raise ValueError(f"{path}: invalid case identity in {identity}")
        case_ids = tuple(value)
    disposition = row.get("disposition")
    if disposition is None:
        disposition = INFERRED_DISPOSITIONS.get(path.name)
    if not isinstance(disposition, str) or disposition not in DISPOSITION_VALUES:
        raise ValueError(f"{path}: invalid disposition: {disposition!r}")
    return case_ids, disposition


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    """Read non-empty JSON Lines objects and retain their source order."""
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as stream:
        for number, line in enumerate(stream, start=1):
            if not line.strip():
                continue
            try:
                value = json.loads(line, object_pairs_hook=unique_json_object)
            except ValueError as error:
                raise ValueError(f"{path}:{number}: {error}") from error
            if not isinstance(value, dict):
                raise ValueError(f"{path}:{number}: expected a JSON object")
            rows.append(value)
    return rows


def unique_json_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    """Build one object without applying JSON's ambiguous last-key-wins rule."""
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def validate_executable_manifest(
    manifest_path: Path = MANIFEST_PATH,
    components_path: Path = COMPONENTS_PATH,
    contracts_path: Path = CONTRACTS_PATH,
) -> tuple[Any, ...]:
    """Validate the GUI catalogue with the controller's canonical schema.

    Builder 3 intentionally delegates this contract instead of maintaining a
    second permissive parser.  The controller directory is exposed only while
    importing its public package; no repository or process state is changed.
    """
    controller_path = str(CONTROLLER_ROOT)
    inserted = controller_path not in sys.path
    if inserted:
        sys.path.insert(0, controller_path)
    try:
        from nsc_suite.manifest import InventoryIndex, load_manifest

        inventory = InventoryIndex.from_csv(components_path, contracts_path)
        return load_manifest(manifest_path, inventory)
    finally:
        if inserted:
            sys.path.remove(controller_path)


def parse_builder_config_payload(value: Any) -> dict[str, Any]:
    """Validate persisted Builder 3 settings before constructing the dataclass."""
    if not isinstance(value, dict):
        raise ValueError("Builder 3 configuration must be a JSON object")
    allowed = set(CONFIG_STRING_FIELDS) | {"jobs"}
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise ValueError("Builder 3 configuration contains unknown fields: " + ", ".join(unknown))
    for field_name in CONFIG_STRING_FIELDS:
        if field_name in value and not isinstance(value[field_name], str):
            raise ValueError(f"Builder 3 configuration {field_name} must be a string")
    if "jobs" in value and type(value["jobs"]) is not int:
        raise ValueError("Builder 3 configuration jobs must be an integer")
    if "jobs" in value and not 1 <= value["jobs"] <= 256:
        raise ValueError("parallel jobs must be between 1 and 256")
    return dict(value)


def write_json(path: Path, value: object) -> None:
    """Publish one deterministic JSON artifact after creating its directory."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, indent=2, ensure_ascii=False, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def write_json_atomic(path: Path, value: object) -> None:
    """Durably replace one JSON file without exposing partial contents."""
    content = json.dumps(value, indent=2, ensure_ascii=False, sort_keys=True) + "\n"
    write_text_atomic(path, content)


def write_text_atomic(path: Path, content: str) -> None:
    """Durably replace one UTF-8 text file without exposing partial contents."""
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    try:
        with temporary.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def sha256_file(path: Path) -> str:
    """Hash a file iteratively so large executables do not require one allocation."""
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def split_semicolon(value: str) -> tuple[str, ...]:
    """Normalize semicolon-separated allocation fields into stable tuples."""
    return tuple(part.strip() for part in value.split(";") if part.strip())


def is_a_drive(path: Path) -> bool:
    """Return whether an absolute Windows path belongs to the authorized A drive."""
    return path.is_absolute() and path.drive.casefold() == "a:"


def require_a_path(path: Path, label: str, must_exist: bool = False) -> Path:
    """Validate an A-drive path so Builder 3 never writes or deploys to C."""
    resolved = path.resolve()
    if not is_a_drive(resolved):
        raise ValueError(f"{label} must be on A:, got {resolved}")
    if must_exist and not resolved.exists():
        raise ValueError(f"{label} does not exist: {resolved}")
    return resolved


def require_file_path(path: Path, label: str, require_a_drive: bool = False) -> Path:
    """Require an existing regular file, optionally constrained to the A drive."""
    resolved = path.resolve()
    if require_a_drive and not is_a_drive(resolved):
        raise ValueError(f"{label} must be on A:, got {resolved}")
    if not resolved.is_file():
        raise ValueError(f"{label} must be an existing file: {resolved}")
    return resolved


def require_directory_path(
    path: Path,
    label: str,
    *,
    require_a_drive: bool = False,
    must_exist: bool = False,
) -> Path:
    """Require a directory or a not-yet-created directory destination."""
    resolved = path.resolve()
    if require_a_drive and not is_a_drive(resolved):
        raise ValueError(f"{label} must be on A:, got {resolved}")
    if resolved.exists() and not resolved.is_dir():
        raise ValueError(f"{label} must be a directory: {resolved}")
    if must_exist and not resolved.is_dir():
        raise ValueError(f"{label} directory does not exist: {resolved}")
    return resolved


def current_revision() -> str:
    """Read the local revision for evidence without contacting a remote."""
    completed = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=SOURCE_ROOT,
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        check=False,
    )
    return completed.stdout.strip() if completed.returncode == 0 else "UNKNOWN"


def first_existing_file(candidates: Iterable[str | Path]) -> str:
    """Return the first file candidate without searching unrelated drives."""
    for candidate in candidates:
        if not candidate:
            continue
        path = Path(candidate).resolve()
        if path.is_file():
            return str(path)
    return ""


def first_existing_directory(candidates: Iterable[str | Path]) -> str:
    """Return the first directory candidate without searching unrelated drives."""
    for candidate in candidates:
        if not candidate:
            continue
        path = Path(candidate).resolve()
        if path.is_dir():
            return str(path)
    return ""


def discover_build_tools(llvm_dir: str) -> dict[str, str]:
    """Discover the fixed LLVM toolchain and PATH-owned CMake/Ninja tools."""
    llvm_roots: list[Path] = [Path("C:/clang-llvm")]
    if llvm_dir:
        configured = Path(llvm_dir).resolve()
        if len(configured.parents) >= 3:
            llvm_roots.insert(0, configured.parents[2])
    clang = first_existing_file(
        [*(root / "bin" / "clang-cl.exe" for root in llvm_roots), shutil.which("clang-cl") or ""]
    )
    if not clang:
        raise ValueError("clang-cl.exe was not found under C:/clang-llvm or on PATH")
    root = Path(clang).parent.parent
    tools = {
        "cmake": shutil.which("cmake") or "",
        "ninja": shutil.which("ninja") or "",
        "clang_cl": clang,
        "lld_link": first_existing_file([root / "bin" / "lld-link.exe"]),
        "llvm_mt": first_existing_file([root / "bin" / "llvm-mt.exe"]),
        "llvm_rc": first_existing_file([root / "bin" / "llvm-rc.exe"]),
        "llvm_dir": first_existing_directory(
            [llvm_dir, root / "lib" / "cmake" / "llvm"]
        ),
    }
    missing = [name for name, value in tools.items() if not value]
    if missing:
        raise ValueError("required build tools were not found: " + ", ".join(missing))
    return tools


@dataclass(frozen=True)
class BuilderConfig:
    """Persist user-selected tools and output roots without implicit deployment."""

    nsc_executable: str = "A:/native-sharp-3.0-baseline/bin/nsc.exe"
    nslink_executable: str = "A:/native-sharp-3.0-baseline/bin/nslink.exe"
    compiler_build_root: str = "A:/nsc-devel/build-clang"
    install_prefix: str = "A:/native-sharp-3.0-baseline"
    build_root: str = "A:/nsc-devel/build-compiler-suite-v3"
    llvm_dir: str = ""
    archive_executable: str = ""
    comparison_nsc: str = ""
    build_id_header: str = ""
    jobs: int = max(1, min(32, os.cpu_count() or 1))

    @classmethod
    def load(cls) -> "BuilderConfig":
        """Load saved overrides and repair missing tools with known local locations."""
        if CONFIG_PATH.is_file():
            value = json.loads(
                CONFIG_PATH.read_text(encoding="utf-8"),
                object_pairs_hook=unique_json_object,
            )
            base = cls(**parse_builder_config_payload(value))
        else:
            base = cls()
        return base.with_discovery()

    def with_discovery(self) -> "BuilderConfig":
        """Fill absent or stale tool paths while preserving valid user overrides."""
        nsc = first_existing_file(
            [
                self.nsc_executable,
                "A:/native-sharp-3.0-baseline/bin/nsc.exe",
                "A:/nsc-devel/build-clang/bin/nsc.exe",
                "A:/nsc-devel/build-clang/nsc.exe",
            ]
        ) or self.nsc_executable
        nslink = first_existing_file(
            [
                Path(nsc).parent / "nslink.exe",
                self.nslink_executable,
                "A:/nslink-devel/bin/release/nslink.exe",
                "A:/native-sharp-3.0-baseline/bin/nslink.exe",
            ]
        ) or self.nslink_executable
        llvm_dir = first_existing_directory(
            [self.llvm_dir, os.environ.get("LLVM_DIR", ""), "C:/clang-llvm/lib/cmake/llvm"]
        )
        archive = first_existing_file(
            [self.archive_executable, shutil.which("7z") or "", shutil.which("7zz") or ""]
        )
        compiler_build_root = self.compiler_build_root
        if Path(compiler_build_root).name.casefold() == "build-clang-v3":
            compiler_build_root = "A:/nsc-devel/build-clang"
        return BuilderConfig(
            nsc_executable=nsc,
            nslink_executable=nslink,
            compiler_build_root=compiler_build_root,
            install_prefix=self.install_prefix,
            build_root=self.build_root,
            llvm_dir=llvm_dir,
            archive_executable=archive,
            comparison_nsc=self.comparison_nsc,
            build_id_header=self.build_id_header,
            jobs=self.jobs,
        )

    def save(self) -> None:
        """Persist settings under the repository-owned Builder 3 directory."""
        payload = parse_builder_config_payload(asdict(self))
        self.validated(require_tools=False)
        write_json_atomic(CONFIG_PATH, payload)

    def validated(self, require_tools: bool = False) -> "BuilderConfig":
        """Validate writable roots and, when requested, external tool receipts."""
        require_directory_path(
            Path(self.build_root), "test build root", require_a_drive=True
        )
        require_directory_path(
            Path(self.compiler_build_root), "compiler build root", require_a_drive=True
        )
        require_directory_path(
            Path(self.install_prefix), "compiler install prefix", require_a_drive=True
        )
        require_directory_path(
            STORAGE_ROOT, "Builder 3 storage root", require_a_drive=True
        )
        if not 1 <= self.jobs <= 256:
            raise ValueError("parallel jobs must be between 1 and 256")
        if require_tools:
            require_file_path(
                Path(self.nsc_executable), "NSC executable", require_a_drive=True
            )
            require_file_path(
                Path(self.nslink_executable), "NSLink executable", require_a_drive=True
            )
            if self.archive_executable:
                require_file_path(Path(self.archive_executable), "archive executable")
            if self.comparison_nsc:
                require_file_path(Path(self.comparison_nsc), "comparison NSC executable")
            if self.build_id_header:
                require_file_path(Path(self.build_id_header), "build-id header")
            if self.llvm_dir:
                require_directory_path(
                    Path(self.llvm_dir), "LLVM_DIR", must_exist=True
                )
        return self


@dataclass(frozen=True)
class CaseRecord:
    """Canonical projection of one Phase 2 case across all source inventories."""

    case_id: str
    contract_id: str
    component: str
    wave: int
    purpose: str
    layer: str
    profile: str
    build_modes: tuple[str, ...]
    optimizations: tuple[str, ...]
    executor: str
    disposition: str
    objective: str
    expected_outcome: str
    manifest: dict[str, Any] | None = field(compare=False, repr=False)

    @property
    def executable(self) -> bool:
        """Report whether the central executable manifest owns this case."""
        return self.manifest is not None and self.disposition not in NON_EXECUTABLE


@dataclass(frozen=True)
class RunVariant:
    """One concrete case execution after build-mode and optimization expansion."""

    case: CaseRecord
    build_mode: str
    optimization: str

    @property
    def key(self) -> str:
        """Return the stable GUI/evidence identity for this matrix cell."""
        return f"{self.case.case_id}|{self.build_mode}|{self.optimization}"


@dataclass
class CaseOutcome:
    """Durable result of one concrete case execution."""

    key: str
    case_id: str
    contract_id: str
    component: str
    wave: int
    build_mode: str
    optimization: str
    status: str
    duration_seconds: float = 0.0
    detail: str = ""
    evidence: str = ""


class CanonicalCatalog:
    """Join Phase 2 inventories without hiding non-executable dispositions."""

    def __init__(self, cases: Sequence[CaseRecord]) -> None:
        self.cases = tuple(cases)
        self.by_id = {case.case_id: case for case in self.cases}
        self.components = tuple(sorted({case.component for case in self.cases}))
        self.contracts = tuple(sorted({case.contract_id for case in self.cases}))
        self.waves = tuple(sorted({case.wave for case in self.cases}))

    @classmethod
    def load(cls) -> "CanonicalCatalog":
        """Load and validate the complete compiler-only denominator."""
        validated_manifests = validate_executable_manifest()
        validated_by_id = {case.case_id: case for case in validated_manifests}
        waves = read_catalog_csv(WAVES_PATH, WAVE_CATALOG_FIELDS)
        wave_by_component = {
            row["component"]: parse_wave_number(row["wave"]) for row in waves
        }
        purpose_by_component = {row["component"]: row["purpose"] for row in waves}
        design = [
            row
            for row in read_catalog_csv(DESIGN_PATH, DESIGN_CATALOG_FIELDS)
            if row["component"] in wave_by_component
        ]
        allocation_rows = [
            row
            for row in read_catalog_csv(ALLOCATION_PATH, ALLOCATION_CATALOG_FIELDS)
            if row["component"] in wave_by_component
        ]
        validate_allocation_domains(allocation_rows)
        manifest_rows = read_jsonl(MANIFEST_PATH)
        dispositions = cls._load_dispositions()
        cls._validate_sources(design, allocation_rows, manifest_rows, waves)
        foreign_dispositions = sorted(
            set(dispositions) - {row["case_id"] for row in design}
        )
        if foreign_dispositions:
            raise ValueError(
                f"disposition catalogues contain {len(foreign_dispositions)} foreign cases"
            )
        allocation = {row["case_id"]: row for row in allocation_rows}
        manifest = {row["case_id"]: row for row in manifest_rows}
        cases: list[CaseRecord] = []
        for row in design:
            case_id = row["case_id"]
            assigned = allocation[case_id]
            executable = manifest.get(case_id)
            canonical = validated_by_id.get(case_id)
            disposition = dispositions.get(case_id, "Executable" if executable else "InfrastructurePending")
            expected = ""
            if canonical is not None:
                expected = canonical.expected_outcome
                if expected == "KNOWN_FAILURE":
                    disposition = "KnownProductDefect"
            cases.append(
                CaseRecord(
                    case_id=case_id,
                    contract_id=row["contract_id"],
                    component=row["component"],
                    wave=wave_by_component[row["component"]],
                    purpose=purpose_by_component[row["component"]],
                    layer=canonical.layer if canonical is not None else assigned["layer"],
                    profile=canonical.profile
                    if canonical is not None
                    else assigned["default_profile"],
                    build_modes=canonical.build_modes
                    if canonical is not None
                    else split_semicolon(assigned["supported_build_modes"]),
                    optimizations=canonical.optimizations
                    if canonical is not None
                    else cls._optimizations(assigned),
                    executor=canonical.executor
                    if canonical is not None
                    else assigned["planned_executor"],
                    disposition=disposition,
                    objective=row["objective"],
                    expected_outcome=expected,
                    manifest=executable,
                )
            )
        return cls(cases)

    @staticmethod
    def _optimizations(allocation: dict[str, str]) -> tuple[str, ...]:
        """Normalize non-executable allocation optimizations for display."""
        values = split_semicolon(allocation["supported_optimizations"])
        normalized = tuple("NA" if value in {"N/A", ""} else value for value in values)
        return normalized or ("NA",)

    @staticmethod
    def _validate_sources(
        design: Sequence[dict[str, str]],
        allocation: Sequence[dict[str, str]],
        manifest: Sequence[dict[str, Any]],
        waves: Sequence[dict[str, str]],
    ) -> None:
        """Reject relational omissions without freezing revision-specific totals."""
        wave_components = [row["component"] for row in waves]
        design_ids = [row["case_id"] for row in design]
        allocation_ids = [row["case_id"] for row in allocation]
        manifest_ids = [row["case_id"] for row in manifest]
        if not wave_components:
            raise ValueError("implementation waves do not contain compiler components")
        if len(wave_components) != len(set(wave_components)):
            raise ValueError("implementation-waves.csv contains duplicate compiler components")
        if not design_ids:
            raise ValueError("test-case-design.csv does not contain compiler cases")
        if len(design_ids) != len(set(design_ids)):
            raise ValueError("test-case-design.csv contains duplicate compiler-suite IDs")
        if len(allocation_ids) != len(set(allocation_ids)):
            raise ValueError("suite-case-allocation.csv contains duplicate compiler-suite IDs")
        if len(manifest_ids) != len(set(manifest_ids)):
            raise ValueError("manifest contains duplicate executable compiler-suite IDs")
        design_set = set(design_ids)
        allocation_set = set(allocation_ids)
        missing_allocation = sorted(design_set - allocation_set)
        if missing_allocation:
            raise ValueError(f"allocation is missing {len(missing_allocation)} compiler cases")
        foreign_allocation = sorted(allocation_set - design_set)
        if foreign_allocation:
            raise ValueError(f"allocation contains {len(foreign_allocation)} foreign compiler cases")
        design_components = {row["component"] for row in design}
        missing_components = sorted(set(wave_components) - design_components)
        if missing_components:
            raise ValueError(
                f"design is missing cases for {len(missing_components)} compiler components"
            )
        design_component_by_id = {row["case_id"]: row["component"] for row in design}
        mismatched_components = [
            row["case_id"]
            for row in allocation
            if design_component_by_id.get(row["case_id"]) != row["component"]
        ]
        if mismatched_components:
            raise ValueError(
                f"allocation disagrees with design for {len(mismatched_components)} compiler cases"
            )
        foreign_manifest = sorted(set(manifest_ids) - design_set)
        if foreign_manifest:
            raise ValueError(f"manifest contains {len(foreign_manifest)} non-canonical cases")

    @staticmethod
    def _load_dispositions() -> dict[str, str]:
        """Collect explicit case dispositions from all Phase 2 JSONL catalogues."""
        dispositions: dict[str, str] = {}
        for path in sorted(SUITE_ROOT.rglob("*.jsonl")):
            if path == MANIFEST_PATH or path.parent.name == "infrastructure":
                continue
            for row in read_jsonl(path):
                parsed = parse_disposition_row(path, row)
                if parsed is None:
                    continue
                case_ids, disposition = parsed
                for case_id in case_ids:
                    previous = dispositions.get(case_id)
                    if previous and previous != disposition:
                        if previous == "Executable":
                            dispositions[case_id] = disposition
                        elif disposition != "Executable":
                            raise ValueError(
                                f"conflicting dispositions for {case_id}: "
                                f"{previous} and {disposition}"
                            )
                    elif not previous:
                        dispositions[case_id] = disposition
        return dispositions

    def summary(self) -> dict[str, Any]:
        """Return deterministic catalogue counts for the audit mode and GUI."""
        dispositions = Counter(case.disposition for case in self.cases)
        return {
            "cases": len(self.cases),
            "components": len(self.components),
            "contracts": len(self.contracts),
            "waves": list(self.waves),
            "executable": sum(case.executable for case in self.cases),
            "dispositions": dict(sorted(dispositions.items())),
        }

    def select(
        self,
        scope: str,
        value: str,
        profile: str,
    ) -> tuple[CaseRecord, ...]:
        """Select canonical rows without converting pending rows into executable cases."""
        selected: list[CaseRecord] = []
        for case in self.cases:
            if scope == "Ola" and str(case.wave) != value:
                continue
            if scope == "Componente" and case.component != value:
                continue
            if scope == "Contrato" and case.contract_id != value:
                continue
            if scope == "Caso" and case.case_id != value:
                continue
            if profile != "Todos" and PROFILE_RANK.get(case.profile, 99) > PROFILE_RANK[profile]:
                continue
            selected.append(case)
        return tuple(selected)


def expand_variants(
    cases: Iterable[CaseRecord], build_mode: str, optimization: str
) -> tuple[RunVariant, ...]:
    """Expand selected executable cases into their supported configuration matrix."""
    variants: list[RunVariant] = []
    requested_modes = ("Debug", "Release") if build_mode == "Todos" else (build_mode,)
    requested_opts = ("O0", "O1", "O2", "O3") if optimization == "Todas" else (optimization,)
    for case in cases:
        if not case.executable:
            continue
        for mode in requested_modes:
            if mode not in case.build_modes:
                continue
            supported = case.optimizations
            effective = ("NA",) if supported == ("NA",) else requested_opts
            for opt in effective:
                if opt in supported:
                    variants.append(RunVariant(case, mode, opt))
    return tuple(variants)


def infrastructure_command_outcome(
    *,
    index: int,
    label: str,
    exit_code: int,
    duration_seconds: float,
    cancelled: bool,
    evidence: str,
) -> CaseOutcome:
    """Project one infrastructure command into the shared terminal protocol."""
    status = "CANCELLED" if cancelled else ("PASS" if exit_code == 0 else "FAIL")
    if cancelled:
        detail = "operation cancelled"
    elif exit_code != 0:
        detail = f"exit code {exit_code}"
    else:
        detail = ""
    return CaseOutcome(
        key=f"INFRA|{index}",
        case_id=label,
        contract_id="NSCBUILDER3-INFRASTRUCTURE",
        component="TestInfrastructure",
        wave=0,
        build_mode="NA",
        optimization="NA",
        status=status,
        duration_seconds=duration_seconds,
        detail=detail,
        evidence=evidence,
    )


def cancelled_infrastructure_outcomes(
    commands: Sequence[tuple[str, Sequence[str], Path]],
    first_unstarted_index: int,
    evidence: str,
) -> tuple[CaseOutcome, ...]:
    """Close every unstarted command after an infrastructure cancellation."""
    if not 1 <= first_unstarted_index <= len(commands) + 1:
        raise ValueError("first unstarted command index is outside the command plan")
    outcomes: list[CaseOutcome] = []
    for index in range(first_unstarted_index, len(commands) + 1):
        label = commands[index - 1][0]
        outcome = infrastructure_command_outcome(
            index=index,
            label=label,
            exit_code=130,
            duration_seconds=0.0,
            cancelled=True,
            evidence=evidence,
        )
        outcome.detail = "not started because operation was cancelled"
        outcomes.append(outcome)
    return tuple(outcomes)


def retry_variants(
    visible: Sequence[RunVariant], outcomes: Sequence[CaseOutcome]
) -> tuple[RunVariant, ...]:
    """Project terminal failures back to the exact visible plan deterministically."""
    by_key = {variant.key: variant for variant in visible}
    if len(by_key) != len(visible):
        raise ValueError("visible retry plan contains duplicate variant keys")
    status_by_key: dict[str, str] = {}
    for outcome in outcomes:
        if outcome.key in status_by_key:
            raise ValueError(f"duplicate outcome key in retry projection: {outcome.key}")
        status_by_key[outcome.key] = outcome.status
    terminal_keys = {
        key for key, status in status_by_key.items() if status in TERMINAL_FAILURES
    }
    unknown = sorted(terminal_keys - set(by_key))
    if unknown:
        raise ValueError("terminal outcomes are outside the visible plan: " + ", ".join(unknown))
    return tuple(variant for variant in visible if variant.key in terminal_keys)


class EvidenceStore:
    """Write crash-surviving run evidence and final human/machine reports."""

    def __init__(self, run_id: str, metadata: dict[str, Any]) -> None:
        self.run_id = run_id
        self.expected_variant_count = metadata.get("variant_count")
        self.root = HISTORY_ROOT / run_id
        self.root.mkdir(parents=True, exist_ok=False)
        self.events_path = self.root / "events.jsonl"
        self.log_path = self.root / "full.log"
        self._event_stream = self.events_path.open("a", encoding="utf-8", newline="\n")
        self._log_stream = self.log_path.open("a", encoding="utf-8", newline="\n")
        write_json_atomic(self.root / "metadata.json", metadata)
        self.event("RUN_STARTED", metadata)

    def event(self, kind: str, payload: dict[str, Any]) -> None:
        """Append and flush one structured event so crashes retain progress."""
        record = {"schema": 1, "timestamp": utc_now(), "kind": kind, "payload": payload}
        self._event_stream.write(json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n")
        self._event_stream.flush()

    def log(self, line: str) -> None:
        """Append progressive textual output without delaying visibility."""
        self._log_stream.write(line)
        if not line.endswith("\n"):
            self._log_stream.write("\n")
        self._log_stream.flush()

    def finish(self, outcomes: Sequence[CaseOutcome], cancelled: bool) -> None:
        """Publish final results and a concise Markdown report."""
        keys = [outcome.key for outcome in outcomes]
        if len(keys) != len(set(keys)):
            raise ValueError("duplicate terminal result keys")
        expected = self.expected_variant_count
        if expected is not None:
            if isinstance(expected, bool) or not isinstance(expected, int) or expected < 1:
                raise ValueError(f"invalid metadata variant_count: {expected!r}")
            if len(outcomes) != expected:
                raise ValueError(
                    f"terminal result cardinality {len(outcomes)} does not match "
                    f"variant_count {expected}"
                )
        counts = Counter(outcome.status for outcome in outcomes)
        overall = self._overall_status(counts)
        status = "CANCELLED" if cancelled else overall
        payload = {
            "run_id": self.run_id,
            "status": status,
            "finished_at": utc_now(),
            "counts": dict(sorted(counts.items())),
            "results": [asdict(outcome) for outcome in outcomes],
        }
        write_json_atomic(self.root / "results.json", payload)
        lines = [
            f"# NSC Builder 3 — {self.run_id}",
            "",
            f"Estado: `{status}`",
            "",
            "| Estado | Cantidad |",
            "|---|---:|",
        ]
        lines.extend(f"| `{name}` | {count} |" for name, count in sorted(counts.items()))
        failures = [item for item in outcomes if item.status in TERMINAL_FAILURES]
        lines.extend(["", "## Fallos", ""])
        if failures:
            lines.extend(
                f"- `{item.key}`: {item.detail or item.status}" for item in failures
            )
        else:
            lines.append("No se registraron fallos terminales en los casos ejecutados.")
        write_text_atomic(self.root / "summary.md", "\n".join(lines) + "\n")
        self.event("RUN_FINISHED", {"status": status, "counts": dict(counts)})
        self.close()

    @staticmethod
    def _overall_status(counts: Counter[str]) -> str:
        """Keep known defects visible while distinguishing hard failures."""
        if sum(counts.values()) <= 0:
            raise ValueError("cannot publish evidence with no case results")
        unknown = sorted(set(counts) - RESULT_STATUSES)
        if unknown:
            raise ValueError("unknown result states: " + ", ".join(unknown))
        if any(counts[name] for name in TERMINAL_FAILURES):
            return "FAILED"
        if counts["KNOWN_FAILURE"]:
            return "COMPLETED_WITH_KNOWN_DEFECTS"
        return "PASSED"

    def close(self) -> None:
        """Close evidence streams exactly once."""
        if not self._event_stream.closed:
            self._event_stream.close()
        if not self._log_stream.closed:
            self._log_stream.close()


def history_record(root: Path) -> tuple[str, str, str, str, str]:
    """Validate one durable run before projecting it into the history table."""
    run_id = root.name
    fallback = (run_id, "incomplete", "INCOMPLETE", "", str(root))
    metadata_path = root / "metadata.json"
    results_path = root / "results.json"
    summary_path = root / "summary.md"
    events_path = root / "events.jsonl"
    if not metadata_path.is_file():
        return fallback
    try:
        metadata = json.loads(
            metadata_path.read_text(encoding="utf-8"),
            object_pairs_hook=unique_json_object,
        )
        if not isinstance(metadata, dict):
            raise ValueError("metadata must be an object")
        kind = metadata.get("kind")
        if not isinstance(kind, str) or not kind:
            raise ValueError("metadata kind is missing")
        if metadata.get("run_id") != run_id:
            raise ValueError("metadata run_id does not match its directory")
    except (OSError, json.JSONDecodeError, ValueError, TypeError):
        return (run_id, "malformed", "MALFORMED", "", str(root))
    incomplete = (run_id, kind, "INCOMPLETE", "", str(root))
    if not results_path.is_file() or not summary_path.is_file() or not events_path.is_file():
        return incomplete
    try:
        payload = json.loads(
            results_path.read_text(encoding="utf-8"),
            object_pairs_hook=unique_json_object,
        )
        if not isinstance(payload, dict):
            raise ValueError("results must be an object")
        if payload.get("run_id") != run_id:
            raise ValueError("results run_id does not match its directory")
        finished_at = payload.get("finished_at")
        if not isinstance(finished_at, str) or not finished_at:
            raise ValueError("finished_at is missing")
        results = payload.get("results")
        if not isinstance(results, list) or not results:
            raise ValueError("terminal results must be a non-empty list")
        if any(not isinstance(item, dict) for item in results):
            raise ValueError("each terminal result must be an object")
        keys = [item.get("key") for item in results]
        if any(not isinstance(key, str) or not key for key in keys):
            raise ValueError("each terminal result must have a key")
        if len(keys) != len(set(keys)):
            raise ValueError("terminal result keys must be unique")
        statuses = [item.get("status") for item in results]
        if any(status not in RESULT_STATUSES for status in statuses):
            raise ValueError("terminal result contains an unknown status")
        observed = Counter(statuses)
        raw_counts = payload.get("counts")
        if not isinstance(raw_counts, dict):
            raise ValueError("result counts must be an object")
        if any(
            not isinstance(name, str)
            or name not in RESULT_STATUSES
            or isinstance(count, bool)
            or not isinstance(count, int)
            or count <= 0
            for name, count in raw_counts.items()
        ):
            raise ValueError("result counts contain an invalid entry")
        counts = Counter(raw_counts)
        if counts != observed:
            raise ValueError("result counts do not match terminal rows")
        expected_count = metadata.get("variant_count")
        if expected_count is not None:
            if (
                isinstance(expected_count, bool)
                or not isinstance(expected_count, int)
                or expected_count < 1
                or expected_count != len(results)
            ):
                raise ValueError("metadata variant_count does not match terminal rows")
        status = payload.get("status")
        expected_status = EvidenceStore._overall_status(counts)
        if status == "CANCELLED":
            if counts["CANCELLED"] <= 0:
                raise ValueError("cancelled run has no cancelled result")
        elif status != expected_status:
            raise ValueError("overall status does not match terminal rows")
        events = read_jsonl(events_path)
        if not events:
            raise ValueError("event stream is empty")
        final_event = events[-1]
        final_payload = final_event.get("payload")
        if final_event.get("kind") != "RUN_FINISHED" or not isinstance(final_payload, dict):
            raise ValueError("event stream lacks a final RUN_FINISHED event")
        if final_payload.get("status") != status or Counter(final_payload.get("counts", {})) != counts:
            raise ValueError("final event does not match results")
    except (OSError, json.JSONDecodeError, ValueError, TypeError):
        return (run_id, kind, "MALFORMED", "", str(root))
    return (run_id, kind, str(status), finished_at, str(root))


class ProcessController:
    """Run one child process with streamed output and scoped tree cancellation."""

    def __init__(self, output: Callable[[str], None]) -> None:
        self.output = output
        self.cancelled = threading.Event()
        self.last_termination_reason: str | None = None
        self._lock = threading.Lock()
        self._process: subprocess.Popen[str] | None = None

    def run(
        self,
        command: Sequence[str],
        cwd: Path,
        environment: dict[str, str] | None = None,
        timeout: float | None = None,
    ) -> tuple[int, str]:
        """Stream merged output and return exit code plus captured text."""
        self.last_termination_reason = None
        if self.cancelled.is_set():
            self.last_termination_reason = "cancelled"
            message = "cancelled before launch\n"
            self.output(message)
            return 130, message
        flags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
        process = subprocess.Popen(
            [str(part) for part in command],
            cwd=cwd,
            env=environment,
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            creationflags=flags,
        )
        with self._lock:
            self._process = process
        output: list[str] = []
        pending: queue.Queue[str | None] = queue.Queue()
        started = time.monotonic()
        assert process.stdout is not None

        def read_output() -> None:
            """Drain the child pipe without blocking cancellation or timeout checks."""
            for line in process.stdout:
                pending.put(line)
            pending.put(None)

        reader = threading.Thread(target=read_output, name="nscbuilder3-output", daemon=True)
        reader.start()
        stream_closed = False
        while True:
            try:
                line = pending.get(timeout=0.1)
                if line is None:
                    stream_closed = True
                else:
                    output.append(line)
                    self.output(line)
            except queue.Empty:
                pass
            if process.poll() is not None and stream_closed and pending.empty():
                break
            if timeout is not None and time.monotonic() - started > timeout:
                self.last_termination_reason = "timeout"
                self._terminate_tree(process)
                message = f"process timed out after {timeout:.1f} seconds\n"
                output.append(message)
                self.output(message)
                break
            if self.cancelled.is_set():
                self.last_termination_reason = "cancelled"
                self._terminate_tree(process)
                message = "process cancelled\n"
                output.append(message)
                self.output(message)
                break
        reader.join(timeout=1.0)
        while True:
            try:
                line = pending.get_nowait()
            except queue.Empty:
                break
            if line is not None:
                output.append(line)
                self.output(line)
        with self._lock:
            self._process = None
        return process.returncode if process.returncode is not None else 130, "".join(output)

    def run_completed(
        self,
        command: Sequence[str],
        cwd: Path,
        environment: dict[str, str] | None = None,
        timeout: float | None = None,
    ) -> subprocess.CompletedProcess[str]:
        """Run one owned child while preserving stdout and stderr separately."""
        self.last_termination_reason = None
        arguments = [str(part) for part in command]
        if self.cancelled.is_set():
            self.last_termination_reason = "cancelled"
            message = "cancelled before launch\n"
            self.output(message)
            return subprocess.CompletedProcess(arguments, 130, "", message)
        flags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
        process = subprocess.Popen(
            arguments,
            cwd=cwd,
            env=environment,
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            creationflags=flags,
        )
        with self._lock:
            self._process = process
        pending: queue.Queue[tuple[str, str | None]] = queue.Queue()
        captured = {"stdout": [], "stderr": []}
        started = time.monotonic()
        assert process.stdout is not None
        assert process.stderr is not None

        def read_channel(name: str, stream: Iterable[str]) -> None:
            """Drain one child channel and identify its end in the shared queue."""
            for line in stream:
                pending.put((name, line))
            pending.put((name, None))

        readers = [
            threading.Thread(
                target=read_channel,
                args=("stdout", process.stdout),
                name="nscbuilder3-stdout",
                daemon=True,
            ),
            threading.Thread(
                target=read_channel,
                args=("stderr", process.stderr),
                name="nscbuilder3-stderr",
                daemon=True,
            ),
        ]
        for reader in readers:
            reader.start()
        closed: set[str] = set()
        while True:
            try:
                channel, line = pending.get(timeout=0.1)
                if line is None:
                    closed.add(channel)
                else:
                    captured[channel].append(line)
                    self.output(line)
            except queue.Empty:
                pass
            if process.poll() is not None and len(closed) == 2 and pending.empty():
                break
            if timeout is not None and time.monotonic() - started > timeout:
                self.last_termination_reason = "timeout"
                self._terminate_tree(process)
                message = f"process timed out after {timeout:.1f} seconds\n"
                captured["stderr"].append(message)
                self.output(message)
                break
            if self.cancelled.is_set():
                self.last_termination_reason = "cancelled"
                self._terminate_tree(process)
                message = "process cancelled\n"
                captured["stderr"].append(message)
                self.output(message)
                break
        for reader in readers:
            reader.join(timeout=1.0)
        while True:
            try:
                channel, line = pending.get_nowait()
            except queue.Empty:
                break
            if line is not None:
                captured[channel].append(line)
                self.output(line)
        with self._lock:
            self._process = None
        return subprocess.CompletedProcess(
            arguments,
            process.returncode if process.returncode is not None else 130,
            "".join(captured["stdout"]),
            "".join(captured["stderr"]),
        )

    def cancel(self) -> None:
        """Cancel only the active child process tree owned by this controller."""
        self.last_termination_reason = "cancelled"
        self.cancelled.set()
        with self._lock:
            process = self._process
        if process is not None:
            self._terminate_tree(process)

    @staticmethod
    def _terminate_tree(process: subprocess.Popen[str]) -> None:
        """Terminate a known child tree without enumerating unrelated processes."""
        if process.poll() is not None:
            return
        if os.name == "nt":
            completed = subprocess.run(
                ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                capture_output=True,
                check=False,
            )
            if completed.returncode != 0 and process.poll() is None:
                process.kill()
        else:
            os.killpg(os.getpgid(process.pid), signal.SIGTERM)
        try:
            process.wait(timeout=5.0)
        except subprocess.TimeoutExpired:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=5.0)


def effective_build_dir(config: BuilderConfig, mode: str) -> Path:
    """Keep Debug and Release test infrastructure in independent trees."""
    return Path(config.build_root).resolve() / mode.casefold()


def compiler_build_dir(config: BuilderConfig, mode: str) -> Path:
    """Keep Debug and Release compiler products in independent build trees."""
    release = Path(config.compiler_build_root).resolve()
    if mode == "Release":
        return release
    return release.parent / f"{release.name}-debug"


def compiler_output(config: BuilderConfig, mode: str) -> Path:
    """Return the NSC artifact produced by one compiler configuration."""
    return compiler_build_dir(config, mode) / "bin" / "nsc.exe"


def host_executable(config: BuilderConfig, mode: str) -> Path:
    """Locate the consolidated host executable in supported CMake layouts."""
    root = effective_build_dir(config, mode)
    candidates = (
        root / "nsc_component_tests.exe",
        root / mode / "nsc_component_tests.exe",
        root / "bin" / "nsc_component_tests.exe",
        root / "bin" / mode / "nsc_component_tests.exe",
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return candidates[0]


def parse_host_envelope(
    *,
    captured: str,
    exit_code: int,
    case_id: str,
    contract_id: str,
    component: str,
) -> tuple[bool, str]:
    """Validate the exact single-case protocol emitted by the consolidated host."""
    lines = [line for line in captured.splitlines() if line.strip()]
    if len(lines) != 1:
        raise ValueError(f"host must emit exactly one JSON object, got {len(lines)}")
    payload = json.loads(lines[0], object_pairs_hook=unique_json_object)
    if not isinstance(payload, dict):
        raise ValueError("host result must be a JSON object")
    expected_identity = (case_id, contract_id, component)
    actual_identity = (
        payload.get("case_id"),
        payload.get("contract_id"),
        payload.get("component"),
    )
    if actual_identity != expected_identity:
        raise ValueError("host result identity mismatch")
    duration = payload.get("duration_us")
    if isinstance(duration, bool) or not isinstance(duration, int) or duration < 0:
        raise ValueError("host duration_us must be a non-negative integer")
    failures = payload.get("failures")
    if not isinstance(failures, list) or any(
        not isinstance(item, str) or not item for item in failures
    ):
        raise ValueError("host failures must be a list of strings")
    status = payload.get("status")
    if status not in {"PASS", "FAIL"}:
        raise ValueError(f"host status is invalid: {status!r}")
    if status == "PASS" and failures:
        raise ValueError("host PASS result cannot contain failures")
    if status == "FAIL" and not failures:
        raise ValueError("host FAIL result requires failure evidence")
    expected_exit = 0 if status == "PASS" else 1
    if exit_code != expected_exit:
        raise ValueError(
            f"host exit code {exit_code} contradicts status {status}; expected {expected_exit}"
        )
    return status == "PASS", "; ".join(failures)


def parse_identity_envelope(
    *,
    content: str,
    exit_code: int,
    run_id: str,
    revision: str,
    build_mode: str,
    optimization: str,
    case_id: str,
    expected_tools: Sequence[tuple[str, str, str]],
) -> tuple[str, str]:
    """Validate one canonical focused report produced by ``run_suite.py``."""
    payload = json.loads(content, object_pairs_hook=unique_json_object)
    if not isinstance(payload, dict):
        raise ValueError("identity report must be a JSON object")
    if type(payload.get("schema")) is not int or payload.get("schema") != 1:
        raise ValueError("identity report schema must be 1")

    run = payload.get("run")
    if not isinstance(run, dict):
        raise ValueError("identity report run must be a JSON object")
    expected_identity = (run_id, revision, build_mode, optimization)
    actual_identity = (
        run.get("run_id"),
        run.get("revision"),
        run.get("build_mode"),
        run.get("optimization"),
    )
    if actual_identity != expected_identity:
        raise ValueError("identity report run identity mismatch")
    if (
        run.get("profile") != "explicit"
        or run.get("scope") != "focused"
        or type(run.get("selected")) is not int
        or run.get("selected") != 1
    ):
        raise ValueError("identity report is not a focused single-case run")
    tools = run.get("tools")
    if not isinstance(tools, list):
        raise ValueError("identity report tools must be a list")
    actual_tools: list[tuple[str, str, str]] = []
    for tool in tools:
        if not isinstance(tool, dict) or set(tool) != {"name", "path", "sha256"}:
            raise ValueError("identity report tool receipt schema is invalid")
        identity = (tool.get("name"), tool.get("path"), tool.get("sha256"))
        if any(not isinstance(value, str) or not value for value in identity):
            raise ValueError("identity report tool receipt fields must be non-empty strings")
        actual_tools.append(identity)
    if len(actual_tools) != len(set(actual_tools)):
        raise ValueError("identity report contains duplicate tool receipts")
    if sorted(actual_tools) != sorted(expected_tools):
        raise ValueError("identity report tool receipts do not match selected tools")

    results = payload.get("results")
    if not isinstance(results, list) or len(results) != 1:
        raise ValueError("identity report must contain exactly one result")
    result = results[0]
    if not isinstance(result, dict):
        raise ValueError("identity result must be a JSON object")
    if result.get("case_id") != case_id:
        raise ValueError("identity report case identity mismatch")

    status = result.get("status")
    if not isinstance(status, str) or status not in RESULT_STATUSES:
        raise ValueError(f"identity report has unknown status: {status!r}")
    duration = result.get("duration_seconds")
    if (
        isinstance(duration, bool)
        or not isinstance(duration, (int, float))
        or not math.isfinite(duration)
        or duration < 0
    ):
        raise ValueError("identity result duration_seconds must be non-negative")
    failures = result.get("failures")
    if not isinstance(failures, list) or any(
        not isinstance(item, str) or not item for item in failures
    ):
        raise ValueError("identity result failures must be a list of strings")
    blocked_by = result.get("blocked_by")
    if not isinstance(blocked_by, list) or any(
        not isinstance(item, str) or not item for item in blocked_by
    ):
        raise ValueError("identity result blocked_by must be a list of strings")

    if status == "PASS" and (failures or blocked_by):
        raise ValueError("PASS cannot contain failures or blockers")
    if status == "BLOCKED" and not blocked_by:
        raise ValueError("BLOCKED requires blocked_by evidence")
    if status != "BLOCKED" and blocked_by:
        raise ValueError("only BLOCKED can contain blocked_by evidence")
    if status in {
        "FAIL",
        "KNOWN_FAILURE",
        "UNEXPECTED_PASS",
        "INFRASTRUCTURE_ERROR",
    } and not failures:
        raise ValueError(f"{status} requires failure evidence")

    counts = payload.get("counts")
    expected_counts = {name: 0 for name in RESULT_STATUSES}
    expected_counts[status] = 1
    if (
        not isinstance(counts, dict)
        or any(
            not isinstance(name, str)
            or isinstance(count, bool)
            or not isinstance(count, int)
            or count < 0
            for name, count in counts.items()
        )
        or counts != expected_counts
    ):
        raise ValueError("identity report counts do not match its result")

    expected_exit = 0 if status in {"PASS", "KNOWN_FAILURE"} else 1
    if exit_code != expected_exit:
        raise ValueError(
            f"identity controller exit code {exit_code} contradicts status {status}; "
            f"expected {expected_exit}"
        )
    return status, "; ".join(failures)


def parse_python_gate_result(value: Any) -> tuple[bool, str]:
    """Accept only the exact result protocol declared by imported Python gates."""
    if type(value) is not tuple or len(value) != 2:
        raise ValueError("Python gate must return a two-item tuple")
    satisfied, detail = value
    if type(satisfied) is not bool:
        raise ValueError("Python gate satisfaction must be a boolean")
    if not isinstance(detail, str):
        raise ValueError("Python gate detail must be a string")
    if not satisfied and not detail:
        raise ValueError("failed Python gate must provide non-empty detail")
    return satisfied, detail


def parse_selfhost_oracle(value: Any) -> tuple[int, dict[str, Any] | None]:
    """Validate the closed oracle schema used by executable self-host cases."""
    if not isinstance(value, dict):
        raise ValueError("self-host oracle must be a JSON object")
    unknown = sorted(set(value) - {"exit_code", "description", "json"})
    if unknown:
        raise ValueError("self-host oracle contains unknown fields: " + ", ".join(unknown))
    exit_code = value.get("exit_code")
    if isinstance(exit_code, bool) or not isinstance(exit_code, int):
        raise ValueError("self-host oracle exit_code must be an integer")
    description = value.get("description")
    if description is not None and not isinstance(description, str):
        raise ValueError("self-host oracle description must be a string")
    expected_json = value.get("json")
    if "json" in value and not isinstance(expected_json, dict):
        raise ValueError("self-host oracle json must be an object")
    return exit_code, expected_json


def equal_json_exact(expected: Any, actual: Any) -> bool:
    """Compare JSON values iteratively without Python scalar coercions."""
    pending: list[tuple[Any, Any]] = [(expected, actual)]
    while pending:
        expected_value, actual_value = pending.pop()
        if type(expected_value) is not type(actual_value):
            return False
        if isinstance(expected_value, dict):
            if expected_value.keys() != actual_value.keys():
                return False
            pending.extend(
                (expected_value[key], actual_value[key]) for key in expected_value
            )
            continue
        if isinstance(expected_value, list):
            if len(expected_value) != len(actual_value):
                return False
            pending.extend(zip(expected_value, actual_value, strict=True))
            continue
        if expected_value != actual_value:
            return False
    return True


def parse_selfhost_json_output(content: str) -> Any:
    """Parse fixture JSON without allowing duplicate object definitions."""
    return json.loads(content, object_pairs_hook=unique_json_object)


def parse_selfhost_inputs(
    value: Any, suite_root: Path, expected_case_id: str
) -> tuple[str, str, bool]:
    """Validate the closed producer and invocation contract of self-host cases."""
    if not isinstance(value, dict):
        raise ValueError("self-host inputs must be a JSON object")
    unknown = sorted(
        set(value) - {"fixture_project", "case_argument", "sandbox_argument"}
    )
    if unknown:
        raise ValueError("self-host inputs contain unknown fields: " + ", ".join(unknown))

    fixture = value.get("fixture_project")
    if not isinstance(fixture, str) or not fixture:
        raise ValueError("self-host fixture_project must be non-empty text")
    relative = Path(fixture)
    root = suite_root.resolve()
    resolved = (root / relative).resolve()
    if (
        relative.is_absolute()
        or root not in resolved.parents
        or resolved.suffix.casefold() != ".nsproj"
        or not resolved.is_file()
    ):
        raise ValueError("self-host fixture_project must be a local existing .nsproj")

    argument = value.get("case_argument")
    if not isinstance(argument, str) or not argument:
        raise ValueError("self-host case_argument must be non-empty text")
    if argument != expected_case_id:
        raise ValueError("self-host case_argument does not match selected case")
    sandbox = value.get("sandbox_argument")
    if sandbox is not None and sandbox != "fresh-case-directory":
        raise ValueError(
            "self-host sandbox_argument must be absent or fresh-case-directory"
        )
    return fixture, argument, sandbox == "fresh-case-directory"


def configure_commands(config: BuilderConfig, modes: Sequence[str]) -> list[tuple[str, list[str], Path]]:
    """Create explicit standalone CMake commands without building NSC."""
    tools = discover_build_tools(config.llvm_dir)
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        build = effective_build_dir(config, mode)
        command = [
            tools["cmake"],
            "-S",
            SUITE_ROOT.as_posix(),
            "-B",
            build.as_posix(),
            "-G",
            "Ninja",
            f"-DCMAKE_MAKE_PROGRAM={Path(tools['ninja']).as_posix()}",
            f"-DCMAKE_BUILD_TYPE={mode}",
            f"-DCMAKE_CXX_COMPILER={Path(tools['clang_cl']).as_posix()}",
            f"-DCMAKE_LINKER={Path(tools['lld_link']).as_posix()}",
            f"-DCMAKE_MT={Path(tools['llvm_mt']).as_posix()}",
            f"-DCMAKE_RC_COMPILER={Path(tools['llvm_rc']).as_posix()}",
            # CMAKE_LINKER already selects lld-link. Clear a stale cache value
            # because -fuse-ld=lld is a compiler-driver option, not an lld-link option.
            "-DCMAKE_EXE_LINKER_FLAGS=",
            f"-DNSC_COMPILER_SUITE_NSC_EXECUTABLE="
            f"{Path(config.nsc_executable).resolve().as_posix()}",
            f"-DNSC_NSLINK_EXECUTABLE={Path(config.nslink_executable).resolve().as_posix()}",
        ]
        command.append(f"-DLLVM_DIR={Path(tools['llvm_dir']).as_posix()}")
        if config.archive_executable:
            command.append(
                f"-DNSC_COMPILER_SUITE_ARCHIVE_EXECUTABLE="
                f"{Path(config.archive_executable).resolve().as_posix()}"
            )
        else:
            command.append("-UNSC_COMPILER_SUITE_ARCHIVE_EXECUTABLE")
        if config.comparison_nsc:
            command.append(
                f"-DNSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE="
                f"{Path(config.comparison_nsc).resolve().as_posix()}"
            )
        else:
            command.append("-UNSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE")
        if config.build_id_header:
            command.append(
                f"-DNSC_COMPILER_SUITE_BUILD_ID_HEADER="
                f"{Path(config.build_id_header).resolve().as_posix()}"
            )
        else:
            command.append("-UNSC_COMPILER_SUITE_BUILD_ID_HEADER")
        commands.append((f"Configurar {mode}", command, SOURCE_ROOT))
    return commands


def build_commands(config: BuilderConfig, modes: Sequence[str]) -> list[tuple[str, list[str], Path]]:
    """Build only the consolidated host-test target in selected configurations."""
    tools = discover_build_tools(config.llvm_dir)
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        build = effective_build_dir(config, mode)
        command = [
            tools["cmake"],
            "--build",
            build.as_posix(),
            "--config",
            mode,
            "--target",
            "nsc_component_tests",
            "--parallel",
            str(config.jobs),
        ]
        commands.append((f"Construir infraestructura {mode}", command, SOURCE_ROOT))
    return commands


def compiler_configure_commands(
    config: BuilderConfig,
    modes: Sequence[str],
    linkage: str,
    host_optimization: str,
) -> list[tuple[str, list[str], Path]]:
    """Configure NSC itself with the canonical Clang/LLVM/LLD Windows toolchain."""
    tools = discover_build_tools(config.llvm_dir)
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        build = compiler_build_dir(config, mode)
        command = [
            tools["cmake"],
            "-S",
            SOURCE_ROOT.as_posix(),
            "-B",
            build.as_posix(),
            "-G",
            "Ninja",
            f"-DCMAKE_MAKE_PROGRAM={Path(tools['ninja']).as_posix()}",
            f"-DCMAKE_BUILD_TYPE={mode}",
            f"-DCMAKE_INSTALL_PREFIX={Path(config.install_prefix).resolve().as_posix()}",
            f"-DCMAKE_C_COMPILER={Path(tools['clang_cl']).as_posix()}",
            f"-DCMAKE_CXX_COMPILER={Path(tools['clang_cl']).as_posix()}",
            f"-DCMAKE_LINKER={Path(tools['lld_link']).as_posix()}",
            f"-DCMAKE_MT={Path(tools['llvm_mt']).as_posix()}",
            f"-DCMAKE_RC_COMPILER={Path(tools['llvm_rc']).as_posix()}",
            "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld",
            "-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=FALSE",
            "-UNSC_LLVM_MONOLITHIC_LIBRARY",
            f"-DLLVM_DIR={Path(tools['llvm_dir']).as_posix()}",
            "-DNSC_BUILD_TESTS=OFF",
            "-DNSC_WARNINGS_AS_ERRORS=ON",
            f"-DNSC_HOST_LINKAGE={linkage.upper()}",
            f"-DNSC_HOST_OPTIMIZATION={host_optimization}",
            f"-DNSC_NSLINK_EXECUTABLE={Path(config.nslink_executable).resolve().as_posix()}",
            "-DNSC_BUILD_ID_OVERRIDE=",
        ]
        commands.append((f"Configurar NSC {mode}", command, SOURCE_ROOT))
    return commands


def compiler_build_commands(
    config: BuilderConfig, modes: Sequence[str]
) -> list[tuple[str, list[str], Path]]:
    """Build only the NSC target from previously configured compiler trees."""
    tools = discover_build_tools(config.llvm_dir)
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        command = [
            tools["cmake"],
            "--build",
            compiler_build_dir(config, mode).as_posix(),
            "--config",
            mode,
            "--target",
            "nsc",
            "--parallel",
            str(config.jobs),
            "--",
            "-k",
            "0",
        ]
        commands.append((f"Construir NSC {mode}", command, SOURCE_ROOT))
    return commands


class ExecutionEngine:
    """Execute concrete variants while preserving producer/consumer boundaries."""

    def __init__(
        self,
        config: BuilderConfig,
        store: EvidenceStore,
        output: Callable[[str], None],
        status: Callable[[str, str, str, float, str], None],
        progress: Callable[[int, int], None],
    ) -> None:
        self.config = config
        self.store = store
        self.output = output
        self.status = status
        self.progress = progress
        self.process = ProcessController(self._emit_output)
        self.fixture_cache: dict[tuple[str, str, str], Path] = {}
        self.fixture_failures: dict[tuple[str, str, str], tuple[str, str]] = {}
        self.module_cache: dict[str, Any] = {}

    def run(self, variants: Sequence[RunVariant]) -> tuple[list[CaseOutcome], bool]:
        """Run every planned variant in deterministic dependency-oriented order."""
        ordered = sorted(
            variants,
            key=lambda item: (
                item.case.wave,
                item.case.component,
                item.case.contract_id,
                item.case.case_id,
                item.build_mode,
                item.optimization,
            ),
        )
        outcomes: list[CaseOutcome] = []
        total = len(ordered)
        for index, variant in enumerate(ordered, start=1):
            if self.process.cancelled.is_set():
                outcomes.extend(self._cancelled_outcomes(ordered[index - 1 :]))
                break
            self.status(variant.key, "RUNNING", "", 0.0, "")
            self.store.event("CASE_STARTED", self._variant_payload(variant))
            outcome = self._run_variant(variant)
            outcomes.append(outcome)
            self.status(
                variant.key,
                outcome.status,
                outcome.detail,
                outcome.duration_seconds,
                outcome.evidence,
            )
            self.store.event("CASE_FINISHED", asdict(outcome))
            self.progress(index, total)
        return outcomes, self.process.cancelled.is_set()

    def cancel(self) -> None:
        """Request cancellation of the active child and all remaining variants."""
        self.process.cancel()

    def _run_variant(self, variant: RunVariant) -> CaseOutcome:
        """Dispatch one variant by its manifest-owned executor contract."""
        started = time.monotonic()
        self.process.last_termination_reason = None
        try:
            if variant.case.executor == "nsc_component_tests":
                status, detail, evidence = self._run_host(variant)
            elif variant.case.executor != "nsc_suite_controller+nsc":
                raise ValueError(
                    f"unsupported executor in NSC Builder 3: {variant.case.executor}"
                )
            elif variant.case.component.startswith("SelfHost."):
                status, detail, evidence = self._run_selfhost(variant)
            elif variant.case.component == "BuildSystem":
                status, detail, evidence = self._run_buildsystem(variant)
            elif variant.case.component == "Packaging":
                status, detail, evidence = self._run_packaging(variant)
            else:
                status, detail, evidence = self._run_identity(variant)
        except Exception as error:
            status = "INFRASTRUCTURE_ERROR"
            detail = f"{type(error).__name__}: {error}"
            evidence = ""
        duration = time.monotonic() - started
        if self.process.cancelled.is_set():
            status = "CANCELLED"
            detail = "cancelled by user"
        elif self.process.last_termination_reason == "timeout":
            status = "INFRASTRUCTURE_ERROR"
            detail = "process timed out"
        return CaseOutcome(
            key=variant.key,
            case_id=variant.case.case_id,
            contract_id=variant.case.contract_id,
            component=variant.case.component,
            wave=variant.case.wave,
            build_mode=variant.build_mode,
            optimization=variant.optimization,
            status=status,
            duration_seconds=duration,
            detail=detail,
            evidence=evidence,
        )

    def _run_host(self, variant: RunVariant) -> tuple[str, str, str]:
        """Run one consolidated C++ host case and validate its JSON envelope."""
        executable = host_executable(self.config, variant.build_mode)
        if not executable.is_file():
            return "INFRASTRUCTURE_ERROR", f"host executable does not exist: {executable}", ""
        case_root = self._case_root(variant)
        code, captured = self.process.run(
            [str(executable), "--case", variant.case.case_id],
            case_root,
            timeout=TIMEOUT_SECONDS[self._timeout_class(variant.case)],
        )
        log_path = case_root / "host.log"
        log_path.write_text(captured, encoding="utf-8", newline="\n")
        try:
            passed, detail = parse_host_envelope(
                captured=captured,
                exit_code=code,
                case_id=variant.case.case_id,
                contract_id=variant.case.contract_id,
                component=variant.case.component,
            )
        except (json.JSONDecodeError, ValueError) as error:
            return (
                "INFRASTRUCTURE_ERROR",
                str(error),
                str(log_path),
            )
        return self._expected_status(variant.case, passed, detail, str(log_path))

    def _run_identity(self, variant: RunVariant) -> tuple[str, str, str]:
        """Delegate one compiler-identity case to the existing manifest controller."""
        case_root = self._case_root(variant)
        run_id = variant.key.replace("|", "-")
        revision = current_revision()
        nsc = Path(self.config.nsc_executable).resolve()
        expected_tools = [("nsc", str(nsc), sha256_file(nsc))]
        command = [
            sys.executable,
            str(RUN_SUITE_PATH),
            "--manifest",
            str(MANIFEST_PATH),
            "--components",
            str(COMPONENTS_PATH),
            "--contracts",
            str(CONTRACTS_PATH),
            "--nsc-executable",
            str(nsc),
            "--work-root",
            str(case_root),
            "--run-id",
            run_id,
            "--revision",
            revision,
            "--build-mode",
            variant.build_mode,
            "--optimization",
            variant.optimization,
            "--case",
            variant.case.case_id,
        ]
        if self.config.comparison_nsc:
            comparison = Path(self.config.comparison_nsc).resolve()
            command.extend(["--comparison-nsc-executable", str(comparison)])
            expected_tools.append(
                ("comparison-nsc", str(comparison), sha256_file(comparison))
            )
        if self.config.build_id_header:
            header = Path(self.config.build_id_header).resolve()
            command.extend(["--build-id-header", str(header)])
            expected_tools.append(
                ("build-id-header", str(header), sha256_file(header))
            )
        code, captured = self.process.run(command, SOURCE_ROOT, timeout=180.0)
        log_path = case_root / "controller.log"
        log_path.write_text(captured, encoding="utf-8", newline="\n")
        result_path = case_root / run_id / "results.json"
        if not result_path.is_file():
            return "INFRASTRUCTURE_ERROR", f"controller exited with {code} without results", str(log_path)
        try:
            status, detail = parse_identity_envelope(
                content=result_path.read_text(encoding="utf-8"),
                exit_code=code,
                run_id=run_id,
                revision=revision,
                build_mode=variant.build_mode,
                optimization=variant.optimization,
                case_id=variant.case.case_id,
                expected_tools=expected_tools,
            )
        except (OSError, json.JSONDecodeError, TypeError, ValueError) as error:
            return "INFRASTRUCTURE_ERROR", str(error), str(result_path)
        return status, detail, str(result_path)

    def _run_selfhost(self, variant: RunVariant) -> tuple[str, str, str]:
        """Compile a real NativeSharp fixture once, then run one selected case."""
        manifest = variant.case.manifest or {}
        try:
            project_relative, case_argument, use_sandbox = parse_selfhost_inputs(
                manifest.get("inputs"), SUITE_ROOT, variant.case.case_id
            )
            expected, expected_json = parse_selfhost_oracle(manifest.get("oracle"))
        except ValueError as error:
            return "INFRASTRUCTURE_ERROR", str(error), ""
        fixture_key = (project_relative, variant.build_mode, variant.optimization)
        producer_failure = self.fixture_failures.get(fixture_key)
        if producer_failure is not None:
            detail, evidence = producer_failure
            return "BLOCKED", f"BLOQUEADO_POR fixture {project_relative}: {detail}", evidence
        try:
            executable = self._prepare_selfhost_fixture(variant, project_relative)
        except RuntimeError as error:
            project = SUITE_ROOT / project_relative
            fixture_root = self.store.root / "fixtures" / (
                f"{project.stem}-{variant.build_mode.casefold()}-{variant.optimization.casefold()}"
            )
            evidence = str(fixture_root / "compile.log")
            detail = str(error)
            self.fixture_failures[fixture_key] = (detail, evidence)
            return "INFRASTRUCTURE_ERROR", detail, evidence
        case_root = self._case_root(variant)
        if variant.case.component == "SelfHost.Source":
            self._prepare_source_files(case_root)
        command = [str(executable), case_argument]
        if use_sandbox:
            command.append(str(case_root))
        environment = self._nsc_environment()
        code, captured = self.process.run(
            command,
            case_root,
            environment,
            TIMEOUT_SECONDS[self._timeout_class(variant.case)],
        )
        log_path = case_root / "fixture.log"
        log_path.write_text(captured, encoding="utf-8", newline="\n")
        passed = code == expected
        detail = "" if passed else f"exit code {code}; expected {expected}"
        if passed and expected_json is not None:
            try:
                passed = equal_json_exact(
                    expected_json,
                    parse_selfhost_json_output(captured.strip()),
                )
            except json.JSONDecodeError as error:
                passed = False
                detail = f"invalid JSON output: {error.msg}"
            if not passed and not detail:
                detail = "JSON output differs from oracle"
        return self._expected_status(variant.case, passed, detail, str(log_path))

    def _prepare_selfhost_fixture(self, variant: RunVariant, relative: str) -> Path:
        """Build one fixture producer per project, mode and optimization."""
        key = (relative, variant.build_mode, variant.optimization)
        cached = self.fixture_cache.get(key)
        if cached is not None and cached.is_file():
            return cached
        project = SUITE_ROOT / relative
        fixture_root = self.store.root / "fixtures" / (
            f"{project.stem}-{variant.build_mode.casefold()}-{variant.optimization.casefold()}"
        )
        fixture_root.mkdir(parents=True, exist_ok=True)
        executable = fixture_root / f"{project.stem}.exe"
        command = [
            str(Path(self.config.nsc_executable).resolve()),
            "--emit-project-oop-exe",
            str(project),
            f"-{variant.optimization}",
            f"--profile={variant.build_mode.casefold()}",
            "-g0",
            "-o",
            str(executable),
        ]
        self.status(variant.key, "COMPILING", project.name, 0.0, str(fixture_root))
        write_json(fixture_root / "compile.command.json", command)
        code, captured = self.process.run(command, fixture_root, self._nsc_environment(), 900.0)
        (fixture_root / "compile.log").write_text(captured, encoding="utf-8", newline="\n")
        if self.process.last_termination_reason == "timeout":
            raise RuntimeError(f"fixture {project.name} timed out after 900.0 seconds")
        if self.process.last_termination_reason == "cancelled":
            raise RuntimeError(f"fixture {project.name} compilation was cancelled")
        if code != 0:
            raise RuntimeError(f"fixture {project.name} compiler exited with code {code}")
        if not executable.is_file():
            raise RuntimeError(f"fixture {project.name} completed without executable output")
        self.fixture_cache[key] = executable
        return executable

    def _run_buildsystem(self, variant: RunVariant) -> tuple[str, str, str]:
        """Execute one BuildSystem row through its existing case-level oracle."""
        module = self._load_gate("run_buildsystem_gate.py")
        tools = discover_build_tools(self.config.llvm_dir)
        cmake = Path(tools["cmake"]).resolve()
        installation = Path(self.config.nsc_executable).resolve().parent.parent
        arguments = argparse.Namespace(
            cmake_executable=cmake,
            source_root=SUITE_ROOT,
            repository_root=SOURCE_ROOT,
            installation_root=installation,
            work_parent=self.store.root,
            build_mode=variant.build_mode,
            root_config_argument=[
                "-G",
                "Ninja",
                f"-DCMAKE_MAKE_PROGRAM={Path(tools['ninja']).resolve().as_posix()}",
                f"-DCMAKE_BUILD_TYPE={variant.build_mode}",
                f"-DCMAKE_C_COMPILER={Path(tools['clang_cl']).resolve().as_posix()}",
                f"-DCMAKE_CXX_COMPILER={Path(tools['clang_cl']).resolve().as_posix()}",
                f"-DCMAKE_LINKER={Path(tools['lld_link']).resolve().as_posix()}",
                f"-DCMAKE_MT={Path(tools['llvm_mt']).resolve().as_posix()}",
                f"-DCMAKE_RC_COMPILER={Path(tools['llvm_rc']).resolve().as_posix()}",
                f"-DLLVM_DIR={Path(tools['llvm_dir']).resolve().as_posix()}",
                f"-DNSC_NSLINK_EXECUTABLE="
                f"{Path(self.config.nslink_executable).resolve().as_posix()}",
            ],
        )
        return self._run_python_gate_case(module, variant, arguments)

    def _run_packaging(self, variant: RunVariant) -> tuple[str, str, str]:
        """Execute one Packaging row through its existing case-level oracle."""
        archive = Path(self.config.archive_executable).resolve()
        if not archive.is_file():
            return "INFRASTRUCTURE_ERROR", "Packaging requires an archive executable", ""
        module = self._load_gate("run_packaging_gate.py")
        arguments = argparse.Namespace(
            python_executable=Path(sys.executable).resolve(),
            archive_executable=archive,
            source_root=SUITE_ROOT,
            repository_root=SOURCE_ROOT,
            installation_root=Path(self.config.nsc_executable).resolve().parent.parent,
            work_parent=self.store.root,
            build_mode=variant.build_mode,
        )
        return self._run_python_gate_case(module, variant, arguments)

    def _run_python_gate_case(
        self, module: Any, variant: RunVariant, options: argparse.Namespace
    ) -> tuple[str, str, str]:
        """Run one already-defined Python oracle while retaining per-case evidence."""
        case_root = self._case_root(variant)
        manifest = variant.case.manifest or {}
        original_runner = getattr(module, "run_process", None)
        if not callable(original_runner):
            return (
                "INFRASTRUCTURE_ERROR",
                "Python gate does not expose a callable run_process boundary",
                str(case_root),
            )

        def controlled_runner(
            command: Sequence[str],
            cwd: Path,
            environment: dict[str, str],
            timeout: float = 900.0,
        ) -> subprocess.CompletedProcess[str]:
            return self.process.run_completed(command, cwd, environment, timeout)

        raw_result: Any = None
        gate_error = ""
        module.run_process = controlled_runner
        try:
            raw_result = module.run_case(manifest, options, case_root, dict(os.environ))
        except Exception as error:
            gate_error = f"{type(error).__name__}: {error}"
        finally:
            module.run_process = original_runner
        self._emit_case_logs(case_root)
        if gate_error:
            return (
                "INFRASTRUCTURE_ERROR",
                gate_error,
                str(case_root),
            )
        evidence = str(case_root)
        try:
            satisfied, detail = parse_python_gate_result(raw_result)
        except ValueError as error:
            return "INFRASTRUCTURE_ERROR", str(error), evidence
        return self._expected_status(variant.case, satisfied, detail, evidence)

    def _emit_case_logs(self, case_root: Path) -> None:
        """Expose logs captured by imported gates in the unified progressive console."""
        for path in sorted(case_root.rglob("*.log")):
            try:
                content = path.read_text(encoding="utf-8", errors="replace")
            except OSError as error:
                self._emit_output(f"[{path.name}] unreadable: {error}\n")
                continue
            self._emit_output(f"\n===== {path.relative_to(case_root)} =====\n")
            self._emit_output(content)

    def _load_gate(self, name: str) -> Any:
        """Load one existing gate by path without adding new Builder modules."""
        cached = self.module_cache.get(name)
        if cached is not None:
            return cached
        path = SUITE_ROOT / "controller" / "integration" / name
        spec = importlib.util.spec_from_file_location(f"nscbuilder3_{path.stem}", path)
        if spec is None or spec.loader is None:
            raise RuntimeError(f"cannot load gate: {path}")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.module_cache[name] = module
        return module

    def _expected_status(
        self, case: CaseRecord, passed: bool, detail: str, evidence: str
    ) -> tuple[str, str, str]:
        """Preserve known defects as visible red outcomes rather than skips."""
        if case.expected_outcome == "KNOWN_FAILURE" or case.disposition == "KnownProductDefect":
            status = "UNEXPECTED_PASS" if passed else "KNOWN_FAILURE"
        else:
            status = "PASS" if passed else "FAIL"
        return status, detail, evidence

    def _case_root(self, variant: RunVariant) -> Path:
        """Create an isolated evidence directory for one concrete variant."""
        root = self.store.root / "cases" / variant.key.replace("|", "-")
        root.mkdir(parents=True, exist_ok=True)
        return root

    def _nsc_environment(self) -> dict[str, str]:
        """Build the explicit NSLink environment used by NativeSharp products."""
        nslink = Path(self.config.nslink_executable).resolve()
        environment = dict(os.environ)
        environment["NSC_LINKER"] = str(nslink)
        environment["PATH"] = str(nslink.parent) + os.pathsep + environment.get("PATH", "")
        return environment

    @staticmethod
    def _prepare_source_files(case_root: Path) -> None:
        """Create the exact isolated source corpus expected by SelfHost.Source."""
        (case_root / "source-empty.ns").write_bytes(b"")
        (case_root / "source-input.ns").write_bytes(b"alpha\r\nbeta\rgamma\n")
        (case_root / "source-unicode.ns").write_bytes("á\r\nβ".encode("utf-8"))
        (case_root / "source-frontend.ns").write_bytes(b"comptime int value = #run(2 + 3);\n")
        (case_root / "source-large.ns").write_bytes(b"x\r\n" * 32768)

    @staticmethod
    def _timeout_class(case: CaseRecord) -> str:
        """Read a validated timeout class from the executable manifest."""
        value = (case.manifest or {}).get("timeout_class")
        if not isinstance(value, str) or value not in TIMEOUT_SECONDS:
            raise ValueError(f"unsupported timeout class: {value}")
        return value

    @staticmethod
    def _variant_payload(variant: RunVariant) -> dict[str, Any]:
        """Serialize one variant identity for progressive events."""
        return {
            "key": variant.key,
            "case_id": variant.case.case_id,
            "contract_id": variant.case.contract_id,
            "component": variant.case.component,
            "wave": variant.case.wave,
            "build_mode": variant.build_mode,
            "optimization": variant.optimization,
        }

    @staticmethod
    def _cancelled_outcomes(variants: Sequence[RunVariant]) -> list[CaseOutcome]:
        """Materialize every unstarted variant as cancelled instead of dropping it."""
        return [
            CaseOutcome(
                key=item.key,
                case_id=item.case.case_id,
                contract_id=item.case.contract_id,
                component=item.case.component,
                wave=item.case.wave,
                build_mode=item.build_mode,
                optimization=item.optimization,
                status="CANCELLED",
                detail="not started because the run was cancelled",
            )
            for item in variants
        ]

    def _emit_output(self, line: str) -> None:
        """Mirror subprocess output to both the GUI and progressive evidence."""
        self.store.log(line)
        self.output(line)


def make_run_id() -> str:
    """Create a sortable collision-resistant local run identifier."""
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    return f"{stamp}-{uuid.uuid4().hex[:8]}"


def compiler_receipt(config: BuilderConfig) -> dict[str, Any]:
    """Capture immutable identities for the externally supplied NSC and NSLink."""
    config.validated(require_tools=True)
    receipt: dict[str, Any] = {"captured_at": utc_now(), "tools": []}
    for name, text in (
        ("nsc", config.nsc_executable),
        ("nslink", config.nslink_executable),
    ):
        path = Path(text).resolve()
        receipt["tools"].append(
            {
                "name": name,
                "path": str(path),
                "size": path.stat().st_size,
                "mtime_ns": path.stat().st_mtime_ns,
                "sha256": sha256_file(path),
            }
        )
    return receipt


def file_receipt(name: str, path_value: str | Path) -> dict[str, Any]:
    """Capture one immutable file identity for build evidence."""
    path = Path(path_value).resolve()
    if not path.is_file():
        raise FileNotFoundError(f"{name} does not exist: {path}")
    return {
        "name": name,
        "path": str(path),
        "size": path.stat().st_size,
        "mtime_ns": path.stat().st_mtime_ns,
        "sha256": sha256_file(path),
    }


def catalog_audit() -> dict[str, Any]:
    """Load the complete catalogue and expose closure facts without running tests."""
    catalog = CanonicalCatalog.load()
    summary = catalog.summary()
    summary["manifest_rows"] = len(read_jsonl(MANIFEST_PATH))
    summary["non_executable"] = len(catalog.cases) - int(summary["executable"])
    summary["unclassified"] = sum(
        case.disposition == "UNCLASSIFIED" for case in catalog.cases
    )
    return summary


def launch_gui(smoke_only: bool = False) -> int:
    """Launch the complete single-file PySide6 application."""
    try:
        from PySide6.QtCore import QThread, QUrl, Signal
        from PySide6.QtGui import QColor, QDesktopServices
        from PySide6.QtWidgets import (
            QApplication,
            QComboBox,
            QFileDialog,
            QFormLayout,
            QGridLayout,
            QGroupBox,
            QHBoxLayout,
            QHeaderView,
            QLabel,
            QLineEdit,
            QMainWindow,
            QMessageBox,
            QProgressBar,
            QPushButton,
            QSpinBox,
            QTabWidget,
            QTableWidget,
            QTableWidgetItem,
            QTextEdit,
            QVBoxLayout,
            QWidget,
        )
    except ImportError as error:
        raise RuntimeError("NSC Builder 3 requires PySide6 for its graphical interface") from error

    class CommandWorker(QThread):
        """Run explicit compiler or test-infrastructure configure/build stages."""

        output_ready = Signal(str)
        stage_ready = Signal(str)
        progress_ready = Signal(int, int, str)
        completed = Signal(object, bool, str)

        def __init__(
            self,
            run_id: str,
            commands: Sequence[tuple[str, list[str], Path]],
            metadata: dict[str, Any],
        ) -> None:
            super().__init__()
            self.run_id = run_id
            self.commands = tuple(commands)
            self.metadata = metadata
            self.controller = ProcessController(self._output)
            self.store: EvidenceStore | None = None

        def run(self) -> None:
            outcomes: list[CaseOutcome] = []
            try:
                self.store = EvidenceStore(self.run_id, self.metadata)
                for index, (label, command, cwd) in enumerate(self.commands, start=1):
                    if self.controller.cancelled.is_set():
                        outcomes.extend(
                            cancelled_infrastructure_outcomes(
                                self.commands, index, str(self.store.log_path)
                            )
                        )
                        break
                    self.stage_ready.emit(label)
                    self._output(f"$ {subprocess.list2cmdline(command)}\n")
                    started = time.monotonic()
                    code, _ = self.controller.run(command, cwd)
                    duration = time.monotonic() - started
                    command_cancelled = self.controller.cancelled.is_set()
                    outcomes.append(
                        infrastructure_command_outcome(
                            index=index,
                            label=label,
                            exit_code=code,
                            duration_seconds=duration,
                            cancelled=command_cancelled,
                            evidence=str(self.store.log_path),
                        )
                    )
                    if command_cancelled:
                        outcomes.extend(
                            cancelled_infrastructure_outcomes(
                                self.commands, index + 1, str(self.store.log_path)
                            )
                        )
                        break
                    if code != 0:
                        break
                cancelled = self.controller.cancelled.is_set()
                self.store.finish(outcomes, cancelled)
                self.completed.emit(outcomes, cancelled, str(self.store.root))
            except Exception as error:
                if self.store is not None:
                    self.store.close()
                self.completed.emit([], False, f"ERROR: {type(error).__name__}: {error}")

        def cancel(self) -> None:
            self.controller.cancel()

        def _output(self, line: str) -> None:
            if self.store is not None:
                self.store.log(line)
            self.output_ready.emit(line)
            match = NINJA_PROGRESS_PATTERN.match(line.rstrip("\r\n"))
            if match is not None:
                self.progress_ready.emit(
                    int(match.group(1)),
                    int(match.group(2)),
                    match.group(3),
                )

    class SuiteWorker(QThread):
        """Execute one immutable variant plan and publish live GUI events."""

        output_ready = Signal(str)
        case_ready = Signal(str, str, str, float, str)
        progress_ready = Signal(int, int)
        completed = Signal(object, bool, str)

        def __init__(
            self,
            run_id: str,
            config: BuilderConfig,
            variants: Sequence[RunVariant],
            metadata: dict[str, Any],
        ) -> None:
            super().__init__()
            self.run_id = run_id
            self.config = config
            self.variants = tuple(variants)
            self.metadata = metadata
            self.engine: ExecutionEngine | None = None
            self.store: EvidenceStore | None = None

        def run(self) -> None:
            try:
                self.store = EvidenceStore(self.run_id, self.metadata)
                self.engine = ExecutionEngine(
                    self.config,
                    self.store,
                    self.output_ready.emit,
                    self.case_ready.emit,
                    self.progress_ready.emit,
                )
                outcomes, cancelled = self.engine.run(self.variants)
                self.store.finish(outcomes, cancelled)
                self.completed.emit(outcomes, cancelled, str(self.store.root))
            except Exception as error:
                if self.store is not None:
                    self.store.close()
                self.completed.emit([], False, f"ERROR: {type(error).__name__}: {error}")

        def cancel(self) -> None:
            if self.engine is not None:
                self.engine.cancel()

    class MainWindow(QMainWindow):
        """Present compiler-suite construction and execution as separate workflows."""

        TABLE_HEADERS = (
            "Ola",
            "Componente",
            "Contrato",
            "Caso",
            "Build",
            "Opt",
            "Perfil",
            "Ejecutor",
            "Estado",
            "Duración",
            "Detalle / evidencia",
        )

        def __init__(self) -> None:
            super().__init__()
            self.catalog = CanonicalCatalog.load()
            self.worker: CommandWorker | SuiteWorker | None = None
            self.last_failed_variants: tuple[RunVariant, ...] = ()
            self.visible_variants: tuple[RunVariant, ...] = ()
            self.status_item_by_key: dict[str, QTableWidgetItem] = {}
            self.path_edits: dict[str, QLineEdit] = {}
            self.active_operation = ""
            self.active_modes: tuple[str, ...] = ()
            self.infrastructure_ninja_total = 0
            self.setWindowTitle(APP_NAME)
            self.resize(1600, 960)
            self._build_ui()
            self._load_config(BuilderConfig.load())
            self._refresh_scope_values()
            self._refresh_catalog_summary()
            self._preview_plan()
            self._refresh_history()

        def _build_ui(self) -> None:
            central = QWidget()
            outer = QVBoxLayout(central)
            self.summary_label = QLabel()
            self.summary_label.setWordWrap(True)
            outer.addWidget(self.summary_label)
            self.tabs = QTabWidget()
            self.tabs.addTab(self._build_infrastructure_tab(), "Construcción")
            self.tabs.addTab(self._build_execution_tab(), "Ejecución Fase 2")
            self.tabs.addTab(self._build_results_tab(), "Resultados")
            outer.addWidget(self.tabs, 1)
            self.setCentralWidget(central)
            self.statusBar().showMessage("Listo")

        def _build_infrastructure_tab(self) -> QWidget:
            page = QWidget()
            layout = QVBoxLayout(page)
            notice = QLabel(
                "Primero construye NSC con Clang/LLVM/LLD; después configura y construye "
                "la infraestructura nueva de pruebas con ese compilador. Las rutas conocidas "
                "se descubren automáticamente y los campos permiten overrides explícitos."
            )
            notice.setWordWrap(True)
            layout.addWidget(notice)
            paths = QGroupBox("Herramientas y rutas")
            form = QFormLayout(paths)
            self._add_path_row(form, "nsc_executable", "NSC seleccionado", False)
            self._add_path_row(form, "nslink_executable", "NSLink", False)
            self._add_path_row(form, "compiler_build_root", "Build de NSC", True)
            self._add_path_row(form, "install_prefix", "Prefijo de instalación", True)
            self._add_path_row(form, "build_root", "Árbol de pruebas", True)
            self._add_path_row(form, "llvm_dir", "LLVM_DIR", True)
            self._add_path_row(form, "archive_executable", "Archivador", False)
            self._add_path_row(form, "comparison_nsc", "NSC de comparación", False)
            self._add_path_row(form, "build_id_header", "Build-id header", False)
            self.jobs_spin = QSpinBox()
            self.jobs_spin.setRange(1, 256)
            form.addRow("Trabajos paralelos", self.jobs_spin)
            layout.addWidget(paths)
            compiler_group = QGroupBox("1. Construcción del compilador NSC")
            compiler_controls = QHBoxLayout(compiler_group)
            self.compiler_mode = QComboBox()
            self.compiler_mode.addItems(["Release", "Debug", "Todos"])
            self.compiler_linkage = QComboBox()
            self.compiler_linkage.addItems(["Static", "Dynamic"])
            self.compiler_optimization = QComboBox()
            self.compiler_optimization.addItems(["O0", "O1", "O2", "O3"])
            self.compiler_configure_button = QPushButton("Configurar NSC")
            self.compiler_build_button = QPushButton("Construir NSC")
            self.infrastructure_cancel_button = QPushButton("Cancelar construcción")
            self.infrastructure_cancel_button.setEnabled(False)
            compiler_controls.addWidget(QLabel("Build"))
            compiler_controls.addWidget(self.compiler_mode)
            compiler_controls.addWidget(QLabel("Enlace host"))
            compiler_controls.addWidget(self.compiler_linkage)
            compiler_controls.addWidget(QLabel("Optimización host"))
            compiler_controls.addWidget(self.compiler_optimization)
            compiler_controls.addWidget(self.compiler_configure_button)
            compiler_controls.addWidget(self.compiler_build_button)
            compiler_controls.addWidget(self.infrastructure_cancel_button)
            compiler_controls.addStretch(1)
            layout.addWidget(compiler_group)
            tests_group = QGroupBox("2. Infraestructura de pruebas")
            controls = QHBoxLayout(tests_group)
            self.infrastructure_mode = QComboBox()
            self.infrastructure_mode.addItems(["Debug", "Release", "Todos"])
            controls.addWidget(QLabel("Build"))
            controls.addWidget(self.infrastructure_mode)
            self.save_button = QPushButton("Guardar configuración")
            self.detect_button = QPushButton("Autodetectar rutas")
            self.configure_button = QPushButton("Configurar pruebas")
            self.build_button = QPushButton("Construir infraestructura")
            self.receipt_button = QPushButton("Validar compilador externo")
            controls.addWidget(self.save_button)
            controls.addWidget(self.detect_button)
            controls.addWidget(self.receipt_button)
            controls.addWidget(self.configure_button)
            controls.addWidget(self.build_button)
            controls.addStretch(1)
            layout.addWidget(tests_group)
            self.infrastructure_stage = QLabel("Sin operación activa")
            layout.addWidget(self.infrastructure_stage)
            self.infrastructure_progress = QProgressBar()
            self.infrastructure_progress.setRange(0, 1)
            self.infrastructure_progress.setValue(0)
            self.infrastructure_progress.setFormat("Sin pasos de Ninja")
            layout.addWidget(self.infrastructure_progress)
            self.infrastructure_progress_detail = QLabel("Sin actividad de construcción")
            self.infrastructure_progress_detail.setWordWrap(True)
            layout.addWidget(self.infrastructure_progress_detail)
            self.infrastructure_log = QTextEdit()
            self.infrastructure_log.setReadOnly(True)
            layout.addWidget(self.infrastructure_log, 1)
            self.save_button.clicked.connect(self._save_config)
            self.detect_button.clicked.connect(self._redetect_config)
            self.receipt_button.clicked.connect(self._validate_receipt)
            self.compiler_configure_button.clicked.connect(
                lambda: self._start_infrastructure("compiler-configure")
            )
            self.compiler_build_button.clicked.connect(
                lambda: self._start_infrastructure("compiler-build")
            )
            self.infrastructure_cancel_button.clicked.connect(self._cancel_active)
            self.configure_button.clicked.connect(
                lambda: self._start_infrastructure("tests-configure")
            )
            self.build_button.clicked.connect(
                lambda: self._start_infrastructure("tests-build")
            )
            return page

        def _build_execution_tab(self) -> QWidget:
            page = QWidget()
            layout = QVBoxLayout(page)
            filters = QGroupBox("Selección granular")
            grid = QGridLayout(filters)
            self.scope_combo = QComboBox()
            self.scope_combo.addItems(["Total", "Ola", "Componente", "Contrato", "Caso"])
            self.scope_value = QComboBox()
            self.scope_value.setEditable(True)
            self.profile_combo = QComboBox()
            self.profile_combo.addItems(["Todos", "PR", "NIGHTLY", "WEEKLY"])
            self.execution_mode = QComboBox()
            self.execution_mode.addItems(["Debug", "Release", "Todos"])
            self.optimization_combo = QComboBox()
            self.optimization_combo.addItems(["O0", "O1", "O2", "O3", "Todas"])
            grid.addWidget(QLabel("Alcance"), 0, 0)
            grid.addWidget(self.scope_combo, 0, 1)
            grid.addWidget(QLabel("Valor"), 0, 2)
            grid.addWidget(self.scope_value, 0, 3)
            grid.addWidget(QLabel("Perfil"), 0, 4)
            grid.addWidget(self.profile_combo, 0, 5)
            grid.addWidget(QLabel("Build"), 1, 0)
            grid.addWidget(self.execution_mode, 1, 1)
            grid.addWidget(QLabel("Optimización NativeSharp"), 1, 2)
            grid.addWidget(self.optimization_combo, 1, 3)
            self.preview_button = QPushButton("Previsualizar")
            self.run_button = QPushButton("Ejecutar selección")
            self.cancel_button = QPushButton("Cancelar árbol")
            self.retry_button = QPushButton("Reejecutar fallos")
            self.cancel_button.setEnabled(False)
            self.retry_button.setEnabled(False)
            grid.addWidget(self.preview_button, 1, 4)
            grid.addWidget(self.run_button, 1, 5)
            grid.addWidget(self.cancel_button, 2, 4)
            grid.addWidget(self.retry_button, 2, 5)
            layout.addWidget(filters)
            self.plan_label = QLabel()
            layout.addWidget(self.plan_label)
            self.progress = QProgressBar()
            self.progress.setRange(0, 1)
            self.progress.setValue(0)
            layout.addWidget(self.progress)
            self.case_table = QTableWidget(0, len(self.TABLE_HEADERS))
            self.case_table.setHorizontalHeaderLabels(self.TABLE_HEADERS)
            self.case_table.setAlternatingRowColors(True)
            self.case_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
            self.case_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
            self.case_table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.ResizeToContents)
            self.case_table.horizontalHeader().setStretchLastSection(True)
            layout.addWidget(self.case_table, 1)
            self.scope_combo.currentTextChanged.connect(self._refresh_scope_values)
            self.preview_button.clicked.connect(self._preview_plan)
            self.run_button.clicked.connect(self._start_suite)
            self.cancel_button.clicked.connect(self._cancel_active)
            self.retry_button.clicked.connect(self._retry_failures)
            return page

        def _build_results_tab(self) -> QWidget:
            page = QWidget()
            layout = QVBoxLayout(page)
            self.result_tabs = QTabWidget()
            self.full_log = QTextEdit()
            self.full_log.setReadOnly(True)
            self.error_report = QTextEdit()
            self.error_report.setReadOnly(True)
            self.history_table = QTableWidget(0, 5)
            self.history_table.setHorizontalHeaderLabels(
                ["Ejecución", "Tipo", "Estado", "Fecha", "Directorio"]
            )
            self.history_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
            self.history_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
            self.history_table.horizontalHeader().setSectionResizeMode(
                QHeaderView.ResizeMode.ResizeToContents
            )
            self.history_table.horizontalHeader().setStretchLastSection(True)
            history_page = QWidget()
            history_layout = QVBoxLayout(history_page)
            history_controls = QHBoxLayout()
            refresh = QPushButton("Actualizar historial")
            open_button = QPushButton("Abrir evidencia seleccionada")
            history_controls.addWidget(refresh)
            history_controls.addWidget(open_button)
            history_controls.addStretch(1)
            history_layout.addLayout(history_controls)
            history_layout.addWidget(self.history_table)
            refresh.clicked.connect(self._refresh_history)
            open_button.clicked.connect(self._open_history)
            self.history_table.cellDoubleClicked.connect(lambda _row, _column: self._open_history())
            self.result_tabs.addTab(self.full_log, "Log completo")
            self.result_tabs.addTab(self.error_report, "Errores condensados")
            self.result_tabs.addTab(history_page, "Historial")
            layout.addWidget(self.result_tabs)
            return page

        def _add_path_row(
            self, form: QFormLayout, key: str, label: str, directory: bool
        ) -> None:
            container = QWidget()
            row = QHBoxLayout(container)
            row.setContentsMargins(0, 0, 0, 0)
            edit = QLineEdit()
            button = QPushButton("Examinar…")
            button.clicked.connect(lambda: self._browse_path(key, directory))
            row.addWidget(edit, 1)
            row.addWidget(button)
            self.path_edits[key] = edit
            form.addRow(label, container)

        def _browse_path(self, key: str, directory: bool) -> None:
            current = self.path_edits[key].text().strip() or str(SOURCE_ROOT)
            if directory:
                value = QFileDialog.getExistingDirectory(self, "Seleccionar directorio", current)
            else:
                value, _ = QFileDialog.getOpenFileName(self, "Seleccionar archivo", current)
            if value:
                self.path_edits[key].setText(value)

        def _load_config(self, config: BuilderConfig) -> None:
            for key, edit in self.path_edits.items():
                edit.setText(str(getattr(config, key)))
            self.jobs_spin.setValue(config.jobs)

        def _redetect_config(self) -> None:
            try:
                discovered = self._config_from_ui().with_discovery()
                self._load_config(discovered)
                tools = discover_build_tools(discovered.llvm_dir)
                lines = ["RUTAS AUTODETECTADAS", "=" * 72]
                lines.extend(f"{name}: {value}" for name, value in tools.items())
                lines.extend(
                    [
                        f"NSC: {discovered.nsc_executable}",
                        f"NSLink: {discovered.nslink_executable}",
                        f"Fuentes NSC: {SOURCE_ROOT}",
                        f"Fuentes de pruebas: {SUITE_ROOT}",
                    ]
                )
                self.infrastructure_log.setPlainText("\n".join(lines))
            except Exception as error:
                self._show_error("No se pudieron autodetectar las rutas", error)

        def _config_from_ui(self) -> BuilderConfig:
            return BuilderConfig(
                nsc_executable=self.path_edits["nsc_executable"].text().strip(),
                nslink_executable=self.path_edits["nslink_executable"].text().strip(),
                compiler_build_root=self.path_edits["compiler_build_root"].text().strip(),
                install_prefix=self.path_edits["install_prefix"].text().strip(),
                build_root=self.path_edits["build_root"].text().strip(),
                llvm_dir=self.path_edits["llvm_dir"].text().strip(),
                archive_executable=self.path_edits["archive_executable"].text().strip(),
                comparison_nsc=self.path_edits["comparison_nsc"].text().strip(),
                build_id_header=self.path_edits["build_id_header"].text().strip(),
                jobs=self.jobs_spin.value(),
            )

        def _save_config(self) -> None:
            try:
                config = self._config_from_ui().validated(require_tools=False)
                config.save()
                self.statusBar().showMessage(f"Configuración guardada en {CONFIG_PATH}", 8000)
            except Exception as error:
                self._show_error("Configuración inválida", error)

        def _validate_receipt(self) -> None:
            try:
                config = self._config_from_ui()
                receipt = compiler_receipt(config)
                config.save()
                lines = ["RECIBO DEL COMPILADOR EXTERNO", "=" * 72]
                for tool in receipt["tools"]:
                    lines.extend(
                        [
                            f"{tool['name']}: {tool['path']}",
                            f"  SHA-256: {tool['sha256']}",
                            f"  Tamaño: {tool['size']} bytes",
                        ]
                    )
                self.infrastructure_log.setPlainText("\n".join(lines))
            except Exception as error:
                self._show_error("No se pudo validar el recibo", error)

        def _start_infrastructure(self, operation: str) -> None:
            if self.worker is not None:
                return
            try:
                config = self._config_from_ui().validated(require_tools=False)
                self._load_config(config)
                tools = discover_build_tools(config.llvm_dir)
                compiler_operation = operation.startswith("compiler-")
                selected = (
                    self.compiler_mode.currentText()
                    if compiler_operation
                    else self.infrastructure_mode.currentText()
                )
                modes = ("Debug", "Release") if selected == "Todos" else (selected,)
                if compiler_operation:
                    nslink = require_a_path(
                        Path(config.nslink_executable), "NSLink executable", True
                    )
                    configure = compiler_configure_commands(
                        config,
                        modes,
                        self.compiler_linkage.currentText(),
                        self.compiler_optimization.currentText(),
                    )
                    if operation == "compiler-configure":
                        commands = configure
                    else:
                        commands = compiler_configure_commands(
                            config,
                            modes,
                            self.compiler_linkage.currentText(),
                            self.compiler_optimization.currentText(),
                        ) + compiler_build_commands(config, modes)
                    receipt: dict[str, Any] = {
                        "captured_at": utc_now(),
                        "tools": [file_receipt("nslink", nslink)],
                        "build_tools": tools,
                    }
                else:
                    config.validated(require_tools=True)
                    if operation == "tests-configure":
                        commands = configure_commands(config, modes)
                    else:
                        commands = configure_commands(config, modes) + build_commands(config, modes)
                    receipt = compiler_receipt(config)
                config.save()
                run_id = make_run_id()
                metadata = {
                    "schema": 1,
                    "kind": operation,
                    "run_id": run_id,
                    "revision": current_revision(),
                    "commands": [
                        {"label": label, "command": command, "cwd": str(cwd)}
                        for label, command, cwd in commands
                    ],
                    "tool_receipt": receipt,
                }
                self.infrastructure_log.clear()
                self.infrastructure_progress.setRange(0, 1)
                self.infrastructure_progress.setValue(0)
                self.infrastructure_progress.setFormat("Esperando pasos de Ninja")
                self.infrastructure_progress_detail.setText("Esperando salida completa del proceso")
                self.infrastructure_ninja_total = 0
                self.active_operation = operation
                self.active_modes = modes
                self._set_busy(True)
                worker = CommandWorker(run_id, commands, metadata)
                self.worker = worker
                worker.output_ready.connect(self._append_infrastructure_output)
                worker.stage_ready.connect(self._set_infrastructure_stage)
                worker.progress_ready.connect(self._update_infrastructure_progress)
                worker.completed.connect(self._infrastructure_finished)
                worker.start()
            except Exception as error:
                self._show_error("No se pudo iniciar la infraestructura", error)

        def _append_infrastructure_output(self, text: str) -> None:
            cursor = self.infrastructure_log.textCursor()
            cursor.movePosition(cursor.MoveOperation.End)
            cursor.insertText(text)
            self.infrastructure_log.setTextCursor(cursor)
            self.infrastructure_log.ensureCursorVisible()

        def _set_infrastructure_stage(self, stage: str) -> None:
            """Expose stage changes without discarding the unabridged console."""
            self.infrastructure_stage.setText(stage)
            self.infrastructure_progress.setRange(0, 1)
            self.infrastructure_progress.setValue(0)
            self.infrastructure_progress.setFormat("Esperando pasos de Ninja")
            self.infrastructure_progress_detail.setText("Esperando salida completa del proceso")
            self.infrastructure_ninja_total = 0

        def _update_infrastructure_progress(
            self,
            current: int,
            total: int,
            detail: str,
        ) -> None:
            """Render Ninja's own progress while retaining its original log line."""
            self.infrastructure_progress.setRange(0, max(total, 1))
            self.infrastructure_progress.setValue(min(current, total))
            self.infrastructure_progress.setFormat("%v/%m pasos de Ninja (%p%)")
            self.infrastructure_progress_detail.setText(detail or "Paso Ninja sin descripción")
            self.infrastructure_ninja_total = total

        def _infrastructure_finished(
            self, outcomes: object, cancelled: bool, evidence: str
        ) -> None:
            typed = list(outcomes) if isinstance(outcomes, list) else []
            failed = any(item.status != "PASS" for item in typed)
            if evidence.startswith("ERROR:"):
                message = evidence
            elif cancelled:
                message = f"Operación cancelada. Evidencia: {evidence}"
            elif failed:
                message = f"Operación terminada con errores. Evidencia: {evidence}"
            else:
                message = f"Operación terminada correctamente. Evidencia: {evidence}"
                total = self.infrastructure_ninja_total
                self.infrastructure_progress.setRange(0, max(total, 1))
                self.infrastructure_progress.setValue(max(total, 1))
                if total:
                    self.infrastructure_progress.setFormat(
                        "%v/%m pasos de Ninja — TERMINADO (100%)"
                    )
                else:
                    self.infrastructure_progress.setFormat("TERMINADO (100%)")
                self.infrastructure_progress_detail.setText(
                    "TERMINADO: la operación concluyó correctamente."
                )
                if self.active_operation == "compiler-build":
                    preferred = "Release" if "Release" in self.active_modes else self.active_modes[0]
                    config = self._config_from_ui()
                    built = compiler_output(config, preferred)
                    if built.is_file():
                        self.path_edits["nsc_executable"].setText(str(built))
                        self._config_from_ui().save()
                        message += f" NSC seleccionado: {built}"
            self.infrastructure_stage.setText(message)
            self.active_operation = ""
            self.worker = None
            self._set_busy(False)
            self._refresh_history()

        def _refresh_catalog_summary(self) -> None:
            summary = self.catalog.summary()
            dispositions = ", ".join(
                f"{name}: {count}" for name, count in summary["dispositions"].items()
            )
            self.summary_label.setText(
                f"Fase 2 · {summary['cases']:,} casos · {summary['contracts']:,} contratos · "
                f"{summary['components']} componentes · {summary['executable']:,} ejecutables · "
                f"{dispositions}"
            )

        def _refresh_scope_values(self) -> None:
            scope = self.scope_combo.currentText()
            prior = self.scope_value.currentText()
            values: Sequence[str]
            if scope == "Ola":
                values = tuple(str(value) for value in self.catalog.waves)
            elif scope == "Componente":
                values = self.catalog.components
            elif scope == "Contrato":
                values = self.catalog.contracts
            elif scope == "Caso":
                values = tuple(case.case_id for case in self.catalog.cases)
            else:
                values = ()
            self.scope_value.blockSignals(True)
            self.scope_value.clear()
            self.scope_value.addItems(values)
            if prior in values:
                self.scope_value.setCurrentText(prior)
            self.scope_value.setEnabled(bool(values))
            self.scope_value.blockSignals(False)

        def _selected_cases(self) -> tuple[CaseRecord, ...]:
            scope = self.scope_combo.currentText()
            value = self.scope_value.currentText().strip()
            if scope != "Total" and not value:
                raise ValueError(f"selecciona un valor para el alcance {scope}")
            return self.catalog.select(scope, value, self.profile_combo.currentText())

        def _preview_plan(self) -> None:
            try:
                cases = self._selected_cases()
                variants = expand_variants(
                    cases,
                    self.execution_mode.currentText(),
                    self.optimization_combo.currentText(),
                )
                self.visible_variants = variants
                self._populate_plan(cases, variants)
            except Exception as error:
                self._show_error("No se pudo preparar la selección", error)

        def _populate_plan(
            self, cases: Sequence[CaseRecord], variants: Sequence[RunVariant]
        ) -> None:
            executable_ids = {variant.case.case_id for variant in variants}
            pending = [case for case in cases if not case.executable]
            filtered = [
                case for case in cases if case.executable and case.case_id not in executable_ids
            ]
            rows: list[tuple[CaseRecord, str, str, str, str, str]] = []
            rows.extend(
                (
                    variant.case,
                    variant.build_mode,
                    variant.optimization,
                    variant.key,
                    "LISTO",
                    "",
                )
                for variant in variants
            )
            rows.extend(
                (case, "NA", "NA", f"{case.case_id}|NA|NA", "PENDIENTE", case.disposition)
                for case in pending
            )
            rows.extend(
                (
                    case,
                    "NA",
                    "NA",
                    f"{case.case_id}|FILTERED|FILTERED",
                    "FILTRADO",
                    "la combinación Build/Opt elegida no está soportada por el caso",
                )
                for case in filtered
            )
            self.case_table.setSortingEnabled(False)
            self.case_table.clearContents()
            self.case_table.setRowCount(len(rows))
            self.status_item_by_key.clear()
            for row_index, (case, mode, opt, key, status, detail) in enumerate(rows):
                values = (
                    str(case.wave),
                    case.component,
                    case.contract_id,
                    case.case_id,
                    mode,
                    opt,
                    case.profile,
                    case.executor,
                    status,
                    "",
                    detail,
                )
                for column, value in enumerate(values):
                    item = QTableWidgetItem(value)
                    if column == 8:
                        self._color_status(item, status)
                        self.status_item_by_key[key] = item
                    self.case_table.setItem(row_index, column, item)
            self.case_table.setSortingEnabled(True)
            self.plan_label.setText(
                f"Selección: {len(cases):,} casos canónicos · {len(variants):,} variantes "
                f"ejecutables · {len(pending):,} pendientes visibles · {len(filtered):,} filtradas"
            )
            self.progress.setRange(0, max(1, len(variants)))
            self.progress.setValue(0)

        def _start_suite(self) -> None:
            if self.worker is not None:
                return
            try:
                config = self._config_from_ui().validated(require_tools=True)
                config.save()
                cases = self._selected_cases()
                variants = expand_variants(
                    cases,
                    self.execution_mode.currentText(),
                    self.optimization_combo.currentText(),
                )
                if not variants:
                    raise ValueError("la selección no contiene variantes ejecutables")
                self._populate_plan(cases, variants)
                self._run_variants(config, variants, "selection")
            except Exception as error:
                self._show_error("No se pudo iniciar la ejecución", error)

        def _run_variants(
            self,
            config: BuilderConfig,
            variants: Sequence[RunVariant],
            origin: str,
        ) -> None:
            self.visible_variants = tuple(variants)
            run_id = make_run_id()
            metadata = {
                "schema": 1,
                "kind": "phase-2-suite",
                "origin": origin,
                "run_id": run_id,
                "revision": current_revision(),
                "variant_count": len(variants),
                "case_count": len({variant.case.case_id for variant in variants}),
                "compiler_receipt": compiler_receipt(config),
                "selection": {
                    "scope": self.scope_combo.currentText(),
                    "value": self.scope_value.currentText(),
                    "profile": self.profile_combo.currentText(),
                    "build_mode": self.execution_mode.currentText(),
                    "optimization": self.optimization_combo.currentText(),
                },
            }
            self.full_log.clear()
            self.error_report.clear()
            self.tabs.setCurrentIndex(1)
            self._set_busy(True)
            worker = SuiteWorker(run_id, config, variants, metadata)
            self.worker = worker
            worker.output_ready.connect(self._append_full_output)
            worker.case_ready.connect(self._update_case)
            worker.progress_ready.connect(self._update_progress)
            worker.completed.connect(self._suite_finished)
            worker.start()

        def _retry_failures(self) -> None:
            if self.worker is not None or not self.last_failed_variants:
                return
            try:
                config = self._config_from_ui().validated(require_tools=True)
                self._populate_plan(
                    tuple(variant.case for variant in self.last_failed_variants),
                    self.last_failed_variants,
                )
                self._run_variants(config, self.last_failed_variants, "rerun-failures")
            except Exception as error:
                self._show_error("No se pudieron reejecutar los fallos", error)

        def _append_full_output(self, text: str) -> None:
            cursor = self.full_log.textCursor()
            cursor.movePosition(cursor.MoveOperation.End)
            cursor.insertText(text)
            self.full_log.setTextCursor(cursor)
            self.full_log.ensureCursorVisible()

        def _update_case(
            self,
            key: str,
            status: str,
            detail: str,
            duration: float,
            evidence: str,
        ) -> None:
            prior_status = self.status_item_by_key.get(key)
            if prior_status is None:
                return
            row = prior_status.row()
            status_item = QTableWidgetItem(status)
            self._color_status(status_item, status)
            self.case_table.setItem(row, 8, status_item)
            self.status_item_by_key[key] = status_item
            self.case_table.setItem(row, 9, QTableWidgetItem(f"{duration:.3f} s" if duration else ""))
            merged = detail
            if evidence:
                merged = f"{detail} | {evidence}" if detail else evidence
            self.case_table.setItem(row, 10, QTableWidgetItem(merged))
            self.case_table.scrollToItem(status_item)

        def _update_progress(self, done: int, total: int) -> None:
            self.progress.setRange(0, max(1, total))
            self.progress.setValue(done)
            self.statusBar().showMessage(f"Ejecutando {done:,}/{total:,}")

        def _suite_finished(self, outcomes: object, cancelled: bool, evidence: str) -> None:
            typed = list(outcomes) if isinstance(outcomes, list) else []
            failed_keys = {
                item.key for item in typed if item.status in TERMINAL_FAILURES
            }
            self.last_failed_variants = retry_variants(self.visible_variants, typed)
            self.retry_button.setEnabled(bool(self.last_failed_variants))
            self._render_error_report(typed, cancelled, evidence)
            self.worker = None
            self._set_busy(False)
            self._refresh_history()
            self.tabs.setCurrentIndex(2)
            self.result_tabs.setCurrentIndex(1 if failed_keys else 0)

        def _render_error_report(
            self, outcomes: Sequence[CaseOutcome], cancelled: bool, evidence: str
        ) -> None:
            failures = [item for item in outcomes if item.status in TERMINAL_FAILURES]
            known = [item for item in outcomes if item.status == "KNOWN_FAILURE"]
            counts = Counter(item.status for item in outcomes)
            lines = [
                "REPORTE CONDENSADO — NSC BUILDER 3",
                "=" * 88,
                f"Evidencia: {evidence}",
                f"Cancelado: {'sí' if cancelled else 'no'}",
                "Resultados: " + ", ".join(f"{key}={value}" for key, value in sorted(counts.items())),
                "",
            ]
            if failures:
                grouped: dict[str, list[CaseOutcome]] = defaultdict(list)
                for item in failures:
                    grouped[item.detail or item.status].append(item)
                lines.append(f"CAUSAS TERMINALES ÚNICAS: {len(grouped)}")
                for index, (cause, items) in enumerate(grouped.items(), start=1):
                    lines.extend(
                        [
                            "-" * 88,
                            f"CAUSA {index}/{len(grouped)} · {len(items)} ocurrencia(s)",
                            cause,
                        ]
                    )
                    lines.extend(f"  - {item.key}" for item in items)
            else:
                lines.append("No hubo fallos terminales.")
            if known:
                lines.extend(["", f"DEFECTOS CONOCIDOS OBSERVADOS: {len(known)}"])
                lines.extend(f"  - {item.key}: {item.detail}" for item in known)
            self.error_report.setPlainText("\n".join(lines))

        def _cancel_active(self) -> None:
            if self.worker is not None:
                self.worker.cancel()
                self.statusBar().showMessage("Cancelación solicitada al árbol activo")

        def _set_busy(self, busy: bool) -> None:
            self.compiler_configure_button.setEnabled(not busy)
            self.compiler_build_button.setEnabled(not busy)
            self.configure_button.setEnabled(not busy)
            self.build_button.setEnabled(not busy)
            self.run_button.setEnabled(not busy)
            self.preview_button.setEnabled(not busy)
            self.save_button.setEnabled(not busy)
            self.detect_button.setEnabled(not busy)
            self.receipt_button.setEnabled(not busy)
            infrastructure_busy = busy and bool(self.active_operation)
            self.infrastructure_cancel_button.setEnabled(infrastructure_busy)
            self.cancel_button.setEnabled(busy and not infrastructure_busy)
            if busy:
                self.retry_button.setEnabled(False)
            self.statusBar().showMessage("Operación activa" if busy else "Listo")

        def _refresh_history(self) -> None:
            if not HISTORY_ROOT.is_dir():
                self.history_table.setRowCount(0)
                return
            records: list[tuple[str, str, str, str, str]] = []
            for root in sorted(HISTORY_ROOT.iterdir(), reverse=True):
                if not root.is_dir():
                    continue
                records.append(history_record(root))
            self.history_table.setRowCount(len(records))
            for row, record in enumerate(records):
                for column, value in enumerate(record):
                    self.history_table.setItem(row, column, QTableWidgetItem(value))

        def _open_history(self) -> None:
            row = self.history_table.currentRow()
            if row < 0:
                return
            item = self.history_table.item(row, 4)
            if item is not None:
                QDesktopServices.openUrl(QUrl.fromLocalFile(item.text()))

        @staticmethod
        def _color_status(item: QTableWidgetItem, status: str) -> None:
            colors = {
                "PASS": QColor("#73d673"),
                "RUNNING": QColor("#8cb4ff"),
                "COMPILING": QColor("#e6c76a"),
                "KNOWN_FAILURE": QColor("#f0a35e"),
                "FAIL": QColor("#ff7777"),
                "BLOCKED": QColor("#d9955b"),
                "INFRASTRUCTURE_ERROR": QColor("#ff7777"),
                "UNEXPECTED_PASS": QColor("#ff7777"),
                "CANCELLED": QColor("#b7b7b7"),
                "PENDIENTE": QColor("#d9b85f"),
                "LISTO": QColor("#8cb4ff"),
            }
            color = colors.get(status)
            if color is not None:
                item.setForeground(color)

        def _show_error(self, title: str, error: Exception) -> None:
            QMessageBox.critical(self, title, str(error))

        def closeEvent(self, event: Any) -> None:
            if self.worker is not None:
                QMessageBox.warning(
                    self,
                    "Operación activa",
                    "Cancela la operación activa antes de cerrar NSC Builder 3.",
                )
                event.ignore()
                return
            event.accept()

    application = QApplication.instance() or QApplication(sys.argv)
    application.setApplicationName(APP_NAME)
    application.setStyle("Fusion")
    window = MainWindow()
    if smoke_only:
        window.close()
        return 0
    window.show()
    return application.exec()


def main(argv: Sequence[str] | None = None) -> int:
    """Expose GUI launch and non-mutating catalogue diagnostics from one script."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--audit-catalog",
        action="store_true",
        help="validate and print the complete Phase 2 catalogue without launching the GUI",
    )
    parser.add_argument(
        "--gui-smoke",
        action="store_true",
        help="construct and close the GUI without configuring, building or running tests",
    )
    arguments = parser.parse_args(argv)
    try:
        if arguments.audit_catalog:
            summary = catalog_audit()
            print(json.dumps(summary, indent=2, ensure_ascii=False, sort_keys=True))
            return 1 if summary["unclassified"] else 0
        return launch_gui(smoke_only=arguments.gui_smoke)
    except Exception as error:
        print(f"nscbuilder3: error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())


