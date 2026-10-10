from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path
from types import ModuleType
from unittest import mock


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
REPOSITORY_ROOT = CONTROLLER_ROOT.parents[2]
SELFHOST_ROOT = REPOSITORY_ROOT / "tests" / "compiler-suite" / "selfhost"
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

import run_selfhost_bootstrap_infrastructure_gate as bootstrap_gate
import run_selfhost_contracts_gate as contracts_gate
import run_selfhost_documentation_gate as documentation_gate
import run_selfhost_project_gate as project_gate
from selfhost_inventory import read_jsonl


class SelfHostRemainingCatalogBoundaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        module_root = SELFHOST_ROOT / "modules"
        cls.project_contradictions = read_jsonl(
            module_root / "SelfHostProjectPlanContradictions.jsonl"
        )
        cls.bootstrap_contradictions = read_jsonl(
            module_root / "SelfHostBootstrapInfrastructurePlanContradictions.jsonl"
        )
        cls.contract_contradictions = read_jsonl(
            module_root / "SelfHostContractsPlanContradictions.jsonl"
        )
        cls.documentation_contradictions = read_jsonl(
            module_root / "SelfHostDocumentationPlanContradictions.jsonl"
        )
        cls.contract_catalog = read_jsonl(
            SELFHOST_ROOT / "contracts" / "SelfHostContractsCatalog.jsonl"
        )

    def run_gate(self, module: ModuleType) -> int:
        with mock.patch.object(
            sys,
            "argv",
            [module.__file__ or module.__name__, "--repository-root", str(REPOSITORY_ROOT)],
        ):
            return module.main()

    def test_live_selfhost_catalogs_satisfy_exact_boundaries(self) -> None:
        for module in (
            project_gate,
            bootstrap_gate,
            contracts_gate,
            documentation_gate,
        ):
            with self.subTest(module=module.__name__):
                self.assertEqual(self.run_gate(module), 0)

    def test_project_contradiction_rejects_id_coercions(self) -> None:
        for invalid in (7, True, ""):
            with self.subTest(invalid=invalid):
                records = copy.deepcopy(self.project_contradictions)
                records[0]["id"] = invalid
                with mock.patch.object(project_gate, "read_jsonl", return_value=records):
                    with self.assertRaisesRegex(RuntimeError, "id must be a non-empty string"):
                        self.run_gate(project_gate)

    def test_bootstrap_contradiction_rejects_id_coercions(self) -> None:
        for invalid in (7, True, ""):
            with self.subTest(invalid=invalid):
                records = copy.deepcopy(self.bootstrap_contradictions)
                records[0]["id"] = invalid
                with mock.patch.object(bootstrap_gate, "read_jsonl", return_value=records):
                    with self.assertRaisesRegex(RuntimeError, "id must be a non-empty string"):
                        self.run_gate(bootstrap_gate)

    def test_contract_catalog_rejects_identity_and_array_coercions(self) -> None:
        scenarios = (
            ("contract_id", 7, "contract_id must be a non-empty string"),
            ("source_path", 7, "source_path must be a non-empty string"),
            ("case_ids", "T12B-01519", "case_ids must be an array"),
            ("case_ids", [1519], "case_ids must contain non-empty strings"),
            (
                "case_ids",
                ["T12B-01519", "T12B-01519"],
                "case_ids contains duplicates",
            ),
            ("case_ids", [], "invalid case IDs"),
        )
        for field, invalid, message in scenarios:
            with self.subTest(field=field, invalid=invalid):
                catalog = copy.deepcopy(self.contract_catalog)
                contradictions = copy.deepcopy(self.contract_contradictions)
                catalog[0][field] = invalid
                with mock.patch.object(
                    contracts_gate,
                    "read_jsonl",
                    side_effect=(catalog, contradictions),
                ):
                    with self.assertRaisesRegex(RuntimeError, message):
                        self.run_gate(contracts_gate)

    def test_contract_contradiction_rejects_id_coercions(self) -> None:
        for invalid in (7, True, ""):
            with self.subTest(invalid=invalid):
                catalog = copy.deepcopy(self.contract_catalog)
                contradictions = copy.deepcopy(self.contract_contradictions)
                contradictions[0]["id"] = invalid
                with mock.patch.object(
                    contracts_gate,
                    "read_jsonl",
                    side_effect=(catalog, contradictions),
                ):
                    with self.assertRaisesRegex(RuntimeError, "id must be a non-empty string"):
                        self.run_gate(contracts_gate)

    def test_contract_catalog_rejects_producers_outside_repository(self) -> None:
        external = REPOSITORY_ROOT.parent / "LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md"
        scenarios = (
            (str(external.resolve()), "must be repository-relative"),
            ("../LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md", "outside the repository"),
        )
        for source_path, message in scenarios:
            with self.subTest(source_path=source_path):
                catalog = copy.deepcopy(self.contract_catalog)
                contradictions = copy.deepcopy(self.contract_contradictions)
                catalog[0]["source_path"] = source_path
                with mock.patch.object(
                    contracts_gate,
                    "read_jsonl",
                    side_effect=(catalog, contradictions),
                ):
                    with self.assertRaisesRegex(RuntimeError, message):
                        self.run_gate(contracts_gate)

    def test_documentation_conflict_rejects_case_array_coercions(self) -> None:
        first_case = self.documentation_contradictions[0]["affected_case_ids"][0]
        scenarios = (
            ("TSH-00216", "affected_case_ids must be an array"),
            ([216], "affected_case_ids must contain non-empty strings"),
            ([first_case, first_case], "affected_case_ids contains duplicates"),
            ([], "repeat or omit"),
        )
        for invalid, message in scenarios:
            with self.subTest(invalid=invalid):
                conflicts = copy.deepcopy(self.documentation_contradictions)
                conflicts[0]["affected_case_ids"] = invalid
                with mock.patch.object(
                    documentation_gate,
                    "read_jsonl",
                    return_value=conflicts,
                ):
                    with self.assertRaisesRegex(RuntimeError, message):
                        self.run_gate(documentation_gate)


if __name__ == "__main__":
    unittest.main()
