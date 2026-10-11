"""Builder configuration parsing, discovery, validation, and persistence."""

from __future__ import annotations

from ._runtime import legacy


_runtime = legacy()

BuilderConfig = _runtime.BuilderConfig
parse_builder_config_payload = _runtime.parse_builder_config_payload
discover_archive_executable = _runtime.discover_archive_executable
discover_build_tools = _runtime.discover_build_tools
discover_compiler_build_tools = _runtime.discover_compiler_build_tools
first_distinct_existing_file = _runtime.first_distinct_existing_file
first_existing_directory = _runtime.first_existing_directory
first_existing_file = _runtime.first_existing_file

__all__ = (
    "BuilderConfig",
    "discover_archive_executable",
    "discover_build_tools",
    "discover_compiler_build_tools",
    "first_distinct_existing_file",
    "first_existing_directory",
    "first_existing_file",
    "parse_builder_config_payload",
)
