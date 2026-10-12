"""Complete ownership inventory for the extracted single-file implementation."""

from __future__ import annotations


MODULE_OWNERSHIP = {
    "configuration": {
        "BuilderConfig",
        "discover_archive_executable",
        "parse_builder_config_payload",
    },
    "toolchain": {
        "_llvm_config_version",
        "_llvm_producer_identity",
        "discover_build_tools",
        "discover_compiler_build_tools",
        "discover_llvm_toolchain",
        "llvm_root_from_configured_path",
        "selected_llvm_root",
        "validate_llvm_toolchain",
    },
    "models": {
        "CaseOutcome",
        "CaseRecord",
        "HistoricalExecution",
        "RunVariant",
        "TestTreeCleanupResult",
        "VariantTimeoutBudget",
    },
    "catalog": {
        "CanonicalCatalog",
        "expand_variants",
        "parse_wave_number",
        "read_catalog_csv",
        "read_csv",
        "read_jsonl",
        "split_semicolon",
        "validate_allocation_domains",
        "validate_executable_manifest",
    },
    "paths": {
        "first_distinct_existing_file",
        "first_existing_directory",
        "first_existing_file",
        "is_a_drive",
        "require_a_path",
        "require_directory_path",
        "require_file_path",
    },
    "process_control": {"ProcessController"},
    "history": {
        "EvidenceStore",
        "history_record",
        "load_history_execution",
        "load_variant_timeout_budgets",
        "timeout_limit_for_average",
        "variant_history_key",
    },
    "reporting": {
        "active_retry_variants",
        "case_contract_payload",
        "case_diagnostic_summary",
        "case_evidence_output",
        "condensed_error_report",
        "retry_variants",
        "row_action_text",
        "row_progress_text",
        "status_filter_matches",
    },
    "build": {
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
        "effective_build_dir",
        "host_executable",
        "test_tree_cleanup_targets",
    },
    "execution": {
        "ExecutionEngine",
        "blocked_infrastructure_outcomes",
        "cancelled_infrastructure_outcomes",
        "equal_json_exact",
        "expected_abnormal_termination_detail",
        "infrastructure_command_outcome",
        "infrastructure_failure_detail",
        "parse_host_envelope",
        "parse_identity_envelope",
        "parse_python_gate_result",
        "parse_selfhost_inputs",
        "parse_selfhost_json_output",
        "parse_selfhost_oracle",
    },
    "runtime_support": {
        "catalog_audit",
        "compiler_receipt",
        "file_receipt",
        "make_run_id",
    },
    "utils": {
        "current_revision",
        "sha256_file",
        "utc_now",
        "unique_json_object",
        "write_json",
        "write_json_atomic",
        "write_text_atomic",
    },
    "gui": {"launch_gui"},
    "cli": {"main"},
}

EXPECTED_TOP_LEVEL = frozenset().union(*MODULE_OWNERSHIP.values())

VISIBLE_STATE_GROUPS = {
    "terminal_failures": frozenset(
        {
            "BLOCKED",
            "CANCELLED",
            "FAIL",
            "INFRASTRUCTURE_ERROR",
            "NOT_RUN",
            "TIMEOUT",
        }
    ),
    "successful": frozenset({"PASS"}),
    "live": frozenset({"COMPILING", "RUNNING"}),
    "waiting": frozenset({"LISTO", "PENDIENTE"}),
}

__all__ = ("EXPECTED_TOP_LEVEL", "MODULE_OWNERSHIP", "VISIBLE_STATE_GROUPS")
