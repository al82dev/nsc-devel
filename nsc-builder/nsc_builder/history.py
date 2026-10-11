"""Crash-surviving evidence, history restoration, and timeout statistics."""

from __future__ import annotations

from ._runtime import legacy


_runtime = legacy()

EvidenceStore = _runtime.EvidenceStore
VariantTimeoutBudget = _runtime.VariantTimeoutBudget
history_record = _runtime.history_record
load_history_execution = _runtime.load_history_execution
load_variant_timeout_budgets = _runtime.load_variant_timeout_budgets
timeout_limit_for_average = _runtime.timeout_limit_for_average
variant_history_key = _runtime.variant_history_key

__all__ = (
    "EvidenceStore",
    "VariantTimeoutBudget",
    "history_record",
    "load_history_execution",
    "load_variant_timeout_budgets",
    "timeout_limit_for_average",
    "variant_history_key",
)
