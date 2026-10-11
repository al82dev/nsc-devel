"""Stable Builder 3 names, states, and catalogue domains."""

from __future__ import annotations

import re
from pathlib import Path


APP_NAME = "NSC Builder 3"
DEFAULT_COMPILER_LLVM_ROOT = Path("C:/clang-llvm")
REQUIRED_LLVM_MAJOR = 23
LLVM_PRODUCER_NAMES = ("clang_cl", "lld_link", "llvm_rc", "llvm_mt")

PROFILE_RANK = {"PR": 0, "NIGHTLY": 1, "WEEKLY": 2}
TERMINAL_FAILURES = {
    "FAIL",
    "UNEXPECTED_PASS",
    "BLOCKED",
    "INFRASTRUCTURE_ERROR",
    "CANCELLED",
    "TIMEOUT",
    "NOT_RUN",
}
RESULT_STATUSES = TERMINAL_FAILURES | {"PASS", "KNOWN_FAILURE"}
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
    {"N/A", "O0", "O1", "O2", "O3", "FROZEN_CONTRACT"}
)


__all__ = tuple(name for name in globals() if name.isupper() and not name.startswith("_"))
