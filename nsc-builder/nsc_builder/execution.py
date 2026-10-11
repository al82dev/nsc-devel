"""Variant planning, oracle parsing, execution, cancellation, and outcomes."""

from __future__ import annotations

from ._runtime import legacy


_runtime = legacy()

ExecutionEngine = _runtime.ExecutionEngine
blocked_infrastructure_outcomes = _runtime.blocked_infrastructure_outcomes
cancelled_infrastructure_outcomes = _runtime.cancelled_infrastructure_outcomes
equal_json_exact = _runtime.equal_json_exact
expand_variants = _runtime.expand_variants
expected_abnormal_termination_detail = _runtime.expected_abnormal_termination_detail
infrastructure_command_outcome = _runtime.infrastructure_command_outcome
infrastructure_failure_detail = _runtime.infrastructure_failure_detail
parse_host_envelope = _runtime.parse_host_envelope
parse_identity_envelope = _runtime.parse_identity_envelope
parse_python_gate_result = _runtime.parse_python_gate_result
parse_selfhost_inputs = _runtime.parse_selfhost_inputs
parse_selfhost_json_output = _runtime.parse_selfhost_json_output
parse_selfhost_oracle = _runtime.parse_selfhost_oracle

__all__ = (
    "ExecutionEngine",
    "blocked_infrastructure_outcomes",
    "cancelled_infrastructure_outcomes",
    "equal_json_exact",
    "expand_variants",
    "expected_abnormal_termination_detail",
    "infrastructure_command_outcome",
    "infrastructure_failure_detail",
    "parse_host_envelope",
    "parse_identity_envelope",
    "parse_python_gate_result",
    "parse_selfhost_inputs",
    "parse_selfhost_json_output",
    "parse_selfhost_oracle",
)
