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
DEFAULT_COMPILER_LLVM_ROOT = Path("C:/clang-llvm")
REQUIRED_LLVM_MAJOR = 23
LLVM_PRODUCER_NAMES = ("clang_cl", "lld_link", "llvm_rc", "llvm_mt")
WINDOWS_HIDDEN_PROCESS_FLAGS = getattr(subprocess, "CREATE_NO_WINDOW", 0)
WINDOWS_HIDDEN_PROCESS_GROUP_FLAGS = WINDOWS_HIDDEN_PROCESS_FLAGS | getattr(
    subprocess, "CREATE_NEW_PROCESS_GROUP", 0
)

PROFILE_RANK = {"PR": 0, "NIGHTLY": 1, "WEEKLY": 2}
TERMINAL_FAILURES = {
    "FAIL",
    "UNEXPECTED_PASS",
    "BLOCKED",
    "INFRASTRUCTURE_ERROR",
    "CANCELLED",
    "TIMEOUT",
}
RESULT_STATUSES = TERMINAL_FAILURES | {"PASS", "KNOWN_FAILURE"}
# ``run_suite.py`` publishes the full CaseStatus vocabulary in ``counts``.
# A focused execution cannot return SKIPPED as its selected result, but the
# canonical envelope still carries ``"SKIPPED": 0``. Keep that reporting-only
# status separate so execution routing does not silently start accepting skips.
IDENTITY_COUNT_STATUSES = RESULT_STATUSES | {"SKIPPED"}
NINJA_PROGRESS_PATTERN = re.compile(r"^\[(\d+)/(\d+)\]\s*(.*)$")
CONFIG_STRING_FIELDS = (
    "nsc_executable",
    "nslink_executable",
    "compiler_build_root",
    "install_prefix",
    "build_root",
    "compiler_llvm_root",
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
    {
        "nsc_component_tests",
        "nsc_component_tests_destructive",
        "nsc_component_tests_source_extended",
        "nsc_suite_controller+nsc",
        "legacy_historical_suite",
    }
)
ALLOCATION_ISOLATIONS = frozenset(
    {"RESET_PER_CASE", "FRESH_TEMP_PER_CASE", "LEGACY_OWNED"}
)
ALLOCATION_BUILD_MODES = frozenset({"Debug", "Release", "FROZEN_CONTRACT"})
ALLOCATION_OPTIMIZATIONS = frozenset(
    {"NA", "N/A", "O0", "O1", "O2", "O3", "FROZEN_CONTRACT"}
)


def utc_now() -> str:
    """Return an ISO-8601 timestamp used by durable evidence records."""
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def read_csv(path: Path) -> list[dict[str, str]]:
    """Read one extensible CSV using the canonical structural validation."""
    return read_catalog_csv(path, ())


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
            {"NA", "N/A", "FROZEN_CONTRACT"},
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
        creationflags=WINDOWS_HIDDEN_PROCESS_FLAGS if os.name == "nt" else 0,
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


def llvm_root_from_configured_path(configured: str | Path) -> Path:
    """Normalize either an LLVM root or its canonical LLVM_DIR to one root."""
    path = Path(configured).expanduser().resolve()
    lowered = tuple(part.casefold() for part in path.parts[-3:])
    if lowered == ("lib", "cmake", "llvm"):
        return path.parents[2]
    return path


def selected_llvm_root(llvm_root: str, llvm_dir: str) -> Path:
    """Select one explicit LLVM authority, with C:/clang-llvm as the only default."""
    configured = llvm_root or llvm_dir or str(DEFAULT_COMPILER_LLVM_ROOT)
    return llvm_root_from_configured_path(configured)


def _llvm_config_version(config_path: Path) -> str:
    """Read the package version published by the selected LLVMConfig.cmake."""
    text = config_path.read_text(encoding="utf-8", errors="replace")
    match = re.search(
        r"set\(LLVM_PACKAGE_VERSION\s+[\"']?(\d+(?:\.\d+)+)", text
    )
    if match is None:
        raise ValueError(
            f"LLVMConfig.cmake does not publish LLVM_PACKAGE_VERSION: {config_path}"
        )
    return match.group(1)


def _llvm_producer_identity(path: Path) -> str:
    """Query one LLVM producer without using PATH or another installation."""
    argument = "/?" if path.stem.casefold() in {"llvm-rc", "llvm-mt"} else "--version"
    completed = subprocess.run(
        [str(path), argument],
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        creationflags=WINDOWS_HIDDEN_PROCESS_FLAGS if sys.platform == "win32" else 0,
        check=False,
    )
    return (completed.stdout + "\n" + completed.stderr).strip()


def validate_llvm_toolchain(
    root: Path,
    producer_paths: dict[str, Path] | None = None,
    identity_reader: Callable[[Path], str] = _llvm_producer_identity,
) -> dict[str, str]:
    """Require one complete LLVM 23 toolchain rooted at one canonical directory."""
    root = root.expanduser().resolve()
    expected = {
        "clang_cl": root / "bin" / "clang-cl.exe",
        "lld_link": root / "bin" / "lld-link.exe",
        "llvm_rc": root / "bin" / "llvm-rc.exe",
        "llvm_mt": root / "bin" / "llvm-mt.exe",
    }
    producers = producer_paths or expected
    if set(producers) != set(LLVM_PRODUCER_NAMES):
        missing = sorted(set(LLVM_PRODUCER_NAMES) - set(producers))
        extra = sorted(set(producers) - set(LLVM_PRODUCER_NAMES))
        raise ValueError(
            "LLVM 23 producer set is incomplete"
            + (f"; missing: {', '.join(missing)}" if missing else "")
            + (f"; unexpected: {', '.join(extra)}" if extra else "")
        )
    resolved: dict[str, Path] = {}
    for name in LLVM_PRODUCER_NAMES:
        path = Path(producers[name]).expanduser().resolve()
        if path != expected[name].resolve():
            raise ValueError(
                f"LLVM 23 mixed-root producer rejected: {name}={path}; "
                f"expected {expected[name].resolve()}"
            )
        if not path.is_file():
            raise ValueError(f"LLVM 23 producer does not exist: {name}={path}")
        resolved[name] = path

    llvm_dir = root / "lib" / "cmake" / "llvm"
    llvm_config = llvm_dir / "LLVMConfig.cmake"
    if not llvm_config.is_file():
        raise ValueError(f"LLVM 23 configuration does not exist: {llvm_config}")
    config_version = _llvm_config_version(llvm_config)
    if int(config_version.split(".", 1)[0]) != REQUIRED_LLVM_MAJOR:
        raise ValueError(
            f"LLVM major {REQUIRED_LLVM_MAJOR} is required; "
            f"LLVMConfig.cmake reports {config_version} under {root}"
        )

    identities: dict[str, str] = {}
    identity_markers = {
        "clang_cl": "clang version",
        "lld_link": "LLD ",
        "llvm_rc": "LLVM Resource Converter",
        "llvm_mt": "Manifest Tool",
    }
    for name in LLVM_PRODUCER_NAMES:
        identity = identity_reader(resolved[name])
        if identity_markers[name].casefold() not in identity.casefold():
            raise ValueError(
                f"LLVM 23 producer identity is invalid: {name}={resolved[name]}"
            )
        version_match = re.search(
            r"\b(?:version\s+)?(\d+(?:\.\d+)+)\b", identity
        )
        version = version_match.group(1) if version_match else config_version
        if int(version.split(".", 1)[0]) != REQUIRED_LLVM_MAJOR:
            raise ValueError(
                f"LLVM major {REQUIRED_LLVM_MAJOR} is required; "
                f"{name} reports {version} at {resolved[name]}"
            )
        identities[f"{name}_version"] = version

    return {
        "llvm_root": str(root),
        "llvm_dir": str(llvm_dir.resolve()),
        "llvm_config": str(llvm_config.resolve()),
        "llvm_version": config_version,
        **{name: str(resolved[name]) for name in LLVM_PRODUCER_NAMES},
        **identities,
        "llvm_config_version": config_version,
    }


def discover_llvm_toolchain(configured: str | Path) -> dict[str, str]:
    """Resolve LLVM only below one configured root and validate its 23.x identity."""
    root = llvm_root_from_configured_path(configured or DEFAULT_COMPILER_LLVM_ROOT)
    return validate_llvm_toolchain(root)


def discover_build_tools(llvm_root: str) -> dict[str, str]:
    """Resolve build helpers and one strict, non-fallback LLVM 23 toolchain."""
    llvm_tools = discover_llvm_toolchain(llvm_root)
    tools = {
        "cmake": shutil.which("cmake") or "",
        "ninja": shutil.which("ninja") or "",
        "python": first_existing_file([sys.executable]),
        **llvm_tools,
    }
    missing = [name for name, value in tools.items() if not value]
    if missing:
        raise ValueError("required build tools were not found: " + ", ".join(missing))
    return tools


def discover_compiler_build_tools(llvm_root: str) -> dict[str, str]:
    """Resolve the NSC host toolchain from one validated LLVM 23 root."""
    llvm_tools = discover_llvm_toolchain(llvm_root)
    root = Path(llvm_tools["llvm_root"])
    tools = {
        "cmake": shutil.which("cmake") or "",
        "ninja": shutil.which("ninja") or "",
        **llvm_tools,
        "llvm_readobj": first_existing_file([root / "bin" / "llvm-readobj.exe"]),
        "llvm_core": first_existing_file([root / "lib" / "LLVMCore.lib"]),
        "llvm_support": first_existing_file([root / "lib" / "LLVMSupport.lib"]),
    }
    missing = [name for name, value in tools.items() if not value]
    if missing:
        raise ValueError("required NSC build tools were not found: " + ", ".join(missing))
    return tools


def discover_archive_executable(configured: str) -> str:
    """Find 7-Zip only from an explicit A-drive path or canonical A roots."""
    candidates: tuple[str | Path, ...] = (
        configured,
        Path("A:/apps/7zip/7z.exe"),
        Path("A:/7z/7z.exe"),
    )
    for candidate in candidates:
        if not candidate:
            continue
        raw_path = Path(candidate)
        if not is_a_drive(raw_path):
            continue
        path = raw_path.resolve()
        if path.is_file():
            return str(path)
    return ""


def first_distinct_existing_file(
    candidates: Iterable[str | Path], excluded: str | Path
) -> str:
    """Return the first existing file whose identity path differs from excluded."""
    excluded_path = Path(excluded).resolve()
    for candidate in candidates:
        if not candidate:
            continue
        path = Path(candidate).expanduser().resolve()
        if path.is_file() and path != excluded_path:
            return str(path)
    return ""


@dataclass(frozen=True)
class BuilderConfig:
    """Persist user-selected tools and output roots without implicit deployment."""

    nsc_executable: str = "A:/native-sharp-3.0-baseline/bin/nsc.exe"
    nslink_executable: str = "A:/native-sharp-3.0-baseline/bin/nslink.exe"
    compiler_build_root: str = "A:/nsc-devel/build-clang"
    install_prefix: str = "A:/native-sharp-3.0-baseline"
    build_root: str = "A:/nsc-devel/build-compiler-suite-v3"
    compiler_llvm_root: str = ""
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
        compiler_build_root = self.compiler_build_root
        if Path(compiler_build_root).name.casefold() == "build-clang-v3":
            compiler_build_root = "A:/nsc-devel/build-clang"
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
        compiler_llvm_root_path = selected_llvm_root(
            self.compiler_llvm_root, self.llvm_dir
        )
        compiler_llvm_root = str(compiler_llvm_root_path)
        llvm_dir = str(compiler_llvm_root_path / "lib" / "cmake" / "llvm")
        archive = discover_archive_executable(self.archive_executable)
        comparison_nsc = first_distinct_existing_file(
            [
                self.comparison_nsc,
                "A:/native-sharp-3.0-baseline/bin/nsc.exe",
                "A:/nsc-devel/build-clang/bin/nsc.exe",
            ],
            nsc,
        )
        build_id_header = first_existing_file(
            [
                self.build_id_header,
                Path(compiler_build_root) / "generated" / "nativesharp" / "Basic" / "BuildId.h",
            ]
        )
        return BuilderConfig(
            nsc_executable=nsc,
            nslink_executable=nslink,
            compiler_build_root=compiler_build_root,
            install_prefix=self.install_prefix,
            build_root=self.build_root,
            compiler_llvm_root=compiler_llvm_root,
            llvm_dir=llvm_dir,
            archive_executable=archive,
            comparison_nsc=comparison_nsc,
            build_id_header=build_id_header,
            jobs=self.jobs,
        )

    def save(self) -> None:
        """Persist settings under the repository-owned Builder 3 directory."""
        payload_source = asdict(self)
        llvm_root = self.llvm_root_path()
        payload_source["compiler_llvm_root"] = str(llvm_root)
        payload_source["llvm_dir"] = str(llvm_root / "lib" / "cmake" / "llvm")
        payload = parse_builder_config_payload(payload_source)
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
            llvm_root = self.llvm_root_path()
            require_directory_path(llvm_root, "NSC LLVM 23 root", must_exist=True)
            require_directory_path(
                llvm_root / "lib" / "cmake" / "llvm",
                "derived LLVM_DIR",
                must_exist=True,
            )
            discover_llvm_toolchain(self.llvm_root_path())
        return self

    def llvm_root_path(self) -> Path:
        """Return the single LLVM root that owns tools and LLVMConfig.cmake."""
        return selected_llvm_root(self.compiler_llvm_root, self.llvm_dir)


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
        return self.manifest is not None


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
    profile: str = ""
    timeout_pass_count: int = 0
    timeout_average_seconds: float | None = None
    timeout_limit_seconds: int | None = None


@dataclass(frozen=True)
class VariantTimeoutBudget:
    """One per-variant deadline derived exclusively from trusted PASS history."""

    pass_count: int
    average_seconds: float | None
    limit_seconds: int


def variant_history_key(variant: RunVariant) -> tuple[str, str, str, str]:
    """Include the manifest profile in addition to the concrete variant identity."""
    return (
        variant.case.case_id,
        variant.build_mode,
        variant.optimization,
        variant.case.profile,
    )


def timeout_limit_for_average(average_seconds: float | None) -> int:
    """Return the PASS-average deadline, always rounded upward to whole seconds.

    No history gets 45 minutes. Averages through 30 seconds add five seconds
    with a 15-second floor, through 120 add fifteen, through ten minutes add
    three minutes, and longer averages add the greater of five minutes or
    twenty percent. Upward rounding never reduces those margins.
    """
    if average_seconds is None:
        return 45 * 60
    if (
        isinstance(average_seconds, bool)
        or not isinstance(average_seconds, (int, float))
        or not math.isfinite(average_seconds)
        or average_seconds < 0
    ):
        raise ValueError("historical PASS average must be a finite nonnegative number")
    average = float(average_seconds)
    if average <= 30.0:
        return max(15, math.ceil(average + 5.0))
    if average <= 120.0:
        return math.ceil(average + 15.0)
    if average <= 600.0:
        return math.ceil(average + 180.0)
    return math.ceil(average + max(300.0, average * 0.20))


def load_variant_timeout_budgets(
    variants: Sequence[RunVariant], history_root: Path = HISTORY_ROOT
) -> dict[tuple[str, str, str, str], VariantTimeoutBudget]:
    """Load all trustworthy PASS durations once and calculate every requested budget."""
    requested = {variant_history_key(variant) for variant in variants}
    samples: dict[tuple[str, str, str, str], list[float]] = {
        key: [] for key in requested
    }
    profiles_by_identity = {
        (case_id, build_mode, optimization): profile
        for case_id, build_mode, optimization, profile in requested
    }
    if history_root.is_dir():
        for root in history_root.iterdir():
            if not root.is_dir():
                continue
            record = history_record(root)
            if (
                record[1] != "phase-2-suite"
                or record[2] in {"INCOMPLETE", "MALFORMED"}
            ):
                continue
            try:
                payload = json.loads(
                    (root / "results.json").read_text(encoding="utf-8"),
                    object_pairs_hook=unique_json_object,
                )
            except (OSError, json.JSONDecodeError, TypeError, ValueError):
                continue
            rows = payload.get("results") if isinstance(payload, dict) else None
            if not isinstance(rows, list):
                continue
            for row in rows:
                if not isinstance(row, dict) or row.get("status") != "PASS":
                    continue
                identity = (
                    row.get("case_id"),
                    row.get("build_mode"),
                    row.get("optimization"),
                )
                profile = profiles_by_identity.get(identity)
                recorded_profile = row.get("profile")
                duration = row.get("duration_seconds")
                if (
                    profile is None
                    or (
                        recorded_profile is not None
                        and recorded_profile != profile
                    )
                    or isinstance(duration, bool)
                    or not isinstance(duration, (int, float))
                    or not math.isfinite(duration)
                    or duration < 0
                ):
                    continue
                samples[(*identity, profile)].append(float(duration))
    budgets: dict[tuple[str, str, str, str], VariantTimeoutBudget] = {}
    for key, durations in samples.items():
        average = sum(durations) / len(durations) if durations else None
        budgets[key] = VariantTimeoutBudget(
            pass_count=len(durations),
            average_seconds=average,
            limit_seconds=timeout_limit_for_average(average),
        )
    return budgets


@dataclass(frozen=True)
class HistoricalExecution:
    """Validated read-only projection of one completed Phase 2 suite run."""

    root: Path
    metadata: dict[str, Any]
    status: str
    cases: tuple[CaseRecord, ...]
    variants: tuple[RunVariant, ...]
    outcomes: tuple[CaseOutcome, ...]
    full_log: str


def case_diagnostic_summary(
    case: CaseRecord,
    variant: RunVariant | None = None,
    outcome: CaseOutcome | None = None,
) -> str:
    """Render the stable test contract and its observed result for a human reader."""
    manifest = case.manifest or {}
    dependencies = manifest.get("dependencies", [])
    tags = manifest.get("tags", [])
    lines = [
        "RESUMEN DE LA PRUEBA",
        "=" * 80,
        f"Caso              : {case.case_id}",
        f"Contrato          : {case.contract_id}",
        f"Componente        : {case.component}",
        f"Ola               : {case.wave}",
        f"Objetivo          : {case.objective}",
        f"Propósito         : {case.purpose}",
        f"Capa              : {case.layer}",
        f"Clase de escenario: {manifest.get('class', 'no declarada')}",
        f"Etiquetas         : {', '.join(str(tag) for tag in tags) or 'ninguna'}",
        f"Dependencias      : {', '.join(str(item) for item in dependencies) or 'ninguna'}",
        f"Aislamiento       : {manifest.get('isolation', 'no declarado')}",
        f"Ejecutor          : {case.executor}",
        f"Perfil            : {case.profile}",
        f"Disposición       : {case.disposition}",
        f"Resultado esperado: {case.expected_outcome or 'definido por el oráculo'}",
    ]
    if variant is not None:
        lines.extend(
            (
                "",
                "CONFIGURACIÓN EJECUTADA",
                "-" * 80,
                f"Build             : {variant.build_mode}",
                f"Optimización      : {variant.optimization}",
                f"Identidad exacta  : {variant.key}",
            )
        )
    if outcome is not None:
        lines.extend(
            (
                "",
                "RESULTADO OBSERVADO",
                "-" * 80,
                f"Estado            : {outcome.status}",
                f"Duración          : {outcome.duration_seconds:.6f} segundos",
                f"Diagnóstico       : {outcome.detail or 'sin diagnóstico textual'}",
                f"Evidencia         : {outcome.evidence or 'sin archivo de evidencia'}",
            )
        )
    else:
        lines.extend(
            (
                "",
                "RESULTADO OBSERVADO",
                "-" * 80,
                "Estado            : la prueba todavía no tiene un resultado guardado",
            )
        )
    return "\n".join(lines) + "\n"


def case_contract_payload(case: CaseRecord) -> dict[str, Any]:
    """Project the exact declared scenario without interpreting its oracle."""
    manifest = case.manifest or {}
    return {
        "case_id": case.case_id,
        "contract_id": case.contract_id,
        "component": case.component,
        "objective": case.objective,
        "dependencies": manifest.get("dependencies", []),
        "inputs": manifest.get("inputs"),
        "oracle": manifest.get("oracle"),
    }


def case_evidence_output(
    outcome: CaseOutcome | None, run_root: Path | None = None
) -> str:
    """Read the complete per-case textual evidence without changing the saved run."""
    if outcome is None or not outcome.evidence:
        return "La prueba no tiene un archivo de evidencia registrado.\n"
    evidence_path = Path(outcome.evidence)
    if not evidence_path.is_absolute() and run_root is not None:
        evidence_path = run_root / evidence_path
    candidates: list[Path] = []
    if evidence_path.is_file():
        candidates.append(evidence_path)
        candidates.extend(
            path
            for path in sorted(evidence_path.parent.glob("*.log"))
            if path != evidence_path
        )
    elif evidence_path.is_dir():
        candidates.extend(sorted(evidence_path.glob("*.log")))
    if not candidates:
        return f"La evidencia registrada no está disponible: {evidence_path}\n"
    sections: list[str] = []
    for path in candidates:
        try:
            contents = path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            contents = f"No se pudo leer el archivo: {error}\n"
        separator = "" if contents.endswith("\n") else "\n"
        sections.append(
            f"ARCHIVO: {path}\n{'-' * 80}\n{contents}{separator}"
        )
    return "\n".join(sections)


class CanonicalCatalog:
    """Join the executable Phase 2 design, allocation, and manifest inventories."""

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
        cls._validate_sources(design, allocation_rows, manifest_rows, waves)
        allocation = {row["case_id"]: row for row in allocation_rows}
        manifest = {row["case_id"]: row for row in manifest_rows}
        cases: list[CaseRecord] = []
        for row in design:
            case_id = row["case_id"]
            assigned = allocation[case_id]
            executable = manifest.get(case_id)
            canonical = validated_by_id.get(case_id)
            disposition = "Executable"
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
        """Normalize allocation optimizations for display."""
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
        missing_manifest = sorted(design_set - set(manifest_ids))
        if missing_manifest:
            raise ValueError(f"manifest is missing {len(missing_manifest)} canonical cases")

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
    captured_output: str = "",
) -> CaseOutcome:
    """Project one infrastructure command into the shared terminal protocol."""
    status = "CANCELLED" if cancelled else ("PASS" if exit_code == 0 else "FAIL")
    if cancelled:
        detail = "operation cancelled"
    elif exit_code != 0:
        detail = infrastructure_failure_detail(exit_code, captured_output)
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


def infrastructure_failure_detail(exit_code: int, captured_output: str) -> str:
    """Keep one bounded causal diagnostic while full.log retains all output."""
    prefix = f"exit code {exit_code}"
    lines = [line.strip() for line in captured_output.splitlines() if line.strip()]
    for index, line in enumerate(lines):
        folded = line.casefold()
        if not (
            folded.startswith("cmake error")
            or ": error:" in folded
            or ": error " in folded
            or "fatal error" in folded
            or folded.startswith("ninja: error:")
        ):
            continue
        diagnostic = line
        if folded.startswith("cmake error") and index + 1 < len(lines):
            continuation = lines[index + 1]
            if not continuation.casefold().startswith("call stack"):
                diagnostic = f"{diagnostic} — {continuation}"
        return f"{prefix}: {diagnostic}"[:800]
    return prefix


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


def blocked_infrastructure_outcomes(
    commands: Sequence[tuple[str, Sequence[str], Path]],
    first_unstarted_index: int,
    evidence: str,
    failed_label: str,
) -> tuple[CaseOutcome, ...]:
    """Close every command whose prerequisite infrastructure command failed."""
    if not 1 <= first_unstarted_index <= len(commands) + 1:
        raise ValueError("first unstarted command index is outside the command plan")
    outcomes: list[CaseOutcome] = []
    for index in range(first_unstarted_index, len(commands) + 1):
        label = commands[index - 1][0]
        outcomes.append(
            CaseOutcome(
                key=f"INFRA|{index}",
                case_id=label,
                contract_id="NSCBUILDER3-INFRASTRUCTURE",
                component="TestInfrastructure",
                wave=0,
                build_mode="NA",
                optimization="NA",
                status="BLOCKED",
                detail=f"not started because prerequisite failed: {failed_label}",
                evidence=evidence,
            )
        )
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


def active_retry_variants(
    failed: Sequence[RunVariant], disabled_keys: Iterable[str]
) -> tuple[RunVariant, ...]:
    """Return exact failed variants not temporarily disabled by the operator."""
    by_key = {variant.key: variant for variant in failed}
    if len(by_key) != len(failed):
        raise ValueError("failed retry plan contains duplicate variant keys")
    disabled = set(disabled_keys)
    unknown = sorted(disabled - set(by_key))
    if unknown:
        raise ValueError("disabled retry variants are outside the failed plan: " + ", ".join(unknown))
    return tuple(variant for variant in failed if variant.key not in disabled)


def row_progress_text(status: str) -> str:
    """Project one execution state into a compact per-row progress value."""
    if status == "LISTO":
        return "0%"
    if status in {"RUNNING", "COMPILING"}:
        return "EN CURSO"
    if status in RESULT_STATUSES or status == "DESACTIVADA":
        return "100%"
    if status == "PENDIENTE":
        return "NO EJECUTABLE"
    if status == "FILTRADO":
        return "FUERA"
    return ""


def row_action_text(status: str, executable: bool) -> str:
    """Describe the exact-row action without implying a broader selection."""
    if not executable:
        return ""
    if status == "RUNNING":
        return "Cancelar prueba"
    if status == "COMPILING":
        return "En ejecución"
    if status in RESULT_STATUSES:
        return "Volver a correr"
    return "Ejecutar"


def status_filter_matches(filter_name: str, status: str) -> bool:
    """Match one row status without changing the underlying execution plan."""
    if filter_name == "ALL":
        return True
    if filter_name == "FAILURES":
        return status in TERMINAL_FAILURES
    if filter_name == "ACTIVE":
        return status in {"RUNNING", "COMPILING"}
    if filter_name == "WAITING":
        return status in {"LISTO", "PENDIENTE"}
    return status == filter_name


def condensed_error_report(
    outcomes: Sequence[CaseOutcome], cancelled: bool, evidence: str
) -> str:
    """Render one causal report shared by infrastructure and suite closures."""
    failures = [item for item in outcomes if item.status in TERMINAL_FAILURES]
    known = [item for item in outcomes if item.status == "KNOWN_FAILURE"]
    counts = Counter(item.status for item in outcomes)
    controller_failure = evidence.startswith("ERROR:")
    lines = [
        "REPORTE CONDENSADO — NSC BUILDER 3",
        "=" * 88,
        f"Evidencia: {evidence}",
        f"Cancelado: {'sí' if cancelled else 'no'}",
        "Resultados: "
        + ", ".join(f"{key}={value}" for key, value in sorted(counts.items())),
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
    elif controller_failure:
        lines.extend(
            [
                "CAUSAS TERMINALES ÚNICAS: 1",
                "-" * 88,
                "CAUSA 1/1 · fallo del controlador",
                evidence,
            ]
        )
    else:
        lines.append("No hubo fallos terminales.")
    if known:
        lines.extend(["", f"DEFECTOS CONOCIDOS OBSERVADOS: {len(known)}"])
        lines.extend(f"  - {item.key}: {item.detail}" for item in known)
    return "\n".join(lines)


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


def load_history_execution(
    root: Path, catalog: CanonicalCatalog
) -> HistoricalExecution:
    """Load one completed suite run without mutating or replacing its evidence."""
    record = history_record(root)
    if record[1] != "phase-2-suite":
        raise ValueError("the selected history row is not a Phase 2 suite execution")
    if record[2] in {"INCOMPLETE", "MALFORMED"}:
        raise ValueError(f"the selected execution is {record[2].lower()}")
    metadata = json.loads(
        (root / "metadata.json").read_text(encoding="utf-8"),
        object_pairs_hook=unique_json_object,
    )
    payload = json.loads(
        (root / "results.json").read_text(encoding="utf-8"),
        object_pairs_hook=unique_json_object,
    )
    if not isinstance(metadata, dict) or not isinstance(payload, dict):
        raise ValueError("historical execution envelopes must be objects")
    selection = metadata.get("selection")
    selection_fields = {
        "scope",
        "value",
        "profile",
        "build_mode",
        "optimization",
    }
    if not isinstance(selection, dict) or set(selection) != selection_fields:
        raise ValueError("historical execution has an invalid selection envelope")
    if any(not isinstance(selection[field], str) for field in selection_fields):
        raise ValueError("historical execution selection fields must be text")
    scope = selection["scope"]
    value = selection["value"]
    profile = selection["profile"]
    build_mode = selection["build_mode"]
    optimization = selection["optimization"]
    if scope not in {"Total", "Ola", "Componente", "Contrato", "Caso"}:
        raise ValueError(f"historical execution has an unknown scope: {scope!r}")
    if scope != "Total" and not value:
        raise ValueError("historical non-total scope requires a value")
    if scope == "Total" and value:
        raise ValueError("historical total scope cannot have a value")
    if profile not in {"Todos", *PROFILE_RANK}:
        raise ValueError(f"historical execution has an unknown profile: {profile!r}")
    if build_mode not in {"Debug", "Release", "Todos"}:
        raise ValueError(
            f"historical execution has an unknown build mode: {build_mode!r}"
        )
    if optimization not in {"O0", "O1", "O2", "O3", "Todas"}:
        raise ValueError(
            "historical execution has an unknown optimization: "
            f"{optimization!r}"
        )
    selected_cases = catalog.select(scope, value, profile)
    if not selected_cases:
        raise ValueError(
            "historical case selection is absent from the current catalogue"
        )
    selected_variants = expand_variants(
        selected_cases, build_mode, optimization
    )
    if not selected_variants:
        raise ValueError("historical selection has no executable variants")
    expected_case_count = metadata.get("case_count")
    executable_case_count = len(
        {variant.case.case_id for variant in selected_variants}
    )
    if (
        isinstance(expected_case_count, bool)
        or not isinstance(expected_case_count, int)
        or expected_case_count != executable_case_count
    ):
        raise ValueError(
            "historical case_count does not match its executable selection"
        )
    rows = payload.get("results")
    if not isinstance(rows, list):
        raise ValueError("historical execution has no terminal result rows")
    variants: list[RunVariant] = []
    outcomes: list[CaseOutcome] = []
    for index, raw in enumerate(rows, start=1):
        if not isinstance(raw, dict):
            raise ValueError(f"historical result {index} must be an object")
        base_fields = {
            "key",
            "case_id",
            "contract_id",
            "component",
            "wave",
            "build_mode",
            "optimization",
            "status",
            "duration_seconds",
            "detail",
            "evidence",
        }
        optional_fields = {
            "profile",
            "timeout_pass_count",
            "timeout_average_seconds",
            "timeout_limit_seconds",
        }
        raw_fields = set(raw)
        if not base_fields <= raw_fields or not raw_fields <= base_fields | optional_fields:
            raise ValueError(f"historical result {index} has an invalid schema")
        text_fields = (
            "key",
            "case_id",
            "contract_id",
            "component",
            "build_mode",
            "optimization",
            "status",
            "detail",
            "evidence",
        )
        if any(not isinstance(raw[field], str) for field in text_fields):
            raise ValueError(f"historical result {index} has a non-text field")
        if any(
            not raw[field]
            for field in (
                "key",
                "case_id",
                "contract_id",
                "component",
                "build_mode",
                "optimization",
                "status",
            )
        ):
            raise ValueError(f"historical result {index} has a blank identity field")
        wave = raw["wave"]
        duration = raw["duration_seconds"]
        if isinstance(wave, bool) or not isinstance(wave, int) or wave < 0:
            raise ValueError(f"historical result {index} has an invalid wave")
        if (
            isinstance(duration, bool)
            or not isinstance(duration, (int, float))
            or not math.isfinite(duration)
            or duration < 0
        ):
            raise ValueError(f"historical result {index} has an invalid duration")
        if raw["status"] not in RESULT_STATUSES:
            raise ValueError(f"historical result {index} has an invalid status")
        recorded_profile = raw.get("profile")
        if recorded_profile is not None and (
            not isinstance(recorded_profile, str) or not recorded_profile
        ):
            raise ValueError(f"historical result {index} has an invalid profile")
        timeout_pass_count = raw.get("timeout_pass_count", 0)
        timeout_average = raw.get("timeout_average_seconds")
        timeout_limit = raw.get("timeout_limit_seconds")
        if (
            isinstance(timeout_pass_count, bool)
            or not isinstance(timeout_pass_count, int)
            or timeout_pass_count < 0
        ):
            raise ValueError(f"historical result {index} has an invalid PASS count")
        if timeout_average is not None and (
            isinstance(timeout_average, bool)
            or not isinstance(timeout_average, (int, float))
            or not math.isfinite(timeout_average)
            or timeout_average < 0
        ):
            raise ValueError(f"historical result {index} has an invalid PASS average")
        if timeout_limit is not None and (
            isinstance(timeout_limit, bool)
            or not isinstance(timeout_limit, int)
            or timeout_limit < 1
        ):
            raise ValueError(f"historical result {index} has an invalid timeout limit")
        case = catalog.by_id.get(raw["case_id"])
        if case is None:
            raise ValueError(
                f"historical case is absent from the current catalogue: {raw['case_id']}"
            )
        if (
            case.contract_id != raw["contract_id"]
            or case.component != raw["component"]
            or case.wave != wave
        ):
            raise ValueError(
                f"historical case identity changed in the current catalogue: {case.case_id}"
            )
        if "profile" in raw and recorded_profile != case.profile:
            raise ValueError(
                f"historical case profile changed in the current catalogue: {case.case_id}"
            )
        variant = RunVariant(case, raw["build_mode"], raw["optimization"])
        if not case.executable or variant.build_mode not in case.build_modes:
            raise ValueError(f"historical variant is no longer executable: {variant.key}")
        if variant.optimization not in case.optimizations or variant.key != raw["key"]:
            raise ValueError(f"historical variant identity is incompatible: {raw['key']}")
        variants.append(variant)
        outcomes.append(
            CaseOutcome(
                key=raw["key"],
                case_id=raw["case_id"],
                contract_id=raw["contract_id"],
                component=raw["component"],
                wave=wave,
                build_mode=raw["build_mode"],
                optimization=raw["optimization"],
                status=raw["status"],
                duration_seconds=float(duration),
                detail=raw["detail"],
                evidence=raw["evidence"],
                profile=case.profile,
                timeout_pass_count=timeout_pass_count,
                timeout_average_seconds=(
                    float(timeout_average) if timeout_average is not None else None
                ),
                timeout_limit_seconds=timeout_limit,
            )
        )
    expected_keys = tuple(variant.key for variant in selected_variants)
    actual_keys = tuple(outcome.key for outcome in outcomes)
    if Counter(actual_keys) != Counter(expected_keys):
        raise ValueError(
            "historical terminal rows do not reproduce the original selection"
        )
    log_path = root / "full.log"
    full_log = log_path.read_text(encoding="utf-8", errors="replace") if log_path.is_file() else ""
    return HistoricalExecution(
        root=root.resolve(),
        metadata=metadata,
        status=record[2],
        cases=selected_cases,
        variants=selected_variants,
        outcomes=tuple(outcomes),
        full_log=full_log,
    )


class ProcessController:
    """Run one child process with streamed output and scoped tree cancellation."""

    def __init__(
        self,
        output: Callable[[str], None],
        activity: Callable[[float, str], None] | None = None,
    ) -> None:
        self.output = output
        self.activity = activity
        self.cancelled = threading.Event()
        self.last_termination_reason: str | None = None
        self._lock = threading.Lock()
        self._termination_lock = threading.Lock()
        self._process: subprocess.Popen[str] | None = None
        self._variant_active = False
        self._variant_generation = 0
        self._variant_stop = threading.Event()
        self._variant_stop_reason: str | None = None

    def begin_variant(self) -> int:
        """Open a fresh cancellation scope that cannot leak into the next variant."""
        with self._lock:
            if self._variant_active:
                raise RuntimeError("a variant cancellation scope is already active")
            self._variant_generation += 1
            self._variant_active = True
            self._variant_stop.clear()
            self._variant_stop_reason = None
            self.last_termination_reason = None
            return self._variant_generation

    def end_variant(self, generation: int) -> str | None:
        """Close one exact scope and return its first terminal stop reason."""
        with self._lock:
            if not self._variant_active or generation != self._variant_generation:
                raise RuntimeError("variant cancellation scope identity mismatch")
            reason = self._variant_stop_reason
            self._variant_active = False
            self._variant_stop.clear()
            self._variant_stop_reason = None
            return reason

    def cancel_current(
        self, reason: str = "case-cancelled", generation: int | None = None
    ) -> bool:
        """Stop only the active variant tree and leave global cancellation clear."""
        if reason not in {"case-cancelled", "timeout", "cancelled"}:
            raise ValueError(f"unsupported variant stop reason: {reason}")
        with self._lock:
            if (
                not self._variant_active
                or (generation is not None and generation != self._variant_generation)
                or self._variant_stop_reason is not None
            ):
                return False
            self._variant_stop_reason = reason
            self.last_termination_reason = reason
            self._variant_stop.set()
            process = self._process
        if process is not None:
            self._terminate_owned(process)
        return True

    def _requested_stop_reason(self) -> str | None:
        """Read global or current-variant stop state without consuming either."""
        if self.cancelled.is_set():
            return "cancelled"
        if self._variant_stop.is_set():
            with self._lock:
                return self._variant_stop_reason
        return None

    def run(
        self,
        command: Sequence[str],
        cwd: Path,
        environment: dict[str, str] | None = None,
    ) -> tuple[int, str]:
        """Stream merged output and return exit code plus captured text."""
        stop_reason = self._requested_stop_reason()
        if stop_reason is not None:
            self.last_termination_reason = stop_reason
            message = f"{stop_reason} before launch\n"
            self.output(message)
            return 130, message
        flags = WINDOWS_HIDDEN_PROCESS_GROUP_FLAGS if os.name == "nt" else 0
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
        last_activity = started
        last_line = ""
        assert process.stdout is not None

        def read_output() -> None:
            """Drain the child pipe without blocking cancellation checks."""
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
                    last_line = line.strip() or last_line
            except queue.Empty:
                pass
            now = time.monotonic()
            if self.activity is not None and now - last_activity >= 0.25:
                self.activity(now - started, last_line)
                last_activity = now
            if process.poll() is not None and stream_closed and pending.empty():
                break
            stop_reason = self._requested_stop_reason()
            if stop_reason is not None:
                self.last_termination_reason = stop_reason
                self._terminate_owned(process)
                message = f"process stopped: {stop_reason}\n"
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
    ) -> subprocess.CompletedProcess[str]:
        """Run one owned child while preserving stdout and stderr separately."""
        arguments = [str(part) for part in command]
        stop_reason = self._requested_stop_reason()
        if stop_reason is not None:
            self.last_termination_reason = stop_reason
            message = f"{stop_reason} before launch\n"
            self.output(message)
            return subprocess.CompletedProcess(arguments, 130, "", message)
        flags = WINDOWS_HIDDEN_PROCESS_GROUP_FLAGS if os.name == "nt" else 0
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
        last_activity = started
        last_line = ""
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
                    last_line = line.strip() or last_line
            except queue.Empty:
                pass
            now = time.monotonic()
            if self.activity is not None and now - last_activity >= 0.25:
                self.activity(now - started, last_line)
                last_activity = now
            if process.poll() is not None and len(closed) == 2 and pending.empty():
                break
            stop_reason = self._requested_stop_reason()
            if stop_reason is not None:
                self.last_termination_reason = stop_reason
                self._terminate_owned(process)
                message = f"process stopped: {stop_reason}\n"
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
        """Cancel the complete operation and its currently owned child tree."""
        self.last_termination_reason = "cancelled"
        self.cancelled.set()
        with self._lock:
            if self._variant_active and self._variant_stop_reason is None:
                self._variant_stop_reason = "cancelled"
                self._variant_stop.set()
            process = self._process
        if process is not None:
            self._terminate_owned(process)

    def _terminate_owned(self, process: subprocess.Popen[str]) -> None:
        """Serialize concurrent UI, timer, and polling termination requests."""
        with self._termination_lock:
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
                creationflags=WINDOWS_HIDDEN_PROCESS_FLAGS,
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


@dataclass(frozen=True)
class TestTreeCleanupResult:
    """Summarize one bounded cleanup of generated test-build directories."""

    targets: tuple[Path, ...]
    missing_targets: tuple[Path, ...]
    removed_files: int
    removed_directories: int
    failures: tuple[str, ...]


def _has_reparse_attribute(path: Path) -> bool:
    """Detect Windows junctions and other reparse points without following them."""
    attributes = getattr(os.lstat(path), "st_file_attributes", 0)
    return bool(attributes & 0x400)


def _ensure_plain_path(path: Path, label: str) -> None:
    """Reject symlinks or junctions in an existing path before destructive work."""
    existing: list[Path] = []
    current = path
    while True:
        if current.exists() or current.is_symlink():
            existing.append(current)
        if current.parent == current:
            break
        current = current.parent
    for candidate in reversed(existing):
        if candidate.is_symlink() or _has_reparse_attribute(candidate):
            raise ValueError(f"{label} crosses a symbolic link or junction: {candidate}")


def _paths_overlap(left: Path, right: Path) -> bool:
    """Return whether either absolute path contains the other."""
    return left == right or left in right.parents or right in left.parents


def test_tree_cleanup_targets(config: BuilderConfig) -> tuple[Path, ...]:
    """Return the only test-build roots that Builder 3 is allowed to remove."""
    raw_root = Path(os.path.abspath(config.build_root))
    if not is_a_drive(raw_root):
        raise ValueError(f"test build root must be on A:, got {raw_root}")
    if raw_root.parent == raw_root:
        raise ValueError("test build root cannot be a drive root")
    _ensure_plain_path(raw_root, "test build root")
    root = raw_root.resolve()
    targets = tuple(root / mode.casefold() for mode in ("Debug", "Release"))

    protected = (
        SUITE_ROOT.resolve(),
        STORAGE_ROOT.resolve(),
        compiler_build_dir(config, "Debug").resolve(),
        compiler_build_dir(config, "Release").resolve(),
        Path(config.install_prefix).resolve(),
        Path(config.nsc_executable).resolve().parent,
        Path(config.nslink_executable).resolve().parent,
    )
    for target in targets:
        if target.parent != root:
            raise ValueError(f"test cleanup target escaped its configured root: {target}")
        if target == SOURCE_ROOT or target in SOURCE_ROOT.parents:
            raise ValueError(f"test cleanup target contains the source tree: {target}")
        if any(_paths_overlap(target, reserved) for reserved in protected):
            raise ValueError(f"test cleanup target overlaps a protected path: {target}")
        _ensure_plain_path(target, "test cleanup target")
    return targets


def clean_test_build_trees(config: BuilderConfig) -> TestTreeCleanupResult:
    """Delete selected generated trees with an explicit iterative post-order walk."""
    targets = test_tree_cleanup_targets(config)
    removed_files = 0
    removed_directories = 0
    missing: list[Path] = []
    failures: list[str] = []

    for root in targets:
        if not root.exists():
            missing.append(root)
            continue
        stack: list[tuple[Path, bool]] = [(root, False)]
        while stack:
            path, visited = stack.pop()
            try:
                if visited:
                    path.rmdir()
                    removed_directories += 1
                    continue
                if path.is_symlink() or _has_reparse_attribute(path):
                    attributes = getattr(os.lstat(path), "st_file_attributes", 0)
                    if attributes & 0x10:
                        path.rmdir()
                        removed_directories += 1
                    else:
                        path.unlink()
                        removed_files += 1
                    continue
                if not path.is_dir():
                    path.unlink()
                    removed_files += 1
                    continue
                stack.append((path, True))
                with os.scandir(path) as entries:
                    children = [Path(entry.path) for entry in entries]
                stack.extend((child, False) for child in children)
            except OSError as error:
                failures.append(f"{path}: {error}")

    return TestTreeCleanupResult(
        targets=tuple(targets),
        missing_targets=tuple(missing),
        removed_files=removed_files,
        removed_directories=removed_directories,
        failures=tuple(failures),
    )


def compiler_build_dir(config: BuilderConfig, mode: str) -> Path:
    """Keep Debug and Release compiler products in independent build trees."""
    release = Path(config.compiler_build_root).resolve()
    if mode == "Release":
        return release
    return release.parent / f"{release.name}-debug"


def compiler_output(config: BuilderConfig, mode: str) -> Path:
    """Return the NSC artifact produced by one compiler configuration."""
    return compiler_build_dir(config, mode) / "bin" / "nsc.exe"


def compiler_runtime_library(config: BuilderConfig) -> Path:
    """Return the runtime produced with the selected compiler identity."""
    if config.build_id_header:
        build_id_header = Path(config.build_id_header).resolve()
        if len(build_id_header.parents) < 3:
            raise ValueError(
                "El encabezado BuildId no permite identificar su directorio generated"
            )
        generated_root = build_id_header.parents[2]
    else:
        selected_nsc = Path(config.nsc_executable).resolve()
        generated_root = selected_nsc.parent.parent / "generated"
    return generated_root / "NativeSharp.Runtime.Win32.lib"


def component_test_executable(config: BuilderConfig, mode: str, target: str) -> Path:
    """Locate one named component-test executable in supported CMake layouts."""
    root = effective_build_dir(config, mode)
    candidates = (
        root / f"{target}.exe",
        root / mode / f"{target}.exe",
        root / "bin" / f"{target}.exe",
        root / "bin" / mode / f"{target}.exe",
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return candidates[0]


def host_executable(config: BuilderConfig, mode: str) -> Path:
    """Locate the ordinary consolidated host executable."""
    return component_test_executable(config, mode, "nsc_component_tests")


def destructive_host_executable(config: BuilderConfig, mode: str) -> Path:
    """Locate the isolated five-case destructive Host.AST executable."""
    return component_test_executable(config, mode, "nsc_component_tests_destructive")


def expected_abnormal_termination_detail(
    case: CaseRecord, exit_code: int, captured: str
) -> str | None:
    """Recognize only a manifest-authorized known process termination."""
    manifest = case.manifest or {}
    oracle = manifest.get("oracle")
    if not isinstance(oracle, dict):
        return None
    abnormal = oracle.get("abnormal_termination")
    if abnormal is None:
        return None
    if case.expected_outcome != "KNOWN_FAILURE" or case.disposition != "KnownProductDefect":
        raise ValueError("abnormal termination contract belongs to a non-defect case")
    tags = manifest.get("tags")
    if not isinstance(tags, list) or not {"known-defect", "unsafe-boundary"} <= set(tags):
        raise ValueError("abnormal termination contract lacks required defect tags")
    if not isinstance(abnormal, dict) or set(abnormal) != {
        "exit_code",
        "output_contains",
    }:
        raise ValueError("invalid abnormal termination contract")
    if abnormal.get("exit_code") != "nonzero" or exit_code == 0:
        raise ValueError(
            f"abnormal termination exit code {exit_code} contradicts nonzero contract"
        )
    fragments = abnormal.get("output_contains")
    if (
        not isinstance(fragments, list)
        or not fragments
        or not all(isinstance(value, str) and value for value in fragments)
    ):
        raise ValueError("invalid abnormal termination output contract")
    missing = [value for value in fragments if value not in captured]
    if missing:
        raise ValueError(
            "abnormal termination output omits: " + ", ".join(repr(value) for value in missing)
        )
    return (
        f"known product defect terminated the isolated host process with exit code {exit_code}; "
        + "; ".join(f"observed {value!r}" for value in fragments)
    )


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
        "TIMEOUT",
        "INFRASTRUCTURE_ERROR",
    } and not failures:
        raise ValueError(f"{status} requires failure evidence")

    counts = payload.get("counts")
    expected_counts = {name: 0 for name in IDENTITY_COUNT_STATUSES}
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
    tools = discover_build_tools(str(config.llvm_root_path()))
    runtime_library = compiler_runtime_library(config).resolve().as_posix()
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
            f"-DPython3_EXECUTABLE={Path(tools['python']).as_posix()}",
            # CMAKE_LINKER already selects lld-link. Clear a stale cache value
            # because -fuse-ld=lld is a compiler-driver option, not an lld-link option.
            "-DCMAKE_EXE_LINKER_FLAGS=",
            f"-DNSC_COMPILER_SUITE_NSC_EXECUTABLE="
            f"{Path(config.nsc_executable).resolve().as_posix()}",
            f"-DNSC_NSLINK_EXECUTABLE={Path(config.nslink_executable).resolve().as_posix()}",
            f"-DNSC_COMPILER_SUITE_DEFAULT_WINDOWS_RUNTIME_LIBRARY="
            f"{runtime_library}",
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
    """Build the ordinary and isolated host-test targets in selected configurations."""
    tools = discover_build_tools(str(config.llvm_root_path()))
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
            "nsc_component_tests_destructive",
            "nsc_component_tests_source_extended",
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
    tools = discover_compiler_build_tools(str(config.llvm_root_path()))
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
    tools = discover_compiler_build_tools(str(config.llvm_root_path()))
    commands: list[tuple[str, list[str], Path]] = []
    for mode in modes:
        command = [
            tools["cmake"],
            "--build",
            compiler_build_dir(config, mode).as_posix(),
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
        timeout_budgets: dict[
            tuple[str, str, str, str], VariantTimeoutBudget
        ] | None = None,
    ) -> None:
        self.config = config
        self.store = store
        self.output = output
        self.status = status
        self.progress = progress
        self.process = ProcessController(self._emit_output, self._emit_activity)
        self.timeout_budgets = dict(timeout_budgets or {})
        self.fixture_cache: dict[tuple[str, str, str], Path] = {}
        self.fixture_failures: dict[tuple[str, str, str], tuple[str, str]] = {}
        self.module_cache: dict[str, Any] = {}
        self.active_key = ""
        self.active_phase = ""
        self.active_detail = ""

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
            self.store.event("CASE_STARTED", self._variant_payload(variant))
            outcome = self._run_controlled_variant(variant)
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
            self.active_key = ""
            self.active_phase = ""
            self.active_detail = ""
        return outcomes, self.process.cancelled.is_set()

    def cancel(self) -> None:
        """Request cancellation of the active child and all remaining variants."""
        self.process.cancel()

    def cancel_current(self, key: str) -> bool:
        """Cancel exactly the active row without setting the run-wide event."""
        if not key or key != self.active_key:
            return False
        return self.process.cancel_current("case-cancelled")

    def _run_controlled_variant(self, variant: RunVariant) -> CaseOutcome:
        """Apply one isolated timeout/cancellation scope around a variant dispatch."""
        budget = self.timeout_budgets.get(
            variant_history_key(variant),
            VariantTimeoutBudget(0, None, timeout_limit_for_average(None)),
        )
        generation = self.process.begin_variant()
        started = time.monotonic()
        average_detail = (
            f"promedio PASS {budget.average_seconds:.3f} s"
            if budget.average_seconds is not None
            else "sin PASS histórico"
        )
        self._set_active_phase(
            variant,
            "RUNNING",
            f"iniciando variante; límite {budget.limit_seconds} s; {average_detail}",
        )
        heartbeat_stop = threading.Event()

        def heartbeat() -> None:
            """Keep elapsed duration visible even while no child emits output."""
            while not heartbeat_stop.wait(0.25):
                self._emit_activity(time.monotonic() - started, "")

        heartbeat_thread = threading.Thread(
            target=heartbeat,
            name="nscbuilder3-variant-heartbeat",
            daemon=True,
        )
        timeout_timer = threading.Timer(
            budget.limit_seconds,
            lambda: self.process.cancel_current("timeout", generation),
        )
        timeout_timer.daemon = True
        heartbeat_thread.start()
        timeout_timer.start()
        try:
            outcome = self._run_variant(variant)
        finally:
            timeout_timer.cancel()
            heartbeat_stop.set()
            heartbeat_thread.join(timeout=1.0)
        elapsed = time.monotonic() - started
        stop_reason = self.process.end_variant(generation)
        outcome.duration_seconds = elapsed
        outcome.profile = variant.case.profile
        outcome.timeout_pass_count = budget.pass_count
        outcome.timeout_average_seconds = budget.average_seconds
        outcome.timeout_limit_seconds = budget.limit_seconds
        if self.process.cancelled.is_set():
            outcome.status = "CANCELLED"
            outcome.detail = "corrida cancelada por el usuario"
        elif stop_reason == "case-cancelled":
            outcome.status = "CANCELLED"
            outcome.detail = "prueba cancelada por el usuario; la corrida continúa"
            outcome.evidence = self._write_variant_stop_evidence(
                variant, outcome, budget, "CANCELLED", outcome.evidence
            )
        elif stop_reason == "timeout":
            outcome.status = "TIMEOUT"
            average = (
                f"{budget.average_seconds:.3f} s en {budget.pass_count} PASS"
                if budget.average_seconds is not None
                else "sin PASS histórico confiable"
            )
            outcome.detail = (
                f"tiempo límite individual agotado a {budget.limit_seconds} s; "
                f"{average}; duración real {elapsed:.3f} s"
            )
            outcome.evidence = self._write_variant_stop_evidence(
                variant, outcome, budget, "TIMEOUT", outcome.evidence
            )
        return outcome

    def _write_variant_stop_evidence(
        self,
        variant: RunVariant,
        outcome: CaseOutcome,
        budget: VariantTimeoutBudget,
        status: str,
        process_evidence: str,
    ) -> str:
        """Persist the causal timing record for a manual cancel or timeout."""
        case_root = self._case_root(variant)
        path = case_root / ("timeout.json" if status == "TIMEOUT" else "cancellation.json")
        payload = {
            "schema": 1,
            "timestamp": utc_now(),
            "key": variant.key,
            "status": status,
            "profile": variant.case.profile,
            "historical_pass_count": budget.pass_count,
            "historical_pass_average_seconds": budget.average_seconds,
            "timeout_limit_seconds": budget.limit_seconds,
            "elapsed_seconds": outcome.duration_seconds,
            "process_evidence": process_evidence,
        }
        write_json_atomic(path, payload)
        self.store.event(f"CASE_{status}", payload)
        self.store.log(
            f"[{status}] {variant.key}: {outcome.duration_seconds:.3f} s; "
            f"límite {budget.limit_seconds} s\n"
        )
        return str(path)

    def _run_variant(self, variant: RunVariant) -> CaseOutcome:
        """Dispatch one variant by its manifest-owned executor contract."""
        started = time.monotonic()
        self.process.last_termination_reason = None
        try:
            if variant.case.executor in {
                "nsc_component_tests",
                "nsc_component_tests_destructive",
                "nsc_component_tests_source_extended",
            }:
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
        executable = component_test_executable(
            self.config, variant.build_mode, variant.case.executor
        )
        if not executable.is_file():
            return "INFRASTRUCTURE_ERROR", f"host executable does not exist: {executable}", ""
        case_root = self._case_root(variant)
        self._set_active_phase(
            variant,
            "RUNNING",
            f"ejecutando {variant.case.executor} --case {variant.case.case_id}",
        )
        code, captured = self.process.run(
            [str(executable), "--case", variant.case.case_id],
            case_root,
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
            try:
                abnormal_detail = expected_abnormal_termination_detail(
                    variant.case, code, captured
                )
            except ValueError as contract_error:
                return (
                    "INFRASTRUCTURE_ERROR",
                    f"{error}; {contract_error}",
                    str(log_path),
                )
            if abnormal_detail is not None:
                return self._expected_status(
                    variant.case, False, abnormal_detail, str(log_path)
                )
            return (
                "INFRASTRUCTURE_ERROR",
                f"{error}; host exit code {code}",
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
        self._set_active_phase(
            variant,
            "RUNNING",
            f"ejecutando controlador de identidad {variant.case.case_id}",
        )
        # This command runs the compiler-identity test controller; it does not build NSC.
        code, captured = self.process.run(command, SOURCE_ROOT)
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
        self._set_active_phase(
            variant,
            "RUNNING",
            f"ejecutando fixture {Path(project_relative).name}: {case_argument}",
        )
        code, captured = self.process.run(
            command,
            case_root,
            environment,
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
        source_project = SUITE_ROOT / relative
        fixture_root = self.store.root / "fixtures" / (
            f"{source_project.stem}-{variant.build_mode.casefold()}-{variant.optimization.casefold()}"
        )
        fixture_root.mkdir(parents=True, exist_ok=True)
        inventory_module = self._load_gate("selfhost_inventory.py")
        materializer = getattr(inventory_module, "materialize_contained_project", None)
        if not callable(materializer):
            raise RuntimeError("self-host inventory does not expose project materialization")
        project = Path(
            materializer(
                SOURCE_ROOT,
                source_project,
                fixture_root / "project",
            )
        )
        executable = fixture_root / f"{source_project.stem}.exe"
        command = [
            str(Path(self.config.nsc_executable).resolve()),
            "--emit-project-oop-exe",
            str(project),
            f"-{variant.optimization}",
            f"--profile={variant.build_mode.casefold()}",
            "-g0",
            "--library-reference-root",
            str((Path(self.config.install_prefix).resolve() / "lib")),
            "-o",
            str(executable),
        ]
        self._set_active_phase(
            variant,
            "COMPILING",
            f"compilando fixture {project.name}",
            str(fixture_root),
        )
        write_json(fixture_root / "compile.command.json", command)
        code, captured = self.process.run(command, fixture_root, self._nsc_environment())
        (fixture_root / "compile.log").write_text(captured, encoding="utf-8", newline="\n")
        if self.process.last_termination_reason == "cancelled":
            raise RuntimeError(f"fixture {source_project.name} compilation was cancelled")
        if code != 0:
            raise RuntimeError(f"fixture {source_project.name} compiler exited with code {code}")
        if not executable.is_file():
            raise RuntimeError(
                f"fixture {source_project.name} completed without executable output"
            )
        self.fixture_cache[key] = executable
        return executable

    def _run_buildsystem(self, variant: RunVariant) -> tuple[str, str, str]:
        """Execute one BuildSystem row through its existing case-level oracle."""
        self._set_active_phase(
            variant,
            "RUNNING",
            f"ejecutando gate BuildSystem {variant.case.case_id}",
        )
        module = self._load_gate("run_buildsystem_gate.py")
        tools = discover_build_tools(str(self.config.llvm_root_path()))
        cmake = Path(tools["cmake"]).resolve()
        installation = Path(self.config.install_prefix).resolve()
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
        self._set_active_phase(
            variant,
            "RUNNING",
            f"ejecutando gate Packaging {variant.case.case_id}",
        )
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
        ) -> subprocess.CompletedProcess[str]:
            return self.process.run_completed(command, cwd, environment)

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
        # Integration gates intentionally share ``component_gate_support`` as a
        # sibling module, and that support module imports the public ``nsc_suite``
        # package from the controller root. Loading a gate from an absolute file
        # path exposes neither location, so publish both canonical roots once
        # before executing any gate. The entries remain stable for lazy imports.
        controller_root = str(SUITE_ROOT / "controller")
        integration_root = str(path.parent)
        for import_root in (controller_root, integration_root):
            if import_root not in sys.path:
                sys.path.insert(0, import_root)
        module_name = f"nscbuilder3_{path.stem}"
        spec = importlib.util.spec_from_file_location(module_name, path)
        if spec is None or spec.loader is None:
            raise RuntimeError(f"cannot load gate: {path}")
        module = importlib.util.module_from_spec(spec)
        # ``dataclasses`` and other runtime introspection resolve annotations
        # through ``sys.modules`` while the module body is executing.  A module
        # loaded directly from a file therefore must be published before
        # ``exec_module`` and removed again if initialization fails.
        sys.modules[module_name] = module
        try:
            spec.loader.exec_module(module)
        except Exception:
            sys.modules.pop(module_name, None)
            raise
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
            "profile": variant.case.profile,
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
                profile=item.case.profile,
            )
            for item in variants
        ]

    def _emit_output(self, line: str) -> None:
        """Mirror subprocess output to both the GUI and progressive evidence."""
        self.store.log(line)
        self.output(line)

    def _set_active_phase(
        self,
        variant: RunVariant,
        phase: str,
        detail: str,
        evidence: str = "",
    ) -> None:
        """Publish the active row and retain its phase for process heartbeats."""
        self.active_key = variant.key
        self.active_phase = phase
        self.active_detail = detail
        self.status(variant.key, phase, detail, 0.0, evidence)

    def _emit_activity(self, elapsed: float, detail: str) -> None:
        """Refresh elapsed time and last child output for the active exact row."""
        if not self.active_key:
            return
        visible_detail = detail[-300:] if detail else self.active_detail
        self.status(
            self.active_key,
            self.active_phase,
            visible_detail,
            elapsed,
            "",
        )


def make_run_id() -> str:
    """Create a sortable collision-resistant local run identifier."""
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    return f"{stamp}-{uuid.uuid4().hex[:8]}"


def compiler_receipt(config: BuilderConfig) -> dict[str, Any]:
    """Capture immutable identities for the externally supplied NSC and NSLink."""
    config.validated(require_tools=True)
    receipt: dict[str, Any] = {
        "captured_at": utc_now(),
        "tools": [],
        "llvm_toolchain": discover_llvm_toolchain(config.llvm_root_path()),
    }
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
    return summary


def launch_gui(smoke_only: bool = False) -> int:
    """Launch the complete single-file PySide6 application."""
    try:
        from PySide6.QtCore import Qt, QThread, QTimer, QUrl, Signal
        from PySide6.QtGui import QColor, QDesktopServices
        from PySide6.QtWidgets import (
            QAbstractItemView,
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
            QMenu,
            QMessageBox,
            QProgressBar,
            QPlainTextEdit,
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
                    code, captured = self.controller.run(command, cwd)
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
                            captured_output=captured,
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
                        outcomes.extend(
                            blocked_infrastructure_outcomes(
                                self.commands,
                                index + 1,
                                str(self.store.log_path),
                                label,
                            )
                        )
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
                timeout_budgets = load_variant_timeout_budgets(self.variants)
                metadata = dict(self.metadata)
                metadata["variant_timeout_policy"] = {
                    "history_status": "PASS",
                    "history_key": ["case_id", "build_mode", "optimization", "profile"],
                    "rounding": "ceil-to-whole-second",
                    "fallback_seconds": 45 * 60,
                    "loaded_once": True,
                }
                self.store = EvidenceStore(self.run_id, metadata)
                self.engine = ExecutionEngine(
                    self.config,
                    self.store,
                    self.output_ready.emit,
                    self.case_ready.emit,
                    self.progress_ready.emit,
                    timeout_budgets,
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

        def cancel_current(self, key: str) -> bool:
            """Forward one row-scoped cancellation without stopping the worker."""
            return self.engine is not None and self.engine.cancel_current(key)

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
            "Progreso",
            "Estado",
            "Acción",
            "Duración",
            "Detalle / evidencia",
        )
        CASE_COLUMN = 3
        PROGRESS_COLUMN = 8
        STATUS_COLUMN = 9
        ACTION_COLUMN = 10
        DURATION_COLUMN = 11
        DETAIL_COLUMN = 12
        HISTORY_HEADERS = (
            "Fecha y hora",
            "Estado",
            "Resultados",
            "Configuración",
            "Pruebas",
            "Tipo",
            "Corrida",
            "Origen / directorio",
        )
        HISTORY_PATH_COLUMN = 7

        def __init__(self) -> None:
            super().__init__()
            self.catalog = CanonicalCatalog.load()
            self.worker: CommandWorker | SuiteWorker | None = None
            self.last_failed_variants: tuple[RunVariant, ...] = ()
            self.failure_outcomes_by_key: dict[str, CaseOutcome] = {}
            self.failure_variants_by_key: dict[str, RunVariant] = {}
            self.current_outcomes_by_key: dict[str, CaseOutcome] = {}
            self.disabled_failure_keys: set[str] = set()
            self.visible_variants: tuple[RunVariant, ...] = ()
            self.variant_by_key: dict[str, RunVariant] = {}
            self.status_item_by_key: dict[str, QTableWidgetItem] = {}
            self.live_status_by_key: dict[str, str] = {}
            self.live_status_counts: Counter[str] = Counter()
            self.variant_position_by_key: dict[str, int] = {}
            self.status_filter_visible_rows = 0
            self.path_edits: dict[str, QLineEdit] = {}
            self.active_operation = ""
            self.active_modes: tuple[str, ...] = ()
            self.infrastructure_ninja_total = 0
            self.history_run_pages: dict[str, QWidget] = {}
            self.pending_exact_action: tuple[str, bool] | None = None
            self.exact_action_suppressed_until = 0.0
            self.exact_action_timer = QTimer(self)
            self.exact_action_timer.setSingleShot(True)
            self.exact_action_timer.timeout.connect(self._run_pending_exact_action)
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
            self.tabs.addTab(self._build_execution_tab(), "Pruebas")
            self.tabs.addTab(self._build_results_tab(), "Resultados")
            self.history_top_tab_index = self.tabs.addTab(
                self._build_history_tab(), "Historial"
            )
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
            self._add_path_row(form, "compiler_llvm_root", "Raíz LLVM 23", True)
            self._add_path_row(form, "llvm_dir", "LLVM_DIR derivado", True)
            self.path_edits["llvm_dir"].setReadOnly(True)
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
            self.clean_test_tree_button = QPushButton("Limpiar árbol de pruebas")
            self.clean_test_tree_button.setToolTip(
                "Elimina únicamente los builds Debug y Release de la infraestructura "
                "de pruebas configurada."
            )
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
            compiler_controls.addWidget(self.clean_test_tree_button)
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
            self.clean_test_tree_button.clicked.connect(self._clean_test_tree)
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
            self.run_button = QPushButton("Ejecutar pruebas")
            self.cancel_button = QPushButton("Cancelar corrida/árbol")
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
            status_filter_bar = QHBoxLayout()
            status_filter_bar.addWidget(QLabel("Mostrar"))
            self.status_filter_combo = QComboBox()
            for label, value in (
                ("Todos los estados", "ALL"),
                ("Fallos", "FAILURES"),
                ("En curso", "ACTIVE"),
                ("Pendientes / listos", "WAITING"),
                ("PASS", "PASS"),
                ("FAIL", "FAIL"),
                ("KNOWN_FAILURE", "KNOWN_FAILURE"),
                ("BLOCKED", "BLOCKED"),
                ("INFRASTRUCTURE_ERROR", "INFRASTRUCTURE_ERROR"),
                ("UNEXPECTED_PASS", "UNEXPECTED_PASS"),
                ("CANCELLED", "CANCELLED"),
                ("TIMEOUT", "TIMEOUT"),
                ("RUNNING", "RUNNING"),
                ("COMPILING", "COMPILING"),
                ("LISTO", "LISTO"),
                ("PENDIENTE", "PENDIENTE"),
                ("FILTRADO", "FILTRADO"),
            ):
                self.status_filter_combo.addItem(label, value)
            self.status_filter_summary = QLabel("Mostrando todas las filas")
            status_filter_bar.addWidget(self.status_filter_combo)
            status_filter_bar.addWidget(self.status_filter_summary)
            status_filter_bar.addStretch(1)
            layout.addLayout(status_filter_bar)
            self.clear_results_button = QPushButton(
                "Limpiar resultados para nueva corrida"
            )
            status_filter_bar.addWidget(self.clear_results_button)
            self.execution_activity = QLabel("Sin ejecución activa")
            self.execution_activity.setWordWrap(True)
            layout.addWidget(self.execution_activity)
            self.execution_counts = QLabel("Sin resultados")
            self.execution_counts.setWordWrap(True)
            layout.addWidget(self.execution_counts)
            self.progress = QProgressBar()
            self.progress.setRange(0, 1)
            self.progress.setValue(0)
            self.progress.setFormat("0/0 pruebas")
            layout.addWidget(self.progress)
            self.case_table = QTableWidget(0, len(self.TABLE_HEADERS))
            self.case_table.setHorizontalHeaderLabels(self.TABLE_HEADERS)
            self.case_table.setAlternatingRowColors(True)
            self.case_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
            self.case_table.setSelectionMode(
                QAbstractItemView.SelectionMode.SingleSelection
            )
            self.case_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
            self.case_table.setContextMenuPolicy(
                Qt.ContextMenuPolicy.CustomContextMenu
            )
            case_header = self.case_table.horizontalHeader()
            case_header.setSectionResizeMode(QHeaderView.ResizeMode.Interactive)
            for column, width in enumerate(
                (55, 190, 170, 180, 75, 55, 75, 180, 100, 125, 125, 90)
            ):
                case_header.resizeSection(column, width)
            case_header.setStretchLastSection(True)
            layout.addWidget(self.case_table, 1)
            self.scope_combo.currentTextChanged.connect(self._refresh_scope_values)
            self.preview_button.clicked.connect(self._preview_plan)
            self.run_button.clicked.connect(self._start_suite)
            self.cancel_button.clicked.connect(self._cancel_active)
            self.retry_button.clicked.connect(self._retry_failures)
            self.status_filter_combo.currentIndexChanged.connect(
                self._apply_status_filter
            )
            self.clear_results_button.clicked.connect(self._clear_current_results)
            self.case_table.cellClicked.connect(self._handle_case_table_click)
            self.case_table.cellDoubleClicked.connect(
                self._open_current_case_detail
            )
            self.case_table.customContextMenuRequested.connect(
                self._show_case_context_menu
            )
            return page

        def _build_results_tab(self) -> QWidget:
            page = QWidget()
            layout = QVBoxLayout(page)
            self.result_tabs = QTabWidget()
            self.full_log = QTextEdit()
            self.full_log.setReadOnly(True)
            self.error_report = QTextEdit()
            self.error_report.setReadOnly(True)
            failure_page = QWidget()
            failure_layout = QVBoxLayout(failure_page)
            self.failure_summary = QLabel("No hay fallos terminales registrados")
            failure_layout.addWidget(self.failure_summary)
            failure_controls = QHBoxLayout()
            self.failure_retry_button = QPushButton("Reejecutar fallos activos")
            self.failure_disable_button = QPushButton(
                "Desactivar seleccionados para reejecución"
            )
            self.failure_enable_button = QPushButton("Reactivar seleccionados")
            failure_controls.addWidget(self.failure_retry_button)
            failure_controls.addWidget(self.failure_disable_button)
            failure_controls.addWidget(self.failure_enable_button)
            failure_controls.addStretch(1)
            failure_layout.addLayout(failure_controls)
            self.failure_table = QTableWidget(0, len(self.TABLE_HEADERS))
            self.failure_table.setHorizontalHeaderLabels(self.TABLE_HEADERS)
            self.failure_table.setAlternatingRowColors(True)
            self.failure_table.setSelectionBehavior(
                QTableWidget.SelectionBehavior.SelectRows
            )
            self.failure_table.setSelectionMode(
                QAbstractItemView.SelectionMode.ExtendedSelection
            )
            self.failure_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
            self.failure_table.setContextMenuPolicy(
                Qt.ContextMenuPolicy.CustomContextMenu
            )
            self.failure_table.horizontalHeader().setSectionResizeMode(
                QHeaderView.ResizeMode.ResizeToContents
            )
            self.failure_table.horizontalHeader().setStretchLastSection(True)
            self.failure_table.cellClicked.connect(
                self._handle_failure_table_click
            )
            self.failure_table.cellDoubleClicked.connect(
                self._open_failure_case_detail
            )
            self.failure_table.customContextMenuRequested.connect(
                self._show_failure_context_menu
            )
            failure_layout.addWidget(self.failure_table, 1)
            self.result_tabs.addTab(self.full_log, "Log completo")
            self.result_tabs.addTab(self.error_report, "Errores condensados")
            self.failure_tab_index = self.result_tabs.addTab(
                failure_page, "Fallos exactos (0)"
            )
            self.failure_retry_button.clicked.connect(self._retry_failures)
            self.failure_disable_button.clicked.connect(
                lambda: self._set_selected_failures_enabled(False)
            )
            self.failure_enable_button.clicked.connect(
                lambda: self._set_selected_failures_enabled(True)
            )
            self._refresh_failure_controls()
            layout.addWidget(self.result_tabs)
            return page

        def _build_history_tab(self) -> QWidget:
            """Build the top-level historical workspace, separate from live results."""
            page = QWidget()
            layout = QVBoxLayout(page)
            self.history_workspace_tabs = QTabWidget()
            self.history_workspace_tabs.setObjectName("history_workspace_tabs")
            launcher_page = QWidget()
            history_layout = QVBoxLayout(launcher_page)
            self.history_table = QTableWidget(0, len(self.HISTORY_HEADERS))
            self.history_table.setHorizontalHeaderLabels(self.HISTORY_HEADERS)
            self.history_table.setSelectionBehavior(
                QTableWidget.SelectionBehavior.SelectRows
            )
            self.history_table.setEditTriggers(
                QTableWidget.EditTrigger.NoEditTriggers
            )
            self.history_table.horizontalHeader().setSectionResizeMode(
                QHeaderView.ResizeMode.ResizeToContents
            )
            self.history_table.horizontalHeader().setStretchLastSection(True)
            history_intro = QLabel(
                f"Corridas guardadas en {HISTORY_ROOT}. Selecciona una fila para ver "
                "qué se ejecutó, sus resultados y su log; abrirla crea un tab "
                "independiente y no modifica Pruebas."
            )
            history_intro.setWordWrap(True)
            history_layout.addWidget(history_intro)
            history_controls = QHBoxLayout()
            self.history_refresh_button = QPushButton("Actualizar lista")
            self.history_open_folder_button = QPushButton(
                "Abrir carpeta de evidencia"
            )
            self.history_load_button = QPushButton(
                "Abrir corrida completa en un tab"
            )
            history_controls.addWidget(self.history_refresh_button)
            history_controls.addWidget(self.history_open_folder_button)
            history_controls.addWidget(self.history_load_button)
            history_controls.addStretch(1)
            history_layout.addLayout(history_controls)
            history_layout.addWidget(self.history_table, 2)
            history_layout.addWidget(QLabel("Vista previa de la corrida seleccionada"))
            self.history_preview_tabs = QTabWidget()
            self.history_preview_summary = QPlainTextEdit()
            self.history_preview_summary.setReadOnly(True)
            self.history_preview_log = QPlainTextEdit()
            self.history_preview_log.setReadOnly(True)
            self.history_preview_report = QPlainTextEdit()
            self.history_preview_report.setReadOnly(True)
            self.history_preview_tabs.addTab(self.history_preview_summary, "Resumen")
            self.history_preview_tabs.addTab(self.history_preview_log, "Log")
            self.history_preview_tabs.addTab(
                self.history_preview_report, "Reporte guardado"
            )
            history_layout.addWidget(self.history_preview_tabs, 1)
            self.history_refresh_button.clicked.connect(self._refresh_history)
            self.history_open_folder_button.clicked.connect(self._open_history)
            self.history_load_button.clicked.connect(self._load_history_from_table)
            self.history_table.itemSelectionChanged.connect(
                self._preview_history_selection
            )
            self.history_table.cellDoubleClicked.connect(
                lambda _row, _column: self._load_history_from_table()
            )
            self.history_launcher_tab_index = self.history_workspace_tabs.addTab(
                launcher_page, "Corridas guardadas"
            )
            layout.addWidget(self.history_workspace_tabs, 1)
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
                tools = discover_build_tools(str(discovered.llvm_root_path()))
                lines = ["RUTAS AUTODETECTADAS", "=" * 72]
                lines.extend(f"{name}: {value}" for name, value in tools.items())
                lines.extend(
                    [
                        f"NSC: {discovered.nsc_executable}",
                        f"NSLink: {discovered.nslink_executable}",
                        f"Fuentes NSC: {SOURCE_ROOT}",
                        f"LLVM root de NSC: {discovered.compiler_llvm_root}",
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
                compiler_llvm_root=self.path_edits["compiler_llvm_root"].text().strip(),
                llvm_dir=self.path_edits["llvm_dir"].text().strip(),
                archive_executable=self.path_edits["archive_executable"].text().strip(),
                comparison_nsc=self.path_edits["comparison_nsc"].text().strip(),
                build_id_header=self.path_edits["build_id_header"].text().strip(),
                jobs=self.jobs_spin.value(),
            )

        def _save_config(self) -> None:
            try:
                config = self._config_from_ui().with_discovery().validated(
                    require_tools=False
                )
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

        def _clean_test_tree(self) -> None:
            """Remove only generated test builds after an exact-path confirmation."""
            if self.worker is not None:
                QMessageBox.warning(
                    self,
                    "Hay un proceso activo",
                    "No se puede limpiar el árbol de pruebas mientras Builder 3 "
                    "ejecuta o construye un proceso. Cancélalo y espera a que termine "
                    "antes de volver a intentarlo.",
                )
                self.statusBar().showMessage(
                    "Limpieza rechazada: todavía hay un proceso activo", 8000
                )
                return
            try:
                config = self._config_from_ui().with_discovery().validated(
                    require_tools=False
                )
                targets = test_tree_cleanup_targets(config)
            except Exception as error:
                self._show_error("No se puede limpiar el árbol de pruebas", error)
                return

            existing = tuple(path for path in targets if path.exists())
            path_lines = [
                f"• {path} ({'existe' if path.exists() else 'no existe'})"
                for path in targets
            ]
            if not existing:
                message = (
                    "No hay artefactos generados que limpiar en:\n\n"
                    + "\n".join(path_lines)
                )
                self.infrastructure_stage.setText("Árbol de pruebas ya limpio")
                self.infrastructure_progress_detail.setText(message)
                self.infrastructure_log.setPlainText(message)
                self.statusBar().showMessage("Árbol de pruebas ya limpio", 8000)
                self._preview_plan()
                self._refresh_history()
                QMessageBox.information(self, "Árbol de pruebas limpio", message)
                return

            prompt = (
                "Se eliminarán únicamente estos árboles generados de pruebas:\n\n"
                + "\n".join(path_lines)
                + "\n\nLa limpieza incluye binarios, objetos y archivos de "
                "configuración/build contenidos en esas rutas. No modifica fuentes, "
                "manifiestos, historial/evidencias, la instalación de NSC ni el build "
                "del compilador. ¿Continuar?"
            )
            answer = QMessageBox.warning(
                self,
                "Confirmar limpieza del árbol de pruebas",
                prompt,
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if answer != QMessageBox.StandardButton.Yes:
                self.statusBar().showMessage("Limpieza cancelada por el usuario", 5000)
                return
            if self.worker is not None:
                QMessageBox.warning(
                    self,
                    "Hay un proceso activo",
                    "La limpieza no comenzó porque se inició otro proceso.",
                )
                return

            self._set_busy(True)
            self.infrastructure_stage.setText("Limpiando árbol generado de pruebas")
            self.infrastructure_progress.setRange(0, 0)
            self.infrastructure_progress.setFormat("Limpieza en curso")
            self.infrastructure_progress_detail.setText("\n".join(path_lines))
            try:
                result = clean_test_build_trees(config)
            except Exception as error:
                self._show_error("Falló la limpieza del árbol de pruebas", error)
                return
            finally:
                self._set_busy(False)

            lines = [
                "LIMPIEZA DEL ÁRBOL DE PRUEBAS",
                "=" * 72,
                *(f"Ruta: {path}" for path in result.targets),
                f"Archivos eliminados: {result.removed_files:,}",
                f"Directorios eliminados: {result.removed_directories:,}",
                f"Rutas ya ausentes: {len(result.missing_targets):,}",
            ]
            if result.failures:
                lines.extend(("", "ERRORES:"))
                lines.extend(result.failures)
            self.infrastructure_log.setPlainText("\n".join(lines))
            self.infrastructure_progress.setRange(0, 1)
            self.infrastructure_progress.setValue(1)
            self._preview_plan()
            self._refresh_history()
            if result.failures:
                self.infrastructure_stage.setText("Limpieza incompleta")
                self.infrastructure_progress.setFormat("Limpieza incompleta")
                self.infrastructure_progress_detail.setText(
                    f"{len(result.failures):,} rutas no pudieron eliminarse; "
                    "revisa el registro."
                )
                QMessageBox.critical(
                    self,
                    "Limpieza incompleta",
                    f"Se encontraron {len(result.failures):,} errores. "
                    "No se tocó ninguna ruta fuera de los árboles confirmados.",
                )
            else:
                self.infrastructure_stage.setText("Árbol de pruebas limpio")
                self.infrastructure_progress.setFormat("Limpieza terminada")
                self.infrastructure_progress_detail.setText(
                    f"Eliminados {result.removed_files:,} archivos y "
                    f"{result.removed_directories:,} directorios."
                )
                self.statusBar().showMessage("Árbol de pruebas limpio", 8000)
                QMessageBox.information(
                    self,
                    "Árbol de pruebas limpio",
                    "La próxima construcción/configuración de pruebas partirá de cero.",
                )

        def _start_infrastructure(self, operation: str) -> None:
            if self.worker is not None:
                return
            try:
                config = self._config_from_ui().with_discovery().validated(
                    require_tools=False
                )
                self._load_config(config)
                compiler_operation = operation.startswith("compiler-")
                tools = (
                    discover_compiler_build_tools(str(config.llvm_root_path()))
                    if compiler_operation
                    else discover_build_tools(str(config.llvm_root_path()))
                )
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
                    "variant_count": len(commands),
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
            failed = evidence.startswith("ERROR:") or any(
                item.status in TERMINAL_FAILURES for item in typed
            )
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
            self._render_error_report(typed, cancelled, evidence)
            if failed:
                self.tabs.setCurrentIndex(2)
                self.result_tabs.setCurrentIndex(1)
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
            self,
            cases: Sequence[CaseRecord],
            variants: Sequence[RunVariant],
            outcomes: Sequence[CaseOutcome] = (),
        ) -> None:
            executable_ids = {variant.case.case_id for variant in variants}
            outcome_by_key = {outcome.key: outcome for outcome in outcomes}
            pending = [case for case in cases if not case.executable]
            filtered = [
                case for case in cases if case.executable and case.case_id not in executable_ids
            ]
            rows: list[tuple[CaseRecord, str, str, str, str, float, str]] = []
            for variant in variants:
                outcome = outcome_by_key.get(variant.key)
                detail = ""
                duration = 0.0
                status = "LISTO"
                if outcome is not None:
                    status = outcome.status
                    duration = outcome.duration_seconds
                    detail = outcome.detail
                    if outcome.evidence:
                        detail = (
                            f"{detail} | {outcome.evidence}"
                            if detail
                            else outcome.evidence
                        )
                rows.append(
                    (
                    variant.case,
                    variant.build_mode,
                    variant.optimization,
                    variant.key,
                    status,
                    duration,
                    detail,
                    )
                )
            rows.extend(
                (
                    case,
                    "NA",
                    "NA",
                    f"{case.case_id}|NA|NA",
                    "PENDIENTE",
                    0.0,
                    case.disposition,
                )
                for case in pending
            )
            rows.extend(
                (
                    case,
                    "NA",
                    "NA",
                    f"{case.case_id}|FILTERED|FILTERED",
                    "FILTRADO",
                    0.0,
                    "la combinación Build/Opt elegida no está soportada por el caso",
                )
                for case in filtered
            )
            self.case_table.setSortingEnabled(False)
            self.case_table.setUpdatesEnabled(False)
            self.case_table.clearContents()
            self.case_table.setRowCount(len(rows))
            self.status_item_by_key.clear()
            self.variant_by_key = {variant.key: variant for variant in variants}
            self.live_status_by_key = {
                key: status
                for _case, _mode, _opt, key, status, _duration, _detail in rows
            }
            self.live_status_counts = Counter(self.live_status_by_key.values())
            self.variant_position_by_key = {
                variant.key: index
                for index, variant in enumerate(variants, start=1)
            }
            for row_index, (
                case,
                mode,
                opt,
                key,
                status,
                duration,
                detail,
            ) in enumerate(rows):
                values = (
                    str(case.wave),
                    case.component,
                    case.contract_id,
                    case.case_id,
                    mode,
                    opt,
                    case.profile,
                    case.executor,
                    row_progress_text(status),
                    status,
                    row_action_text(status, key in self.variant_by_key),
                    f"{duration:.1f} s" if duration else "",
                    detail,
                )
                for column, value in enumerate(values):
                    item = QTableWidgetItem(value)
                    if column == self.CASE_COLUMN:
                        item.setData(Qt.ItemDataRole.UserRole, key)
                        item.setToolTip(
                            "Doble clic para abrir el escenario, el oráculo y la salida completa"
                        )
                    if column == self.STATUS_COLUMN:
                        self._color_status(item, status)
                        self.status_item_by_key[key] = item
                    if column == self.ACTION_COLUMN and value:
                        item.setForeground(QColor("#70a7ff"))
                        item.setToolTip("Ejecutar solamente esta variante exacta")
                    self.case_table.setItem(row_index, column, item)
            self.case_table.setUpdatesEnabled(True)
            self.case_table.setSortingEnabled(True)
            self.plan_label.setText(
                f"Selección: {len(cases):,} casos canónicos · {len(variants):,} variantes "
                f"ejecutables · {len(pending):,} pendientes visibles · {len(filtered):,} filtradas"
            )
            self.progress.setRange(0, max(1, len(variants)))
            self.progress.setValue(0)
            self.progress.setFormat(f"0/{len(variants):,} pruebas (0%)")
            self.execution_activity.setText("Preparada; ninguna prueba ha iniciado")
            self._refresh_live_counts()
            self._apply_status_filter()

        def _current_status_filter(self) -> str:
            value = self.status_filter_combo.currentData()
            return value if isinstance(value, str) and value else "ALL"

        def _render_status_filter_summary(self) -> None:
            total = self.case_table.rowCount()
            self.status_filter_summary.setText(
                f"Mostrando {self.status_filter_visible_rows:,} de {total:,} filas"
            )

        def _apply_status_filter(self, _index: int | None = None) -> None:
            """Hide rows by status while preserving the immutable execution plan."""
            filter_name = self._current_status_filter()
            visible = 0
            self.case_table.setUpdatesEnabled(False)
            try:
                for row in range(self.case_table.rowCount()):
                    item = self.case_table.item(row, self.STATUS_COLUMN)
                    status = item.text() if item is not None else ""
                    matches = status_filter_matches(filter_name, status)
                    self.case_table.setRowHidden(row, not matches)
                    visible += int(matches)
            finally:
                self.case_table.setUpdatesEnabled(True)
            self.status_filter_visible_rows = visible
            self._render_status_filter_summary()

        def _apply_status_filter_to_row(self, row: int, status: str) -> None:
            """Refresh one changing row without rescanning the full test table."""
            was_visible = not self.case_table.isRowHidden(row)
            is_visible = status_filter_matches(self._current_status_filter(), status)
            self.case_table.setRowHidden(row, not is_visible)
            if was_visible != is_visible:
                self.status_filter_visible_rows += 1 if is_visible else -1
                self._render_status_filter_summary()

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

        def _case_key_at(self, table: QTableWidget, row: int) -> str | None:
            """Read the exact immutable variant key carried by one visible row."""
            item = table.item(row, self.CASE_COLUMN)
            if item is None:
                return None
            key = item.data(Qt.ItemDataRole.UserRole)
            return key if isinstance(key, str) and key else None

        def _start_exact_variant(self, variant: RunVariant) -> None:
            """Run one exact case/build/optimization variant without widening scope."""
            if self.worker is not None:
                return
            try:
                config = self._config_from_ui().validated(require_tools=True)
                config.save()
                self._update_case(
                    variant.key,
                    "LISTO",
                    "ejecución individual preparada",
                    0.0,
                    "",
                )
                self._run_variants(config, (variant,), "single-exact-variant")
            except Exception as error:
                self._show_error("No se pudo ejecutar la prueba seleccionada", error)

        def _start_exact_key(self, key: str, *, failed: bool = False) -> None:
            """Resolve and start one exact key from the main or failure projection."""
            variants = self.failure_variants_by_key if failed else self.variant_by_key
            variant = variants.get(key)
            if variant is not None:
                self._start_exact_variant(variant)

        def _case_at(self, table: QTableWidget, row: int) -> CaseRecord | None:
            """Resolve the canonical case represented by one table row."""
            item = table.item(row, self.CASE_COLUMN)
            if item is None:
                return None
            return self.catalog.by_id.get(item.text())

        @staticmethod
        def _resolved_evidence_path(
            outcome: CaseOutcome | None, run_root: Path | None
        ) -> Path | None:
            """Resolve one evidence location without requiring it to still exist."""
            if outcome is None or not outcome.evidence:
                return None
            path = Path(outcome.evidence)
            if not path.is_absolute() and run_root is not None:
                path = run_root / path
            return path

        def _open_case_detail_tab(
            self,
            tab_widget: QTabWidget,
            case: CaseRecord,
            variant: RunVariant | None,
            outcome: CaseOutcome | None,
            run_root: Path | None = None,
        ) -> None:
            """Open a read-only case diagnosis without executing or rewriting it."""
            identity = variant.key if variant is not None else case.case_id
            detail_key = f"case-detail:{identity}"
            for index in range(tab_widget.count() - 1, -1, -1):
                existing = tab_widget.widget(index)
                if existing.property("caseDetailKey") == detail_key:
                    tab_widget.removeTab(index)
                    existing.deleteLater()

            page = QWidget()
            page.setProperty("caseDetailKey", detail_key)
            layout = QVBoxLayout(page)
            heading = QLabel(
                f"{case.case_id} · {case.component} · {case.contract_id} · "
                f"{outcome.status if outcome is not None else 'SIN RESULTADO'}"
            )
            heading.setWordWrap(True)
            layout.addWidget(heading)

            actions = QHBoxLayout()
            evidence_path = self._resolved_evidence_path(outcome, run_root)
            open_evidence = QPushButton("Abrir evidencia")
            open_evidence.setEnabled(
                evidence_path is not None and evidence_path.exists()
            )
            close_detail = QPushButton("Cerrar detalle")
            actions.addWidget(open_evidence)
            actions.addStretch(1)
            actions.addWidget(close_detail)
            layout.addLayout(actions)

            sections = QTabWidget()

            def add_text_section(title: str, contents: str) -> None:
                view = QPlainTextEdit()
                view.setReadOnly(True)
                view.setPlainText(contents)
                sections.addTab(view, title)

            add_text_section(
                "Resumen", case_diagnostic_summary(case, variant, outcome)
            )
            add_text_section(
                "Entradas y oráculo",
                json.dumps(
                    case_contract_payload(case),
                    ensure_ascii=False,
                    indent=2,
                    sort_keys=True,
                ),
            )
            add_text_section(
                "Salida completa", case_evidence_output(outcome, run_root)
            )
            add_text_section(
                "Manifiesto",
                json.dumps(
                    case.manifest,
                    ensure_ascii=False,
                    indent=2,
                    sort_keys=True,
                )
                if case.manifest is not None
                else "Este caso no posee todavía un manifiesto ejecutable.\n",
            )
            layout.addWidget(sections, 1)
            index = tab_widget.addTab(page, f"{case.case_id} · detalle")
            tab_widget.setCurrentIndex(index)

            def reveal_evidence() -> None:
                if evidence_path is None:
                    return
                target = (
                    evidence_path if evidence_path.is_dir() else evidence_path.parent
                )
                QDesktopServices.openUrl(QUrl.fromLocalFile(str(target)))

            def close_page() -> None:
                page_index = tab_widget.indexOf(page)
                if page_index >= 0:
                    tab_widget.removeTab(page_index)
                page.deleteLater()

            open_evidence.clicked.connect(reveal_evidence)
            close_detail.clicked.connect(close_page)

        def _cancel_pending_exact_action(self) -> None:
            """Prevent a double click from also triggering the single-click action."""
            self.exact_action_timer.stop()
            self.pending_exact_action = None
            self.exact_action_suppressed_until = time.monotonic() + 0.5

        def _run_pending_exact_action(self) -> None:
            pending = self.pending_exact_action
            self.pending_exact_action = None
            if pending is None or self.worker is not None:
                return
            key, failed = pending
            self._start_exact_key(key, failed=failed)

        def _queue_exact_action(
            self, table: QTableWidget, row: int, column: int, *, failed: bool
        ) -> None:
            if (
                column != self.ACTION_COLUMN
                or self.worker is not None
                or time.monotonic() < self.exact_action_suppressed_until
            ):
                return
            key = self._case_key_at(table, row)
            variants = (
                self.failure_variants_by_key if failed else self.variant_by_key
            )
            if key is None or key not in variants:
                return
            self.pending_exact_action = (key, failed)
            self.exact_action_timer.start(QApplication.doubleClickInterval() + 50)

        def _open_current_case_detail(self, row: int, _column: int) -> None:
            self._cancel_pending_exact_action()
            case = self._case_at(self.case_table, row)
            key = self._case_key_at(self.case_table, row)
            if case is None or key is None:
                return
            self._open_case_detail_tab(
                self.result_tabs,
                case,
                self.variant_by_key.get(key),
                self.current_outcomes_by_key.get(key),
            )
            self.tabs.setCurrentIndex(2)

        def _open_failure_case_detail(self, row: int, _column: int) -> None:
            self._cancel_pending_exact_action()
            case = self._case_at(self.failure_table, row)
            key = self._case_key_at(self.failure_table, row)
            if case is None or key is None:
                return
            self._open_case_detail_tab(
                self.result_tabs,
                case,
                self.failure_variants_by_key.get(key),
                self.failure_outcomes_by_key.get(key),
            )
            self.tabs.setCurrentIndex(2)

        def _handle_case_table_click(self, row: int, column: int) -> None:
            if column == self.ACTION_COLUMN:
                key = self._case_key_at(self.case_table, row)
                if (
                    key is not None
                    and self.live_status_by_key.get(key) == "RUNNING"
                    and isinstance(self.worker, SuiteWorker)
                ):
                    self._cancel_current_variant(key)
                    return
            self._queue_exact_action(
                self.case_table, row, column, failed=False
            )

        def _cancel_current_variant(self, key: str) -> None:
            """Request cancellation only for the exact active RUNNING row."""
            worker = self.worker
            if isinstance(worker, SuiteWorker) and worker.cancel_current(key):
                self.statusBar().showMessage(
                    f"Cancelación solicitada únicamente para {key}; la corrida continuará"
                )

        def _show_case_context_menu(self, position: Any) -> None:
            row = self.case_table.rowAt(position.y())
            if row < 0:
                return
            key = self._case_key_at(self.case_table, row)
            variant = self.variant_by_key.get(key or "")
            if variant is None:
                return
            self.case_table.selectRow(row)
            status = self.live_status_by_key.get(variant.key, "LISTO")
            menu = QMenu(self.case_table)
            if status == "RUNNING" and isinstance(self.worker, SuiteWorker):
                action = menu.addAction("Cancelar prueba")
                action.triggered.connect(
                    lambda: self._cancel_current_variant(variant.key)
                )
            else:
                label = (
                    "Volver a correr solamente esta prueba"
                    if status in RESULT_STATUSES
                    else "Ejecutar solamente esta prueba"
                )
                action = menu.addAction(label)
                action.setEnabled(self.worker is None)
                action.triggered.connect(lambda: self._start_exact_key(variant.key))
            menu.exec(self.case_table.viewport().mapToGlobal(position))

        def _handle_failure_table_click(self, row: int, column: int) -> None:
            self._queue_exact_action(
                self.failure_table, row, column, failed=True
            )

        def _show_failure_context_menu(self, position: Any) -> None:
            row = self.failure_table.rowAt(position.y())
            if row < 0:
                return
            key = self._case_key_at(self.failure_table, row)
            if key is None or key not in self.failure_variants_by_key:
                return
            self.failure_table.selectRow(row)
            menu = QMenu(self.failure_table)
            action = menu.addAction("Volver a correr solamente esta prueba")
            action.setEnabled(self.worker is None)
            action.triggered.connect(
                lambda: self._start_exact_key(key, failed=True)
            )
            menu.exec(self.failure_table.viewport().mapToGlobal(position))

        def _run_variants(
            self,
            config: BuilderConfig,
            variants: Sequence[RunVariant],
            origin: str,
        ) -> None:
            self.visible_variants = tuple(variants)
            self.current_outcomes_by_key.clear()
            self.variant_position_by_key = {
                variant.key: index
                for index, variant in enumerate(variants, start=1)
            }
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
            self.case_table.setSortingEnabled(False)
            self.execution_activity.setText(
                f"Iniciando 1/{len(variants):,}: {variants[0].case.case_id}"
            )
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
                variants = active_retry_variants(
                    self.last_failed_variants, self.disabled_failure_keys
                )
                if not variants:
                    raise ValueError(
                        "todos los fallos están desactivados para esta reejecución"
                    )
                self._populate_plan(
                    tuple(variant.case for variant in variants),
                    variants,
                )
                self._run_variants(config, variants, "rerun-failures")
            except Exception as error:
                self._show_error("No se pudieron reejecutar los fallos", error)

        def _set_selected_failures_enabled(self, enabled: bool) -> None:
            """Toggle only selected exact variants in the directed retry plan."""
            rows = {
                index.row()
                for index in self.failure_table.selectionModel().selectedRows()
            }
            for row in rows:
                case_item = self.failure_table.item(row, 3)
                if case_item is None:
                    continue
                key = case_item.data(Qt.ItemDataRole.UserRole)
                if not isinstance(key, str) or key not in self.failure_variants_by_key:
                    continue
                if enabled:
                    self.disabled_failure_keys.discard(key)
                else:
                    self.disabled_failure_keys.add(key)
            self._populate_failure_table()

        def _populate_failure_table(self) -> None:
            """Render one row per exact terminal variant retained for retry."""
            variants = tuple(
                self.failure_variants_by_key[key]
                for key in sorted(self.failure_variants_by_key)
            )
            self.failure_table.setSortingEnabled(False)
            self.failure_table.clearContents()
            self.failure_table.setRowCount(len(variants))
            for row, variant in enumerate(variants):
                outcome = self.failure_outcomes_by_key[variant.key]
                disabled = variant.key in self.disabled_failure_keys
                status = "DESACTIVADA" if disabled else outcome.status
                detail = outcome.detail
                if outcome.evidence:
                    detail = (
                        f"{detail} | {outcome.evidence}"
                        if detail
                        else outcome.evidence
                    )
                values = (
                    str(variant.case.wave),
                    variant.case.component,
                    variant.case.contract_id,
                    variant.case.case_id,
                    variant.build_mode,
                    variant.optimization,
                    variant.case.profile,
                    variant.case.executor,
                    row_progress_text(outcome.status),
                    status,
                    "Volver a correr",
                    f"{outcome.duration_seconds:.3f} s",
                    detail,
                )
                for column, value in enumerate(values):
                    item = QTableWidgetItem(value)
                    if column == self.CASE_COLUMN:
                        item.setData(Qt.ItemDataRole.UserRole, variant.key)
                        item.setToolTip(
                            "Doble clic para abrir el escenario, el oráculo y la salida completa"
                        )
                    if column == self.STATUS_COLUMN:
                        self._color_status(item, status)
                    if column == self.ACTION_COLUMN:
                        item.setForeground(QColor("#70a7ff"))
                        item.setToolTip("Volver a correr solamente esta variante exacta")
                    self.failure_table.setItem(row, column, item)
            self.failure_table.setSortingEnabled(True)
            active = len(variants) - len(self.disabled_failure_keys)
            disabled = len(self.disabled_failure_keys)
            self.failure_summary.setText(
                f"Fallos exactos: {len(variants):,} · activos para reejecución: "
                f"{active:,} · desactivados temporalmente: {disabled:,}"
            )
            self.result_tabs.setTabText(
                self.failure_tab_index, f"Fallos exactos ({len(variants):,})"
            )
            self._refresh_failure_controls()

        def _refresh_failure_controls(self) -> None:
            """Keep retry controls consistent with active and disabled failures."""
            idle = self.worker is None
            active = len(self.failure_variants_by_key) - len(self.disabled_failure_keys)
            self.retry_button.setEnabled(idle and active > 0)
            self.failure_retry_button.setEnabled(idle and active > 0)
            self.failure_disable_button.setEnabled(
                idle and bool(self.failure_variants_by_key)
            )
            self.failure_enable_button.setEnabled(
                idle and bool(self.disabled_failure_keys)
            )

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
            prior = self.live_status_by_key.get(key)
            if prior is not None and prior != status:
                self.live_status_counts[prior] -= 1
                if self.live_status_counts[prior] <= 0:
                    del self.live_status_counts[prior]
                self.live_status_counts[status] += 1
                self.live_status_by_key[key] = status
            self.case_table.setItem(
                row,
                self.PROGRESS_COLUMN,
                QTableWidgetItem(row_progress_text(status)),
            )
            status_item = QTableWidgetItem(status)
            self._color_status(status_item, status)
            self.case_table.setItem(row, self.STATUS_COLUMN, status_item)
            self.status_item_by_key[key] = status_item
            action_item = QTableWidgetItem(row_action_text(status, key in self.variant_by_key))
            if action_item.text():
                action_item.setForeground(QColor("#70a7ff"))
                action_item.setToolTip(
                    "Cancelar únicamente esta variante; la corrida continuará"
                    if status == "RUNNING"
                    else "Ejecutar solamente esta variante exacta"
                )
            self.case_table.setItem(row, self.ACTION_COLUMN, action_item)
            self.case_table.setItem(
                row,
                self.DURATION_COLUMN,
                QTableWidgetItem(
                    f"{duration:.1f} s"
                    if duration or status in {"RUNNING", "COMPILING"}
                    else ""
                ),
            )
            merged = detail
            if evidence:
                merged = f"{detail} | {evidence}" if detail else evidence
            variant = self.variant_by_key.get(key)
            if variant is not None and status in RESULT_STATUSES:
                self.current_outcomes_by_key[key] = CaseOutcome(
                    key=key,
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
                    profile=variant.case.profile,
                )
            self.case_table.setItem(
                row, self.DETAIL_COLUMN, QTableWidgetItem(merged)
            )
            self._apply_status_filter_to_row(row, status)
            position = self.variant_position_by_key.get(key, 0)
            prefix = f"{position:,}/{len(self.visible_variants):,}" if position else "Actual"
            self.execution_activity.setText(
                f"{prefix} · {status}: {key} — {merged or 'sin salida todavía'}"
            )
            self._refresh_live_counts()
            if not self.case_table.isRowHidden(row):
                self.case_table.scrollToItem(status_item)

        def _update_progress(self, done: int, total: int) -> None:
            self.progress.setRange(0, max(1, total))
            self.progress.setValue(done)
            self.progress.setFormat(f"{done:,}/{total:,} pruebas (%p%)")
            self.statusBar().showMessage(f"Ejecutando {done:,}/{total:,}")

        def _refresh_live_counts(self) -> None:
            """Render exact live state totals without rescanning the complete table."""
            order = (
                "LISTO",
                "PENDIENTE",
                "FILTRADO",
                "COMPILING",
                "RUNNING",
                "PASS",
                "FAIL",
                "KNOWN_FAILURE",
                "BLOCKED",
                "INFRASTRUCTURE_ERROR",
                "UNEXPECTED_PASS",
                "CANCELLED",
                "TIMEOUT",
            )
            parts = [
                f"{name}: {self.live_status_counts[name]:,}"
                for name in order
                if self.live_status_counts[name]
            ]
            self.execution_counts.setText(" · ".join(parts) if parts else "Sin resultados")

        def _suite_finished(self, outcomes: object, cancelled: bool, evidence: str) -> None:
            typed = list(outcomes) if isinstance(outcomes, list) else []
            failed_keys = {
                item.key for item in typed if item.status in TERMINAL_FAILURES
            }
            failed = bool(failed_keys) or evidence.startswith("ERROR:")
            current_failures = retry_variants(self.visible_variants, typed)
            current_keys = {variant.key for variant in self.visible_variants}
            outcome_by_key = {item.key: item for item in typed}
            self.current_outcomes_by_key.update(outcome_by_key)
            for key in current_keys:
                self.failure_variants_by_key.pop(key, None)
                self.failure_outcomes_by_key.pop(key, None)
                self.disabled_failure_keys.discard(key)
            for variant in current_failures:
                self.failure_variants_by_key[variant.key] = variant
                self.failure_outcomes_by_key[variant.key] = outcome_by_key[variant.key]
            self.disabled_failure_keys.intersection_update(self.failure_variants_by_key)
            self.last_failed_variants = tuple(
                self.failure_variants_by_key[key]
                for key in sorted(self.failure_variants_by_key)
            )
            self._populate_failure_table()
            self._render_error_report(typed, cancelled, evidence)
            completed = len(typed)
            total = len(self.visible_variants)
            self.progress.setRange(0, max(1, total))
            self.progress.setValue(min(completed, total))
            self.progress.setFormat(f"{completed:,}/{total:,} pruebas — TERMINADO (%p%)")
            self.execution_activity.setText(
                "Ejecución cancelada" if cancelled else "Ejecución terminada"
            )
            self.case_table.setSortingEnabled(True)
            self.worker = None
            self._set_busy(False)
            self._refresh_history()
            self.tabs.setCurrentIndex(2)
            self.result_tabs.setCurrentIndex(self.failure_tab_index if failed else 0)

        def _render_error_report(
            self, outcomes: Sequence[CaseOutcome], cancelled: bool, evidence: str
        ) -> None:
            self.error_report.setPlainText(
                condensed_error_report(outcomes, cancelled, evidence)
            )

        def _cancel_active(self) -> None:
            if self.worker is not None:
                self.worker.cancel()
                self.statusBar().showMessage("Cancelación solicitada al árbol activo")

        def _set_busy(self, busy: bool) -> None:
            self.compiler_configure_button.setEnabled(not busy)
            self.compiler_build_button.setEnabled(not busy)
            self.clean_test_tree_button.setEnabled(not busy)
            self.configure_button.setEnabled(not busy)
            self.build_button.setEnabled(not busy)
            self.run_button.setEnabled(not busy)
            self.preview_button.setEnabled(not busy)
            self.save_button.setEnabled(not busy)
            self.detect_button.setEnabled(not busy)
            self.receipt_button.setEnabled(not busy)
            self.history_refresh_button.setEnabled(not busy)
            for page in self.history_run_pages.values():
                rerun_all = page.findChild(QPushButton, "history_rerun_all")
                rerun_failures = page.findChild(
                    QPushButton, "history_rerun_failures"
                )
                rerun_selected = page.findChild(
                    QPushButton, "history_rerun_selected"
                )
                if rerun_all is not None:
                    rerun_all.setEnabled(not busy)
                if rerun_failures is not None:
                    rerun_failures.setEnabled(
                        not busy
                        and bool(rerun_failures.property("hasRetryableFailures"))
                    )
                if rerun_selected is not None:
                    rerun_selected.setEnabled(
                        not busy
                        and bool(
                            rerun_selected.property("hasExecutableSelection")
                        )
                    )
            self.clear_results_button.setEnabled(not busy)
            infrastructure_busy = busy and bool(self.active_operation)
            self.infrastructure_cancel_button.setEnabled(infrastructure_busy)
            self.cancel_button.setEnabled(busy and not infrastructure_busy)
            if busy:
                self.retry_button.setEnabled(False)
                self.failure_retry_button.setEnabled(False)
                self.failure_disable_button.setEnabled(False)
                self.failure_enable_button.setEnabled(False)
            else:
                self._refresh_failure_controls()
                self._preview_history_selection()
            self.statusBar().showMessage("Operación activa" if busy else "Listo")

        def _clear_current_results(self) -> None:
            """Reset only the live workspace; historical result tabs stay untouched."""
            if self.worker is not None:
                return
            self.failure_outcomes_by_key.clear()
            self.failure_variants_by_key.clear()
            self.current_outcomes_by_key.clear()
            self.disabled_failure_keys.clear()
            self.last_failed_variants = ()
            self.full_log.clear()
            self.error_report.clear()
            for index in range(self.result_tabs.count() - 1, 2, -1):
                detail_page = self.result_tabs.widget(index)
                self.result_tabs.removeTab(index)
                detail_page.deleteLater()
            all_statuses_index = self.status_filter_combo.findData("ALL")
            if all_statuses_index >= 0:
                self.status_filter_combo.setCurrentIndex(all_statuses_index)
            self._populate_failure_table()
            self._preview_plan()
            self.execution_activity.setText("Preparada; ninguna prueba ha iniciado")
            self.statusBar().showMessage(
                "Resultados actuales limpiados; lista para una nueva corrida"
            )

        def _refresh_history(self) -> None:
            selected_path = self._selected_history_path()
            if not HISTORY_ROOT.is_dir():
                self.history_table.setRowCount(0)
                self.history_load_button.setEnabled(False)
                self.history_open_folder_button.setEnabled(False)
                self.history_preview_summary.setPlainText(
                    f"No existe todavía el directorio de historial: {HISTORY_ROOT}"
                )
                self.history_preview_log.clear()
                self.history_preview_report.clear()
                return
            records: list[tuple[tuple[str, ...], tuple[str, str, str, str, str]]] = []
            for root in sorted(HISTORY_ROOT.iterdir(), reverse=True):
                if not root.is_dir():
                    continue
                record = history_record(root)
                records.append((self._history_display_values(root, record), record))
            self.history_table.setSortingEnabled(False)
            self.history_table.clearContents()
            self.history_table.setRowCount(len(records))
            selected_row = 0
            for row, (values, record) in enumerate(records):
                for column, value in enumerate(values):
                    item = QTableWidgetItem(value)
                    item.setData(Qt.ItemDataRole.UserRole, record[4])
                    if column == 1:
                        self._color_status(item, record[2])
                    self.history_table.setItem(row, column, item)
                if selected_path is not None and record[4] == str(selected_path):
                    selected_row = row
            self.history_table.setSortingEnabled(True)
            if records:
                path_to_select = (
                    str(selected_path)
                    if selected_path is not None
                    else records[selected_row][1][4]
                )
                for row in range(self.history_table.rowCount()):
                    item = self.history_table.item(row, self.HISTORY_PATH_COLUMN)
                    if item is not None and item.text() == path_to_select:
                        self.history_table.selectRow(row)
                        break
            else:
                self.history_preview_summary.setPlainText(
                    "No hay corridas guardadas todavía."
                )
                self.history_preview_log.clear()
                self.history_preview_report.clear()
                self.history_load_button.setEnabled(False)
                self.history_open_folder_button.setEnabled(False)

        def _history_display_values(
            self,
            root: Path,
            record: tuple[str, str, str, str, str],
        ) -> tuple[str, ...]:
            """Describe a stored run in user-facing terms without executing it."""
            run_id, kind, status, finished_at, path = record
            display_time = finished_at.replace("T", " ") if finished_at else "Sin cierre"
            result_summary = "Sin resultados completos"
            configuration = "No disponible"
            test_count = "0"
            try:
                metadata_path = root / "metadata.json"
                results_path = root / "results.json"
                metadata = (
                    json.loads(
                        metadata_path.read_text(encoding="utf-8"),
                        object_pairs_hook=unique_json_object,
                    )
                    if metadata_path.is_file()
                    else {}
                )
                payload = (
                    json.loads(
                        results_path.read_text(encoding="utf-8"),
                        object_pairs_hook=unique_json_object,
                    )
                    if results_path.is_file()
                    else {}
                )
                raw_counts = payload.get("counts", {})
                if isinstance(raw_counts, dict) and raw_counts:
                    order = (
                        "PASS",
                        "FAIL",
                        "INFRASTRUCTURE_ERROR",
                        "BLOCKED",
                        "KNOWN_FAILURE",
                        "UNEXPECTED_PASS",
                        "CANCELLED",
                    )
                    result_summary = " · ".join(
                        f"{name}: {raw_counts[name]:,}"
                        for name in order
                        if isinstance(raw_counts.get(name), int)
                        and raw_counts[name] > 0
                    )
                raw_results = payload.get("results")
                if isinstance(raw_results, list):
                    test_count = f"{len(raw_results):,} terminales"
                selection = metadata.get("selection")
                if isinstance(selection, dict):
                    scope = str(selection.get("scope", ""))
                    value = str(selection.get("value", ""))
                    profile = str(selection.get("profile", ""))
                    mode = str(selection.get("build_mode", ""))
                    optimization = str(selection.get("optimization", ""))
                    scope_text = f"{scope} {value}".strip()
                    configuration = " · ".join(
                        part
                        for part in (scope_text, profile, mode, optimization)
                        if part
                    )
                elif isinstance(metadata, dict):
                    configuration = str(
                        metadata.get("operation") or metadata.get("origin") or kind
                    )
            except (OSError, json.JSONDecodeError, ValueError, TypeError):
                result_summary = "La evidencia no se pudo interpretar"
            kind_label = "Suite de pruebas" if kind == "phase-2-suite" else kind
            return (
                display_time,
                status,
                result_summary,
                configuration,
                test_count,
                kind_label,
                run_id,
                path,
            )

        def _selected_history_path(self) -> Path | None:
            """Return the evidence directory selected in the history launcher."""
            row = self.history_table.currentRow()
            if row < 0:
                return None
            item = self.history_table.item(row, self.HISTORY_PATH_COLUMN)
            if item is None or not item.text():
                return None
            return Path(item.text())

        def _preview_history_selection(self) -> None:
            """Show identity, summary and log for a run before opening its full tab."""
            root = self._selected_history_path()
            if root is None:
                self.history_load_button.setEnabled(False)
                self.history_open_folder_button.setEnabled(False)
                return
            record = history_record(root)
            values = self._history_display_values(root, record)
            summary = [
                f"Corrida: {record[0]}",
                f"Fecha y hora: {values[0]}",
                f"Estado: {record[2]}",
                f"Tipo: {values[5]}",
                f"Configuración: {values[3]}",
                f"Pruebas: {values[4]}",
                f"Resultados: {values[2]}",
                f"Evidencia: {root}",
            ]
            if record[1] != "phase-2-suite":
                summary.append(
                    "Esta fila no es una corrida completa de pruebas de Fase 2; "
                    "puede consultarse, pero no abrirse como batería reejecutable."
                )
            elif record[2] in {"INCOMPLETE", "MALFORMED"}:
                summary.append(
                    "La corrida está incompleta o dañada y no puede reejecutarse."
                )
            self.history_preview_summary.setPlainText("\n".join(summary))
            log_path = root / "full.log"
            self.history_preview_log.setPlainText(
                log_path.read_text(encoding="utf-8", errors="replace")
                if log_path.is_file()
                else "Esta corrida no conserva full.log."
            )
            report_path = root / "summary.md"
            self.history_preview_report.setPlainText(
                report_path.read_text(encoding="utf-8", errors="replace")
                if report_path.is_file()
                else "Esta corrida no conserva summary.md."
            )
            idle = self.worker is None
            can_open = (
                record[1] == "phase-2-suite"
                and record[2] not in {"INCOMPLETE", "MALFORMED"}
            )
            self.history_load_button.setEnabled(idle and can_open)
            self.history_open_folder_button.setEnabled(idle)

        def _load_history_from_table(self) -> None:
            root = self._selected_history_path()
            if root is not None:
                self._load_history_path(root)

        def _load_history_path(self, root: Path) -> None:
            """Open one immutable historical run in its own Results tab."""
            if self.worker is not None:
                return
            try:
                loaded = load_history_execution(root, self.catalog)
                key = os.path.normcase(str(loaded.root))
                existing = self.history_run_pages.get(key)
                if existing is not None:
                    index = self.history_workspace_tabs.indexOf(existing)
                    if index >= 0:
                        self.tabs.setCurrentIndex(self.history_top_tab_index)
                        self.history_workspace_tabs.setCurrentIndex(index)
                        self.statusBar().showMessage(
                            f"Ejecución histórica enfocada: {loaded.root.name}"
                        )
                        return
                    self.history_run_pages.pop(key, None)
                page = self._create_history_run_page(loaded, key)
                label = f"{loaded.root.name[:15]} · {loaded.status}"
                index = self.history_workspace_tabs.addTab(page, label)
                self.history_workspace_tabs.setTabToolTip(index, str(loaded.root))
                self.history_run_pages[key] = page
                self.tabs.setCurrentIndex(self.history_top_tab_index)
                self.history_workspace_tabs.setCurrentIndex(index)
                self.statusBar().showMessage(
                    f"Ejecución histórica abierta: {loaded.root.name}"
                )
            except Exception as error:
                self._show_error("No se pudo cargar la ejecución", error)

        def _create_history_run_page(
            self, loaded: HistoricalExecution, page_key: str
        ) -> QWidget:
            """Build an actionable projection of one immutable saved run."""
            page = QWidget()
            layout = QVBoxLayout(page)
            record = history_record(loaded.root)
            counts = Counter(outcome.status for outcome in loaded.outcomes)
            selection = loaded.metadata["selection"]
            header = QLabel(
                f"{loaded.root.name} · {loaded.status} · {record[3]}\n"
                f"{selection['scope']} {selection['value']} · {selection['profile']} · "
                f"{selection['build_mode']} · {selection['optimization']} · "
                + " · ".join(
                    f"{status}: {counts[status]:,}"
                    for status in sorted(counts)
                )
            )
            header.setWordWrap(True)
            layout.addWidget(header)

            run_tabs = QTabWidget()
            run_tabs.setObjectName("history_run_sections")
            tests_page = QWidget()
            tests_layout = QVBoxLayout(tests_page)
            actions = QHBoxLayout()
            rerun_selected_button = QPushButton("Reejecutar prueba seleccionada")
            rerun_selected_button.setObjectName("history_rerun_selected")
            rerun_selected_button.setEnabled(False)
            rerun_failures_button = QPushButton("Reejecutar fallos")
            rerun_failures_button.setObjectName("history_rerun_failures")
            has_retryable_failures = bool(
                retry_variants(loaded.variants, loaded.outcomes)
            )
            rerun_failures_button.setProperty(
                "hasRetryableFailures", has_retryable_failures
            )
            rerun_failures_button.setEnabled(has_retryable_failures)
            rerun_all_button = QPushButton("Reejecutar corrida completa")
            rerun_all_button.setObjectName("history_rerun_all")
            failures_button = QPushButton("Mostrar sólo fallos")
            open_button = QPushButton("Abrir carpeta de evidencia")
            close_button = QPushButton("Cerrar corrida")
            actions.addWidget(rerun_selected_button)
            actions.addWidget(rerun_failures_button)
            actions.addWidget(rerun_all_button)
            actions.addWidget(failures_button)
            actions.addStretch(1)
            actions.addWidget(open_button)
            actions.addWidget(close_button)
            tests_layout.addLayout(actions)

            filters = QHBoxLayout()
            status_combo = QComboBox()
            for label, value in (
                ("Todos los estados", "ALL"),
                ("Fallos", "FAILURES"),
                ("PASS", "PASS"),
                ("FAIL", "FAIL"),
                ("KNOWN_FAILURE", "KNOWN_FAILURE"),
                ("BLOCKED", "BLOCKED"),
                ("INFRASTRUCTURE_ERROR", "INFRASTRUCTURE_ERROR"),
                ("UNEXPECTED_PASS", "UNEXPECTED_PASS"),
                ("CANCELLED", "CANCELLED"),
                ("TIMEOUT", "TIMEOUT"),
                ("PENDIENTE", "PENDIENTE"),
                ("FILTRADO", "FILTRADO"),
            ):
                status_combo.addItem(label, value)
            component_combo = QComboBox()
            component_combo.addItem("Todos los componentes", "ALL")
            for component in sorted({case.component for case in loaded.cases}):
                component_combo.addItem(component, component)
            time_combo = QComboBox()
            for label, value in (
                ("Cualquier momento", "ALL"),
                ("Primer minuto", "LT60"),
                ("1 a 5 min", "60_300"),
                ("5 a 15 min", "300_900"),
                ("15 a 30 min", "900_1800"),
                ("Después de 30 min", "GE1800"),
                ("Sin hora", "NONE"),
            ):
                time_combo.addItem(label, value)
            duration_combo = QComboBox()
            for label, value in (
                ("Cualquier duración", "ALL"),
                ("Menos de 1 s", "LT1"),
                ("1 a 5 s", "1_5"),
                ("5 a 30 s", "5_30"),
                ("30 a 60 s", "30_60"),
                ("60 s o más", "GE60"),
                ("Sin duración", "NONE"),
            ):
                duration_combo.addItem(label, value)
            search_edit = QLineEdit()
            search_edit.setPlaceholderText("Buscar caso, contrato, componente o detalle")
            visible_label = QLabel()
            filters.addWidget(QLabel("Estado"))
            filters.addWidget(status_combo)
            filters.addWidget(QLabel("Componente"))
            filters.addWidget(component_combo)
            filters.addWidget(QLabel("Tiempo"))
            filters.addWidget(time_combo)
            filters.addWidget(QLabel("Duración"))
            filters.addWidget(duration_combo)
            filters.addWidget(search_edit, 1)
            tests_layout.addLayout(filters)
            tests_layout.addWidget(visible_label)

            history_headers = (
                *self.TABLE_HEADERS[: self.DURATION_COLUMN],
                "Hora",
                *self.TABLE_HEADERS[self.DURATION_COLUMN :],
            )
            table = QTableWidget(0, len(history_headers))
            table.setObjectName("history_test_table")
            table.setHorizontalHeaderLabels(history_headers)
            table.setAlternatingRowColors(True)
            table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
            table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
            table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
            table.setSortingEnabled(False)
            table.setUpdatesEnabled(False)
            table.horizontalHeader().setSectionResizeMode(
                QHeaderView.ResizeMode.Interactive
            )
            for column, width in enumerate(
                (55, 185, 165, 175, 70, 55, 75, 175, 90, 125, 120, 185, 90)
            ):
                table.horizontalHeader().resizeSection(column, width)
            table.horizontalHeader().setStretchLastSection(True)

            event_text = ""
            finished_at_by_key: dict[str, tuple[str, float | None]] = {}
            run_started_at: datetime | None = None
            events_path = loaded.root / "events.jsonl"
            if events_path.is_file():
                event_text = events_path.read_text(encoding="utf-8", errors="replace")
                for line in event_text.splitlines():
                    if not line.strip():
                        continue
                    event = json.loads(line, object_pairs_hook=unique_json_object)
                    timestamp = event.get("timestamp")
                    if not isinstance(timestamp, str):
                        continue
                    parsed = datetime.fromisoformat(timestamp)
                    if event.get("kind") == "RUN_STARTED":
                        run_started_at = parsed
                    elif event.get("kind") == "CASE_FINISHED":
                        payload = event.get("payload")
                        if not isinstance(payload, dict):
                            continue
                        event_key = payload.get("key")
                        if not isinstance(event_key, str):
                            continue
                        elapsed = (
                            (parsed - run_started_at).total_seconds()
                            if run_started_at is not None
                            else None
                        )
                        finished_at_by_key[event_key] = (timestamp, elapsed)

            outcome_by_key = {outcome.key: outcome for outcome in loaded.outcomes}
            variant_by_key = {variant.key: variant for variant in loaded.variants}
            case_by_id = {case.case_id: case for case in loaded.cases}
            executable_ids = {variant.case.case_id for variant in loaded.variants}
            rows: list[
                tuple[
                    CaseRecord,
                    str,
                    str,
                    str,
                    str,
                    float | None,
                    str,
                    str,
                    float | None,
                    bool,
                ]
            ] = []
            for variant in loaded.variants:
                outcome = outcome_by_key[variant.key]
                detail = outcome.detail
                if outcome.evidence:
                    detail = (
                        f"{detail} | {outcome.evidence}"
                        if detail
                        else outcome.evidence
                    )
                timestamp, elapsed = finished_at_by_key.get(variant.key, ("", None))
                rows.append(
                    (
                        variant.case,
                        variant.build_mode,
                        variant.optimization,
                        variant.key,
                        outcome.status,
                        outcome.duration_seconds,
                        detail,
                        timestamp,
                        elapsed,
                        True,
                    )
                )
            for case in loaded.cases:
                if not case.executable:
                    rows.append(
                        (
                            case,
                            "NA",
                            "NA",
                            f"{case.case_id}|NA|NA",
                            "PENDIENTE",
                            None,
                            case.disposition,
                            "",
                            None,
                            False,
                        )
                    )
                elif case.case_id not in executable_ids:
                    rows.append(
                        (
                            case,
                            "NA",
                            "NA",
                            f"{case.case_id}|FILTERED|FILTERED",
                            "FILTRADO",
                            None,
                            "la combinación Build/Opt elegida no está soportada por el caso",
                            "",
                            None,
                            False,
                        )
                    )

            table.setRowCount(len(rows))
            history_time_column = self.DURATION_COLUMN
            history_duration_column = self.DURATION_COLUMN + 1
            history_detail_column = self.DETAIL_COLUMN + 1
            for row_index, (
                case,
                mode,
                opt,
                variant_key,
                status,
                duration,
                detail,
                timestamp,
                elapsed,
                executable,
            ) in enumerate(rows):
                values = (
                    str(case.wave),
                    case.component,
                    case.contract_id,
                    case.case_id,
                    mode,
                    opt,
                    case.profile,
                    case.executor,
                    row_progress_text(status),
                    status,
                    row_action_text(status, executable),
                    timestamp.replace("T", " ") if timestamp else "",
                    "",
                    detail,
                )
                for column, value in enumerate(values):
                    item = QTableWidgetItem(value)
                    if column == self.CASE_COLUMN:
                        item.setData(Qt.ItemDataRole.UserRole, variant_key)
                        item.setToolTip(
                            "Doble clic para abrir el escenario, el oráculo y la salida completa"
                        )
                    elif column == self.STATUS_COLUMN:
                        self._color_status(item, status)
                    elif column == self.ACTION_COLUMN and executable:
                        item.setForeground(QColor("#70a7ff"))
                        item.setToolTip("Crear una corrida nueva de esta variante exacta")
                    elif column == history_time_column:
                        item.setData(Qt.ItemDataRole.UserRole, elapsed)
                    elif column == history_duration_column:
                        item.setData(Qt.ItemDataRole.UserRole, duration)
                        if duration is not None:
                            item.setData(
                                Qt.ItemDataRole.DisplayRole, round(duration, 6)
                            )
                            item.setToolTip(f"{duration:.6f} segundos")
                    table.setItem(row_index, column, item)
            table.setUpdatesEnabled(True)
            table.setSortingEnabled(True)
            tests_layout.addWidget(table, 1)

            def matches_bucket(value: float | None, bucket: str, kind: str) -> bool:
                if bucket == "ALL":
                    return True
                if bucket == "NONE":
                    return value is None
                if value is None:
                    return False
                if kind == "time":
                    if bucket == "LT60":
                        return value < 60.0
                    if bucket == "60_300":
                        return 60.0 <= value < 300.0
                    if bucket == "300_900":
                        return 300.0 <= value < 900.0
                    if bucket == "900_1800":
                        return 900.0 <= value < 1800.0
                    return value >= 1800.0
                if bucket == "LT1":
                    return value < 1.0
                if bucket == "1_5":
                    return 1.0 <= value < 5.0
                if bucket == "5_30":
                    return 5.0 <= value < 30.0
                if bucket == "30_60":
                    return 30.0 <= value < 60.0
                return value >= 60.0

            def apply_filters() -> None:
                status_filter = str(status_combo.currentData())
                component_filter = str(component_combo.currentData())
                time_filter = str(time_combo.currentData())
                duration_filter = str(duration_combo.currentData())
                needle = search_edit.text().strip().casefold()
                visible = 0
                for row_index in range(table.rowCount()):
                    status_item = table.item(row_index, self.STATUS_COLUMN)
                    component_item = table.item(row_index, 1)
                    time_item = table.item(row_index, history_time_column)
                    duration_item = table.item(row_index, history_duration_column)
                    status = status_item.text() if status_item is not None else ""
                    component = (
                        component_item.text() if component_item is not None else ""
                    )
                    elapsed = (
                        time_item.data(Qt.ItemDataRole.UserRole)
                        if time_item is not None
                        else None
                    )
                    duration_value = (
                        duration_item.data(Qt.ItemDataRole.UserRole)
                        if duration_item is not None
                        else None
                    )
                    searchable = " ".join(
                        table.item(row_index, column).text()
                        for column in (
                            0,
                            1,
                            2,
                            3,
                            4,
                            5,
                            6,
                            7,
                            self.STATUS_COLUMN,
                            history_detail_column,
                        )
                        if table.item(row_index, column) is not None
                    ).casefold()
                    show = (
                        status_filter_matches(status_filter, status)
                        and (component_filter == "ALL" or component == component_filter)
                        and matches_bucket(elapsed, time_filter, "time")
                        and matches_bucket(duration_value, duration_filter, "duration")
                        and (not needle or needle in searchable)
                    )
                    table.setRowHidden(row_index, not show)
                    visible += int(show)
                visible_label.setText(
                    f"Mostrando {visible:,} de {table.rowCount():,} filas guardadas; "
                    "los encabezados permiten ordenar, incluida la duración numérica."
                )

            action_timer = QTimer(table)
            action_timer.setSingleShot(True)
            pending_action: dict[str, str | float | None] = {
                "key": None,
                "suppressed_until": 0.0,
            }

            def execute_pending_action() -> None:
                variant_key = pending_action["key"]
                pending_action["key"] = None
                if not isinstance(variant_key, str) or self.worker is not None:
                    return
                variant = variant_by_key.get(variant_key)
                if variant is not None:
                    self._start_historical_variant(variant)

            action_timer.timeout.connect(execute_pending_action)

            def run_exact(row: int, column: int) -> None:
                if (
                    column != self.ACTION_COLUMN
                    or self.worker is not None
                    or time.monotonic()
                    < float(pending_action["suppressed_until"] or 0.0)
                ):
                    return
                item = table.item(row, self.CASE_COLUMN)
                variant_key = (
                    item.data(Qt.ItemDataRole.UserRole) if item is not None else None
                )
                if isinstance(variant_key, str) and variant_key in variant_by_key:
                    pending_action["key"] = variant_key
                    action_timer.start(QApplication.doubleClickInterval() + 50)

            def open_detail(row: int, _column: int) -> None:
                action_timer.stop()
                pending_action["key"] = None
                pending_action["suppressed_until"] = time.monotonic() + 0.5
                case_item = table.item(row, self.CASE_COLUMN)
                if case_item is None:
                    return
                case = case_by_id.get(case_item.text())
                variant_key = case_item.data(Qt.ItemDataRole.UserRole)
                if case is None or not isinstance(variant_key, str):
                    return
                self._open_case_detail_tab(
                    run_tabs,
                    case,
                    variant_by_key.get(variant_key),
                    outcome_by_key.get(variant_key),
                    loaded.root,
                )

            def selected_variant() -> RunVariant | None:
                row = table.currentRow()
                if row < 0:
                    return None
                item = table.item(row, self.CASE_COLUMN)
                variant_key = (
                    item.data(Qt.ItemDataRole.UserRole) if item is not None else None
                )
                return (
                    variant_by_key.get(variant_key)
                    if isinstance(variant_key, str)
                    else None
                )

            def update_selected_action() -> None:
                has_executable_selection = selected_variant() is not None
                rerun_selected_button.setProperty(
                    "hasExecutableSelection", has_executable_selection
                )
                rerun_selected_button.setEnabled(
                    self.worker is None and has_executable_selection
                )

            def rerun_selected() -> None:
                variant = selected_variant()
                if variant is not None:
                    self._start_historical_variant(variant)

            for widget_signal in (
                status_combo.currentIndexChanged,
                component_combo.currentIndexChanged,
                time_combo.currentIndexChanged,
                duration_combo.currentIndexChanged,
            ):
                widget_signal.connect(lambda _value: apply_filters())
            search_edit.textChanged.connect(lambda _text: apply_filters())
            table.cellClicked.connect(run_exact)
            table.cellDoubleClicked.connect(open_detail)
            table.itemSelectionChanged.connect(update_selected_action)
            rerun_selected_button.clicked.connect(rerun_selected)
            rerun_failures_button.clicked.connect(
                lambda: self._start_historical_failures(loaded)
            )
            rerun_all_button.clicked.connect(
                lambda: self._start_historical_run(loaded)
            )
            failures_button.clicked.connect(
                lambda: status_combo.setCurrentIndex(
                    max(0, status_combo.findData("FAILURES"))
                )
            )
            open_button.clicked.connect(
                lambda: QDesktopServices.openUrl(QUrl.fromLocalFile(str(loaded.root)))
            )
            close_button.clicked.connect(
                lambda: self._close_history_run_tab(page_key, page)
            )
            run_tabs.addTab(tests_page, f"Pruebas ({len(rows):,})")

            report_page = QWidget()
            report_layout = QVBoxLayout(report_page)
            report_intro = QLabel(
                f"Evidencia inmutable de {loaded.root.name}\n{loaded.root}"
            )
            report_intro.setWordWrap(True)
            report_layout.addWidget(report_intro)
            report_tabs = QTabWidget()
            report_tabs.setObjectName("history_report_sections")

            def add_text_tab(title: str, contents: str) -> None:
                view = QPlainTextEdit()
                view.setReadOnly(True)
                view.setPlainText(contents)
                report_tabs.addTab(view, title)

            summary_path = loaded.root / "summary.md"
            add_text_tab(
                "Resumen",
                summary_path.read_text(encoding="utf-8", errors="replace"),
            )
            add_text_tab(
                "Errores condensados",
                condensed_error_report(
                    loaded.outcomes,
                    loaded.status == "CANCELLED",
                    str(loaded.root),
                ),
            )
            add_text_tab("Log completo", loaded.full_log)
            add_text_tab("Eventos", event_text)
            add_text_tab(
                "Resultados JSON",
                (loaded.root / "results.json").read_text(
                    encoding="utf-8", errors="replace"
                ),
            )
            add_text_tab(
                "Metadatos",
                json.dumps(loaded.metadata, ensure_ascii=False, indent=2),
            )
            report_layout.addWidget(report_tabs, 1)
            run_tabs.addTab(report_page, "Reporte")
            layout.addWidget(run_tabs, 1)
            apply_filters()
            return page

        def _close_history_run_tab(self, page_key: str, page: QWidget) -> None:
            """Close only the selected historical view; evidence remains on disk."""
            index = self.history_workspace_tabs.indexOf(page)
            if index >= 0:
                self.history_workspace_tabs.removeTab(index)
            self.history_run_pages.pop(page_key, None)

        def _start_historical_variant(self, variant: RunVariant) -> None:
            """Create a new exact run from a historical row without editing that tab."""
            if self.worker is not None:
                return
            self.scope_combo.setCurrentText("Caso")
            self._refresh_scope_values()
            self.scope_value.setCurrentText(variant.case.case_id)
            self.profile_combo.setCurrentText("Todos")
            self.execution_mode.setCurrentText(variant.build_mode)
            requested_optimization = (
                "O0" if variant.optimization == "NA" else variant.optimization
            )
            self.optimization_combo.setCurrentText(requested_optimization)
            self.visible_variants = (variant,)
            self._populate_plan((variant.case,), (variant,))
            self.tabs.setCurrentIndex(1)
            self._start_exact_variant(variant)

        def _start_historical_run(self, loaded: HistoricalExecution) -> None:
            """Create a new run from the exact selection stored by a prior run."""
            if self.worker is not None:
                return
            try:
                selection = loaded.metadata["selection"]
                scope = str(selection["scope"])
                self.scope_combo.setCurrentText(scope)
                self._refresh_scope_values()
                if scope != "Total":
                    self.scope_value.setCurrentText(str(selection["value"]))
                self.profile_combo.setCurrentText(str(selection["profile"]))
                self.execution_mode.setCurrentText(str(selection["build_mode"]))
                self.optimization_combo.setCurrentText(
                    str(selection["optimization"])
                )
                config = self._config_from_ui().validated(require_tools=True)
                config.save()
                if not loaded.variants:
                    raise ValueError(
                        "la corrida histórica no contiene variantes ejecutables"
                    )
                self._populate_plan(loaded.cases, loaded.variants)
                self.tabs.setCurrentIndex(1)
                self._run_variants(
                    config,
                    loaded.variants,
                    f"historical-full:{loaded.root.name}",
                )
            except Exception as error:
                self._show_error("No se pudo reejecutar la corrida completa", error)

        def _start_historical_failures(self, loaded: HistoricalExecution) -> None:
            """Create a new run from only the exact failures of a prior run."""
            if self.worker is not None:
                return
            try:
                variants = retry_variants(loaded.variants, loaded.outcomes)
                if not variants:
                    raise ValueError(
                        "la corrida histórica no contiene fallos reejecutables"
                    )
                selection = loaded.metadata["selection"]
                scope = str(selection["scope"])
                self.scope_combo.setCurrentText(scope)
                self._refresh_scope_values()
                if scope != "Total":
                    self.scope_value.setCurrentText(str(selection["value"]))
                self.profile_combo.setCurrentText(str(selection["profile"]))
                self.execution_mode.setCurrentText(str(selection["build_mode"]))
                self.optimization_combo.setCurrentText(
                    str(selection["optimization"])
                )
                config = self._config_from_ui().validated(require_tools=True)
                config.save()
                self._populate_plan(
                    tuple(variant.case for variant in variants), variants
                )
                self.tabs.setCurrentIndex(1)
                self._run_variants(
                    config,
                    variants,
                    f"historical-failures:{loaded.root.name}",
                )
            except Exception as error:
                self._show_error("No se pudieron reejecutar los fallos", error)

        def _open_history(self) -> None:
            root = self._selected_history_path()
            if root is not None:
                QDesktopServices.openUrl(QUrl.fromLocalFile(str(root)))

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
                "TIMEOUT": QColor("#ff7777"),
                "PENDIENTE": QColor("#d9b85f"),
                "LISTO": QColor("#8cb4ff"),
                "DESACTIVADA": QColor("#b7b7b7"),
                "PASSED": QColor("#73d673"),
                "FAILED": QColor("#ff7777"),
                "COMPLETED_WITH_KNOWN_DEFECTS": QColor("#f0a35e"),
                "INCOMPLETE": QColor("#b7b7b7"),
                "MALFORMED": QColor("#ff7777"),
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
        expected_buttons = {
            "run_button": "Ejecutar pruebas",
            "cancel_button": "Cancelar corrida/árbol",
            "clear_results_button": "Limpiar resultados para nueva corrida",
            "history_load_button": "Abrir corrida completa en un tab",
            "failure_retry_button": "Reejecutar fallos activos",
            "failure_disable_button": "Desactivar seleccionados para reejecución",
            "failure_enable_button": "Reactivar seleccionados",
        }
        for attribute, expected_text in expected_buttons.items():
            button = getattr(window, attribute, None)
            if button is None or button.text() != expected_text:
                raise RuntimeError(
                    f"GUI smoke missing {attribute} with text {expected_text!r}"
                )
        if window.result_tabs.tabText(window.failure_tab_index) != "Fallos exactos (0)":
            raise RuntimeError("GUI smoke missing the exact-failure result tab")
        primary_tabs = [
            window.tabs.tabText(index) for index in range(window.tabs.count())
        ]
        if primary_tabs != ["Construcción", "Pruebas", "Resultados", "Historial"]:
            raise RuntimeError(
                "GUI smoke missing the Construcción/Pruebas/Resultados/Historial hierarchy"
            )
        if any(
            window.result_tabs.tabText(index) == "Historial"
            for index in range(window.result_tabs.count())
        ):
            raise RuntimeError("GUI smoke found history mixed into current results")
        if (
            window.history_workspace_tabs.tabText(
                window.history_launcher_tab_index
            )
            != "Corridas guardadas"
        ):
            raise RuntimeError("GUI smoke missing the history run-list panel")
        headers = [
            window.case_table.horizontalHeaderItem(index).text()
            for index in range(window.case_table.columnCount())
        ]
        if (
            "Progreso" not in headers
            or "Estado" not in headers
            or "Acción" not in headers
            or "Duración" not in headers
        ):
            raise RuntimeError("GUI smoke missing per-row progress columns")
        if window.case_table.contextMenuPolicy() != Qt.ContextMenuPolicy.CustomContextMenu:
            raise RuntimeError("GUI smoke missing exact-row context actions")
        if window.execution_activity is None or window.execution_counts is None:
            raise RuntimeError("GUI smoke missing live execution summaries")
        if window.status_filter_combo.count() < 10:
            raise RuntimeError("GUI smoke missing the primary status filters")
        if window.status_filter_combo.findData("TIMEOUT") < 0:
            raise RuntimeError("GUI smoke missing the TIMEOUT status filter")
        valid_history = next(
            (
                root
                for root in sorted(HISTORY_ROOT.iterdir(), reverse=True)
                if root.is_dir()
                and history_record(root)[1] == "phase-2-suite"
                and history_record(root)[2] not in {"INCOMPLETE", "MALFORMED"}
            ),
            None,
        ) if HISTORY_ROOT.is_dir() else None
        if valid_history is not None:
            live_signature = (
                window.case_table.rowCount(),
                dict(window.live_status_counts),
                window.full_log.toPlainText(),
                window.error_report.toPlainText(),
            )
            prior_tab_count = window.history_workspace_tabs.count()
            window._load_history_path(valid_history)
            if window.history_workspace_tabs.count() != prior_tab_count + 1:
                raise RuntimeError("GUI smoke did not open a dedicated historical tab")
            if window.tabs.currentIndex() != window.history_top_tab_index:
                raise RuntimeError("GUI smoke did not keep the run inside Historial")
            if len(window.history_run_pages) != 1:
                raise RuntimeError("GUI smoke did not retain the historical page identity")
            historical_page = next(iter(window.history_run_pages.values()))
            run_sections = historical_page.findChild(
                QTabWidget, "history_run_sections"
            )
            if (
                run_sections is None
                or run_sections.count() != 2
                or not run_sections.tabText(0).startswith("Pruebas (")
                or run_sections.tabText(1) != "Reporte"
            ):
                raise RuntimeError(
                    "GUI smoke missing the Pruebas/Reporte historical hierarchy"
                )
            report_sections = historical_page.findChild(
                QTabWidget, "history_report_sections"
            )
            required_report_tabs = {
                "Resumen",
                "Errores condensados",
                "Log completo",
                "Eventos",
                "Resultados JSON",
                "Metadatos",
            }
            if report_sections is None or {
                report_sections.tabText(index)
                for index in range(report_sections.count())
            } != required_report_tabs:
                raise RuntimeError("GUI smoke missing complete nested report tabs")
            if historical_page.findChild(
                QPushButton, "history_rerun_selected"
            ) is None or historical_page.findChild(
                QPushButton, "history_rerun_failures"
            ) is None or historical_page.findChild(
                QPushButton, "history_rerun_all"
            ) is None:
                raise RuntimeError("GUI smoke missing visible historical rerun actions")
            restored_signature = (
                window.case_table.rowCount(),
                dict(window.live_status_counts),
                window.full_log.toPlainText(),
                window.error_report.toPlainText(),
            )
            if restored_signature != live_signature:
                raise RuntimeError("historical loading mutated the live test workspace")
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
            return 0
        return launch_gui(smoke_only=arguments.gui_smoke)
    except Exception as error:
        print(f"nscbuilder3: error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())


