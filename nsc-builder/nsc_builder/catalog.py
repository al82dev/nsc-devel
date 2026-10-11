"""Canonical manifest, design, allocation, selection, and matrix contracts."""

from __future__ import annotations

from ._runtime import legacy


_runtime = legacy()

CanonicalCatalog = _runtime.CanonicalCatalog
expand_variants = _runtime.expand_variants
parse_wave_number = _runtime.parse_wave_number
read_catalog_csv = _runtime.read_catalog_csv
read_csv = _runtime.read_csv
read_jsonl = _runtime.read_jsonl
split_semicolon = _runtime.split_semicolon
unique_json_object = _runtime.unique_json_object
validate_allocation_domains = _runtime.validate_allocation_domains
validate_executable_manifest = _runtime.validate_executable_manifest

__all__ = (
    "CanonicalCatalog",
    "expand_variants",
    "parse_wave_number",
    "read_catalog_csv",
    "read_csv",
    "read_jsonl",
    "split_semicolon",
    "unique_json_object",
    "validate_allocation_domains",
    "validate_executable_manifest",
)
