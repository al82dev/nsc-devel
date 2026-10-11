"""Canonical repository, suite, inventory, storage, and validation paths."""

from __future__ import annotations

from pathlib import Path
from typing import Iterable

from .constants import APP_NAME, DEFAULT_COMPILER_LLVM_ROOT


SOURCE_ROOT = Path(__file__).resolve().parents[2]
MODULAR_ROOT = Path(__file__).resolve().parents[1]
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
EXPECTED_OUTCOME_OVERRIDES_PATH = MODULAR_ROOT / "metadata" / "expected-outcomes.json"


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


__all__ = tuple(
    name
    for name in globals()
    if name.isupper()
    or name.startswith("require_")
    or name.startswith("first_")
    or name == "is_a_drive"
)
