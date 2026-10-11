"""Mechanically extract owned top-level definitions into the modular package.

This development helper preserves each definition body verbatim. Module
ownership and dependency preambles are explicit below; the generated package
never imports the source script at runtime.
"""

from __future__ import annotations

import ast
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SOURCE = REPOSITORY_ROOT / "nscbuilder3.py"
PACKAGE = REPOSITORY_ROOT / "nsc-builder" / "nsc_builder"

OWNERS = {
    "utils": {
        "utc_now",
        "write_json",
        "write_json_atomic",
        "write_text_atomic",
        "sha256_file",
        "current_revision",
        "unique_json_object",
    },
    "configuration": {
        "parse_builder_config_payload",
        "discover_archive_executable",
        "BuilderConfig",
    },
    "catalog": {
        "read_csv",
        "read_catalog_csv",
        "parse_wave_number",
        "validate_allocation_domains",
        "read_jsonl",
        "validate_executable_manifest",
        "split_semicolon",
        "CanonicalCatalog",
        "expand_variants",
    },
    "history": {
        "variant_history_key",
        "timeout_limit_for_average",
        "load_variant_timeout_budgets",
        "EvidenceStore",
        "history_record",
        "load_history_execution",
    },
    "reporting": {
        "case_diagnostic_summary",
        "case_contract_payload",
        "case_evidence_output",
        "retry_variants",
        "active_retry_variants",
        "row_progress_text",
        "row_action_text",
        "status_filter_matches",
        "condensed_error_report",
    },
    "process_control": {"ProcessController"},
    "build": {
        "effective_build_dir",
        "_has_reparse_attribute",
        "_ensure_plain_path",
        "_paths_overlap",
        "test_tree_cleanup_targets",
        "clean_test_build_trees",
        "compiler_build_dir",
        "compiler_output",
        "compiler_runtime_library",
        "component_test_executable",
        "host_executable",
        "destructive_host_executable",
        "configure_commands",
        "build_commands",
        "compiler_configure_commands",
        "compiler_build_commands",
    },
    "execution": {
        "infrastructure_command_outcome",
        "infrastructure_failure_detail",
        "cancelled_infrastructure_outcomes",
        "blocked_infrastructure_outcomes",
        "expected_abnormal_termination_detail",
        "parse_host_envelope",
        "parse_identity_envelope",
        "parse_python_gate_result",
        "parse_selfhost_oracle",
        "equal_json_exact",
        "parse_selfhost_json_output",
        "parse_selfhost_inputs",
        "ExecutionEngine",
    },
    "runtime_support": {
        "make_run_id",
        "compiler_receipt",
        "file_receipt",
        "catalog_audit",
    },
    "gui": {"launch_gui"},
    "cli": {"main"},
}

PREAMBLES = {
    "utils": "from .constants import *\nfrom .paths import *\n",
    "configuration": (
        "from .constants import *\nfrom .paths import *\nfrom .utils import *\n"
        "from .toolchain import *\n"
    ),
    "catalog": (
        "from .constants import *\nfrom .paths import *\nfrom .models import *\n"
        "from .utils import *\n"
    ),
    "history": (
        "from .constants import *\nfrom .paths import *\nfrom .models import *\n"
        "from .utils import *\nfrom .catalog import *\n"
    ),
    "reporting": (
        "from .constants import *\nfrom .paths import *\nfrom .models import *\n"
    ),
    "process_control": "from .constants import *\n",
    "build": (
        "from .constants import *\nfrom .paths import *\nfrom .models import *\n"
        "from .utils import *\nfrom .configuration import *\nfrom .toolchain import *\n"
    ),
    "execution": (
        "from .constants import *\nfrom .paths import *\nfrom .models import *\n"
        "from .utils import *\nfrom .configuration import *\nfrom .catalog import *\n"
        "from .history import *\nfrom .reporting import *\nfrom .process_control import *\n"
        "from .build import *\n"
    ),
    "runtime_support": (
        "from .constants import *\nfrom .paths import *\nfrom .utils import *\n"
        "from .configuration import *\nfrom .catalog import *\n"
    ),
    "gui": (
        "from .constants import *\nfrom .paths import *\nfrom .models import *\n"
        "from .utils import *\nfrom .configuration import *\nfrom .catalog import *\n"
        "from .history import *\nfrom .reporting import *\nfrom .process_control import *\n"
        "from .build import *\nfrom .execution import *\nfrom .runtime_support import *\n"
    ),
    "cli": "from .runtime_support import catalog_audit\nfrom .gui import launch_gui\n",
}

MODULE_DOCS = {
    "cli": """Command-line and graphical entrypoint for NSC Builder 3.

The application audits the canonical catalogue on request and otherwise opens
the complete Builder 3 graphical interface.
""",
}

STANDARD_IMPORTS = """from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
import math
import os
import queue
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
import uuid
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable, Sequence

"""


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    tree = ast.parse(source)
    lines = source.splitlines(keepends=True)

    def definition_source(node: ast.AST) -> str:
        decorators = getattr(node, "decorator_list", ())
        start = min(
            (decorator.lineno for decorator in decorators),
            default=node.lineno,
        )
        return "".join(lines[start - 1 : node.end_lineno]).rstrip()

    definitions = {
        node.name: definition_source(node)
        for node in tree.body
        if isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef))
    }
    assigned = set().union(*OWNERS.values())
    missing = assigned - set(definitions)
    if missing:
        raise RuntimeError(f"source definitions are missing: {sorted(missing)}")
    for module_name, names in OWNERS.items():
        ordered = [
            node.name
            for node in tree.body
            if isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef))
            and node.name in names
        ]
        body = "\n\n\n".join(definitions[name] for name in ordered)
        module_doc = MODULE_DOCS.get(
            module_name,
            f"Extracted {module_name} implementation for modular NSC Builder 3.",
        )
        text = (
            f'"""{module_doc}"""\n\n'
            + STANDARD_IMPORTS
            + PREAMBLES[module_name]
            + "\n\n"
            + body
            + "\n"
        )
        (PACKAGE / f"{module_name}.py").write_text(
            text, encoding="utf-8", newline="\n"
        )


if __name__ == "__main__":
    main()
