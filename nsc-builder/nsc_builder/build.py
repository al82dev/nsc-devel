"""Test-tree cleanup, compiler/test build commands, and toolchain paths."""

from __future__ import annotations

from ._runtime import legacy


_runtime = legacy()

TestTreeCleanupResult = _runtime.TestTreeCleanupResult
_ensure_plain_path = _runtime._ensure_plain_path
_has_reparse_attribute = _runtime._has_reparse_attribute
_paths_overlap = _runtime._paths_overlap
build_commands = _runtime.build_commands
clean_test_build_trees = _runtime.clean_test_build_trees
compiler_build_commands = _runtime.compiler_build_commands
compiler_build_dir = _runtime.compiler_build_dir
compiler_configure_commands = _runtime.compiler_configure_commands
compiler_output = _runtime.compiler_output
compiler_runtime_library = _runtime.compiler_runtime_library
component_test_executable = _runtime.component_test_executable
configure_commands = _runtime.configure_commands
destructive_host_executable = _runtime.destructive_host_executable
discover_archive_executable = _runtime.discover_archive_executable
discover_build_tools = _runtime.discover_build_tools
discover_compiler_build_tools = _runtime.discover_compiler_build_tools
effective_build_dir = _runtime.effective_build_dir
host_executable = _runtime.host_executable
test_tree_cleanup_targets = _runtime.test_tree_cleanup_targets

__all__ = (
    "TestTreeCleanupResult",
    "_ensure_plain_path",
    "_has_reparse_attribute",
    "_paths_overlap",
    "build_commands",
    "clean_test_build_trees",
    "compiler_build_commands",
    "compiler_build_dir",
    "compiler_configure_commands",
    "compiler_output",
    "compiler_runtime_library",
    "component_test_executable",
    "configure_commands",
    "destructive_host_executable",
    "discover_archive_executable",
    "discover_build_tools",
    "discover_compiler_build_tools",
    "effective_build_dir",
    "host_executable",
    "test_tree_cleanup_targets",
)
