from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import ModuleType
from unittest import mock


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
REPOSITORY_ROOT = CONTROLLER_ROOT.parents[2]
SOURCE_ROOT = REPOSITORY_ROOT / "tests" / "compiler-suite"
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

import run_selfhost_bootstrap_gate as bootstrap_gate
import run_selfhost_codegen_gate as codegen_gate
import run_selfhost_documentation_gate as documentation_gate
from component_gate_support import component_manifest_rows
from selfhost_inventory import (
    classification_sets,
    load_component_inventory,
    read_jsonl,
    validate_manifest_cases,
    validate_executable_manifest,
    validate_unique_materialized_ids,
)


class SelfHostInventoryBoundaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.diagnostics = load_component_inventory(
            REPOSITORY_ROOT, "SelfHost.Diagnostics"
        )
        manifest = read_jsonl(SOURCE_ROOT / "manifest" / "cases.jsonl")
        cls.manifest_row = next(
            row
            for row in manifest
            if row.get("component") == "SelfHost.Diagnostics"
        )
        cls.types = load_component_inventory(REPOSITORY_ROOT, "SelfHost.Types")
        design = next(iter(cls.types.design_by_id.values()))
        cls.classification_row: dict[str, object] = {
            "case_id": design["case_id"],
            "contract_id": design["contract_id"],
            "component": "SelfHost.Types",
            "disposition": "KnownProductDefect",
        }

    def classification_from(self, row: dict[str, object]) -> dict[str, frozenset[str]]:
        with tempfile.TemporaryDirectory(
            prefix="selfhost-classification-", dir=Path.cwd()
        ) as temporary:
            path = Path(temporary) / "classification.jsonl"
            path.write_text(json.dumps(row) + "\n", encoding="utf-8", newline="\n")
            return classification_sets(self.types, path)

    def run_gate(self, module: ModuleType) -> int:
        with mock.patch.object(
            sys,
            "argv",
            [module.__file__ or module.__name__, "--repository-root", str(REPOSITORY_ROOT)],
        ):
            return module.main()

    def test_manifest_identity_fields_reject_string_coercions(self) -> None:
        for field in ("case_id", "contract_id", "layer", "profile"):
            for invalid in (7, True, ""):
                with self.subTest(field=field, invalid=invalid):
                    row = dict(self.manifest_row)
                    row[field] = invalid
                    with self.assertRaisesRegex(
                        RuntimeError, rf"{field} must be a non-empty string"
                    ):
                        validate_manifest_cases(self.diagnostics, [row])

    def test_classification_identity_fields_reject_string_coercions(self) -> None:
        for field in ("case_id", "contract_id", "disposition"):
            for invalid in (7, True, ""):
                with self.subTest(field=field, invalid=invalid):
                    row = dict(self.classification_row)
                    row[field] = invalid
                    with self.assertRaisesRegex(
                        RuntimeError, rf"{field} must be a non-empty string"
                    ):
                        self.classification_from(row)

    def test_optional_classification_component_is_an_exact_string(self) -> None:
        for invalid in (7, True, ""):
            with self.subTest(invalid=invalid):
                row = dict(self.classification_row)
                row["component"] = invalid
                with self.assertRaisesRegex(
                    RuntimeError, r"component must be a non-empty string"
                ):
                    self.classification_from(row)

    def test_live_components_match_their_executable_manifests(self) -> None:
        scenarios = (
            (
                "Host.Source",
                SOURCE_ROOT
                / "host"
                / "modules"
                / "HostSourceCaseClassifications.jsonl",
            ),
            (
                "SelfHost.Source",
                SOURCE_ROOT
                / "selfhost"
                / "modules"
                / "SelfHostSourceCaseClassifications.jsonl",
            ),
            (
                "SelfHost.Types",
                SOURCE_ROOT
                / "selfhost"
                / "modules"
                / "SelfHostTypesCaseClassifications.jsonl",
            ),
        )
        for component, path in scenarios:
            with self.subTest(component=component):
                inventory = load_component_inventory(REPOSITORY_ROOT, component)
                cases = component_manifest_rows(
                    SOURCE_ROOT, REPOSITORY_ROOT, component
                )
                validate_executable_manifest(inventory, cases, path)

        basic = load_component_inventory(REPOSITORY_ROOT, "SelfHost.Basic")
        basic_cases = component_manifest_rows(
            SOURCE_ROOT, REPOSITORY_ROOT, "SelfHost.Basic"
        )
        validate_executable_manifest(basic, basic_cases)

    def test_materialized_case_identity_rejects_string_coercions(self) -> None:
        for invalid in (7, True, "", None):
            with self.subTest(invalid=invalid):
                with self.assertRaisesRegex(RuntimeError, "invalid case identity"):
                    validate_unique_materialized_ids(
                        "SelfHost.Synthetic",
                        [(invalid, "CSH-SYN-001")],
                    )

    def test_live_materialized_module_identities_satisfy_the_boundary(self) -> None:
        for module in (bootstrap_gate, codegen_gate, documentation_gate):
            with self.subTest(module=module.__name__):
                self.assertEqual(self.run_gate(module), 0)


if __name__ == "__main__":
    unittest.main()
