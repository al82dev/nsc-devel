"""Shared serialization, receipts, clocks, revision, and audit helpers."""

from __future__ import annotations

from ._runtime import legacy


_runtime = legacy()

catalog_audit = _runtime.catalog_audit
compiler_receipt = _runtime.compiler_receipt
current_revision = _runtime.current_revision
file_receipt = _runtime.file_receipt
make_run_id = _runtime.make_run_id
sha256_file = _runtime.sha256_file
utc_now = _runtime.utc_now
write_json = _runtime.write_json
write_json_atomic = _runtime.write_json_atomic
write_text_atomic = _runtime.write_text_atomic

__all__ = (
    "catalog_audit",
    "compiler_receipt",
    "current_revision",
    "file_receipt",
    "make_run_id",
    "sha256_file",
    "utc_now",
    "write_json",
    "write_json_atomic",
    "write_text_atomic",
)
