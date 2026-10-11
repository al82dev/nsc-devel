"""Canonical repository, suite, inventory, storage, and tool paths."""

from __future__ import annotations

from ._runtime import legacy


_runtime = legacy()

APP_NAME = _runtime.APP_NAME
SOURCE_ROOT = _runtime.SOURCE_ROOT
SUITE_ROOT = _runtime.SUITE_ROOT
INVENTORY_ROOT = _runtime.INVENTORY_ROOT
STORAGE_ROOT = _runtime.STORAGE_ROOT
HISTORY_ROOT = _runtime.HISTORY_ROOT
WORK_ROOT = _runtime.WORK_ROOT
CONFIG_PATH = _runtime.CONFIG_PATH
MANIFEST_PATH = _runtime.MANIFEST_PATH
DESIGN_PATH = _runtime.DESIGN_PATH
ALLOCATION_PATH = _runtime.ALLOCATION_PATH
WAVES_PATH = _runtime.WAVES_PATH
COMPONENTS_PATH = _runtime.COMPONENTS_PATH
CONTRACTS_PATH = _runtime.CONTRACTS_PATH
CONTROLLER_ROOT = _runtime.CONTROLLER_ROOT
RUN_SUITE_PATH = _runtime.RUN_SUITE_PATH
DEFAULT_COMPILER_LLVM_ROOT = _runtime.DEFAULT_COMPILER_LLVM_ROOT
first_distinct_existing_file = _runtime.first_distinct_existing_file
first_existing_directory = _runtime.first_existing_directory
first_existing_file = _runtime.first_existing_file
is_a_drive = _runtime.is_a_drive
require_a_path = _runtime.require_a_path
require_directory_path = _runtime.require_directory_path
require_file_path = _runtime.require_file_path

__all__ = (
    "ALLOCATION_PATH",
    "APP_NAME",
    "COMPONENTS_PATH",
    "CONFIG_PATH",
    "CONTRACTS_PATH",
    "CONTROLLER_ROOT",
    "DEFAULT_COMPILER_LLVM_ROOT",
    "DESIGN_PATH",
    "HISTORY_ROOT",
    "INVENTORY_ROOT",
    "MANIFEST_PATH",
    "RUN_SUITE_PATH",
    "SOURCE_ROOT",
    "STORAGE_ROOT",
    "SUITE_ROOT",
    "WAVES_PATH",
    "WORK_ROOT",
    "first_distinct_existing_file",
    "first_existing_directory",
    "first_existing_file",
    "is_a_drive",
    "require_a_path",
    "require_directory_path",
    "require_file_path",
)
