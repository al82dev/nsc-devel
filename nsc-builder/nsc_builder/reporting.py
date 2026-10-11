"""Human and machine projections of canonical test and execution records."""

from __future__ import annotations

from ._runtime import legacy


_runtime = legacy()

active_retry_variants = _runtime.active_retry_variants
case_contract_payload = _runtime.case_contract_payload
case_diagnostic_summary = _runtime.case_diagnostic_summary
case_evidence_output = _runtime.case_evidence_output
condensed_error_report = _runtime.condensed_error_report
retry_variants = _runtime.retry_variants
row_action_text = _runtime.row_action_text
row_progress_text = _runtime.row_progress_text
status_filter_matches = _runtime.status_filter_matches

__all__ = (
    "active_retry_variants",
    "case_contract_payload",
    "case_diagnostic_summary",
    "case_evidence_output",
    "condensed_error_report",
    "retry_variants",
    "row_action_text",
    "row_progress_text",
    "status_filter_matches",
)
