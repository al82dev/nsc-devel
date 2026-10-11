from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite.manifest import (
    CASE_CLASSES,
    TIMEOUT_CLASSES,
    InventoryIndex,
    ManifestError,
    load_manifest,
)


def valid_case(case_id: str = "TB-BAS-001", dependencies: list[str] | None = None) -> dict:
    return {
        "case_id": case_id,
        "contract_id": "CB-BAS-001",
        "component": "Host.Basic",
        "class": "nominal",
        "executor": "nsc_component_tests",
        "layer": "unit",
        "inputs": {"value": 1},
        "oracle": {"valid": True, "value": 1},
        "dependencies": dependencies or [],
        "profile": "PR",
        "build_modes": ["Debug", "Release"],
        "optimizations": ["NA"],
        "isolation": "in-process-reset",
        "timeout_class": "tiny",
        "tags": ["foundation"],
    }


class ManifestTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(
            prefix="nsc-suite-manifest-", dir=Path.cwd()
        )
        self.root = Path(self.temporary.name)
        self.components = self.root / "components.csv"
        self.contracts = self.root / "contracts.csv"
        self.components.write_text("component\nHost.Basic\n", encoding="utf-8")
        self.contracts.write_text(
            "contract_id,component\nCB-BAS-001,Host.Basic\n", encoding="utf-8"
        )
        self.inventory = InventoryIndex.from_csv(self.components, self.contracts)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_manifest(self, cases: list[dict]) -> Path:
        path = self.root / "cases.jsonl"
        path.write_text(
            "".join(json.dumps(case, sort_keys=True) + "\n" for case in cases),
            encoding="utf-8",
        )
        return path

    def assert_manifest_error(self, cases: list[dict], text: str) -> None:
        with self.assertRaisesRegex(ManifestError, text):
            load_manifest(self.write_manifest(cases), self.inventory)

    def test_loads_and_orders_valid_cases(self) -> None:
        second = valid_case("TB-BAS-002", ["TB-BAS-001"])
        loaded = load_manifest(self.write_manifest([second, valid_case()]), self.inventory)
        self.assertEqual([case.case_id for case in loaded], ["TB-BAS-001", "TB-BAS-002"])
        self.assertEqual(loaded[1].dependencies, ("TB-BAS-001",))
        self.assertEqual(loaded[0].expected_outcome, "PASS")

    def test_accepts_only_explicit_known_failure_outcomes(self) -> None:
        known = valid_case()
        known["expected_outcome"] = "KNOWN_FAILURE"
        loaded = load_manifest(self.write_manifest([known]), self.inventory)
        self.assertEqual(loaded[0].expected_outcome, "KNOWN_FAILURE")
        invalid = valid_case()
        invalid["expected_outcome"] = "SKIP"
        self.assert_manifest_error([invalid], "invalid expected_outcome")

    def test_accepts_the_isolated_source_executor_as_a_host_boundary(self) -> None:
        extended = valid_case()
        extended["executor"] = "nsc_component_tests_source_extended"
        loaded = load_manifest(self.write_manifest([extended]), self.inventory)
        self.assertEqual(loaded[0].executor, "nsc_component_tests_source_extended")

    def test_abnormal_termination_requires_an_exact_known_defect_contract(self) -> None:
        known = valid_case()
        known["expected_outcome"] = "KNOWN_FAILURE"
        known["tags"] = ["known-defect", "unsafe-boundary"]
        known["oracle"] = {
            "abnormal_termination": {
                "exit_code": "nonzero",
                "output_contains": ["Assertion failed: array subscript out of range"],
            }
        }
        loaded = load_manifest(self.write_manifest([known]), self.inventory)
        self.assertEqual(
            loaded[0].oracle["abnormal_termination"]["exit_code"], "nonzero"
        )

        for mutation, message in (
            ({"expected_outcome": "PASS"}, "requires KNOWN_FAILURE"),
            ({"tags": ["known-defect"]}, "requires known-defect and unsafe-boundary"),
            (
                {
                    "oracle": {
                        "abnormal_termination": {
                            "exit_code": 3,
                            "output_contains": ["assertion"],
                        }
                    }
                },
                "exit_code must be nonzero",
            ),
        ):
            with self.subTest(mutation=mutation):
                invalid = dict(known)
                invalid.update(mutation)
                self.assert_manifest_error([invalid], message)

    def test_contractual_timeout_requires_an_exact_nightly_host_boundary(self) -> None:
        valid = valid_case("TB-BAS-009")
        valid["class"] = "boundary"
        valid["profile"] = "NIGHTLY"
        valid["timeout_class"] = "extended"
        valid["oracle"]["timeout_is_contract_failure"] = True
        loaded = load_manifest(self.write_manifest([valid]), self.inventory)
        self.assertIs(loaded[0].oracle["timeout_is_contract_failure"], True)

        for value in (False, 1):
            with self.subTest(value=value):
                invalid = json.loads(json.dumps(valid))
                invalid["oracle"]["timeout_is_contract_failure"] = value
                self.assert_manifest_error([invalid], "must be true")

        for field, value in (
            ("profile", "PR"),
            ("class", "nominal"),
            ("timeout_class", "medium"),
        ):
            with self.subTest(field=field):
                invalid = json.loads(json.dumps(valid))
                invalid[field] = value
                self.assert_manifest_error(
                    [invalid], "requires a NIGHTLY host boundary"
                )

    def test_rejects_unknown_and_missing_fields(self) -> None:
        unknown = valid_case()
        unknown["surprise"] = True
        self.assert_manifest_error([unknown], "unknown fields")
        missing = valid_case()
        del missing["oracle"]
        self.assert_manifest_error([missing], "missing fields")

    def test_rejects_duplicate_case_ids(self) -> None:
        self.assert_manifest_error([valid_case(), valid_case()], "duplicate case id")

    def test_inventory_rejects_ambiguous_csv_structure(self) -> None:
        self.components.write_text(
            "component,component\nHost.Basic,Host.Source\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ManifestError, "duplicate names"):
            InventoryIndex.from_csv(self.components, self.contracts)

        self.components.write_text("owner\nHost.Basic\n", encoding="utf-8")
        with self.assertRaisesRegex(ManifestError, "missing required fields"):
            InventoryIndex.from_csv(self.components, self.contracts)

        self.components.write_text("component\nHost.Basic\n", encoding="utf-8")
        self.contracts.write_text(
            "contract_id,component\nCB-BAS-001,Host.Basic,extra\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ManifestError, "extra cells"):
            InventoryIndex.from_csv(self.components, self.contracts)

    def test_rejects_duplicate_json_keys_at_any_depth(self) -> None:
        path = self.root / "duplicate-keys.jsonl"
        serialized = json.dumps(valid_case(), sort_keys=True)
        path.write_text(
            serialized.replace(
                '"inputs": {"value": 1}',
                '"inputs": {"value": 1, "value": 2}',
            )
            + "\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ManifestError, "duplicate JSON object key: value"):
            load_manifest(path, self.inventory)

    def test_rejects_unknown_contract_and_owner_mismatch(self) -> None:
        unknown = valid_case()
        unknown["contract_id"] = "CB-BAS-999"
        self.assert_manifest_error([unknown], "unknown contract")
        self.components.write_text("component\nHost.Basic\nHost.Source\n", encoding="utf-8")
        mismatched = valid_case()
        mismatched["component"] = "Host.Source"
        inventory = InventoryIndex.from_csv(self.components, self.contracts)
        with self.assertRaisesRegex(ManifestError, "contract owner mismatch"):
            load_manifest(self.write_manifest([mismatched]), inventory)

    def test_rejects_executor_specific_invalid_combinations(self) -> None:
        wrong_isolation = valid_case()
        wrong_isolation.update(
            {
                "executor": "nsc_suite_controller+nsc",
                "layer": "build-contract",
                "isolation": "in-process-reset",
            }
        )
        self.assert_manifest_error([wrong_isolation], "requires temporary-directory")
        wrong_optimization = valid_case()
        wrong_optimization["optimizations"] = ["O0"]
        self.assert_manifest_error([wrong_optimization], "requires optimizations")

    def test_accepts_host_execution_layers(self) -> None:
        for layer in ("unit", "component", "robustness", "integration"):
            with self.subTest(layer=layer):
                case = valid_case()
                case["layer"] = layer
                loaded = load_manifest(self.write_manifest([case]), self.inventory)
                self.assertEqual(loaded[0].layer, layer)

    def test_accepts_host_isolation_contracts(self) -> None:
        for isolation in (
            "in-process-reset",
            "temporary-directory",
            "filesystem-temp",
        ):
            with self.subTest(isolation=isolation):
                case = valid_case()
                case["isolation"] = isolation
                loaded = load_manifest(self.write_manifest([case]), self.inventory)
                self.assertEqual(loaded[0].isolation, isolation)

    def test_accepts_every_executor_timeout_class(self) -> None:
        for timeout_class in sorted(TIMEOUT_CLASSES):
            with self.subTest(timeout_class=timeout_class):
                case = valid_case()
                case["timeout_class"] = timeout_class
                loaded = load_manifest(self.write_manifest([case]), self.inventory)
                self.assertEqual(loaded[0].timeout_class, timeout_class)

        invalid = valid_case()
        invalid["timeout_class"] = "revision-specific"
        self.assert_manifest_error([invalid], "invalid timeout_class")

    def test_accepts_every_canonical_case_class(self) -> None:
        for case_class in sorted(CASE_CLASSES):
            with self.subTest(case_class=case_class):
                case = valid_case()
                case["class"] = case_class
                loaded = load_manifest(self.write_manifest([case]), self.inventory)
                self.assertEqual(loaded[0].case_class, case_class)

        invalid = valid_case()
        invalid["class"] = "revision-specific"
        self.assert_manifest_error([invalid], "invalid class")

    def test_accepts_non_compilation_nsc_build_contract(self) -> None:
        build_contract = valid_case()
        build_contract.update(
            {
                "executor": "nsc_suite_controller+nsc",
                "layer": "build-contract",
                "isolation": "temporary-directory",
                "optimizations": ["NA"],
            }
        )
        loaded = load_manifest(self.write_manifest([build_contract]), self.inventory)
        self.assertEqual(loaded[0].layer, "build-contract")
        self.assertEqual(loaded[0].optimizations, ("NA",))

    def test_rejects_unknown_self_and_cyclic_dependencies(self) -> None:
        self.assert_manifest_error(
            [valid_case(dependencies=["TB-BAS-999"])], "unknown dependencies"
        )
        self.assert_manifest_error(
            [valid_case(dependencies=["TB-BAS-001"])], "depends on itself"
        )
        first = valid_case("TB-BAS-001", ["TB-BAS-002"])
        second = valid_case("TB-BAS-002", ["TB-BAS-001"])
        self.assert_manifest_error([first, second], "dependency cycle")

    def test_preserves_external_producer_dependencies_outside_case_graph(self) -> None:
        case = valid_case(dependencies=["LLVM 23", "NSLink", "SelfHost.Basic"])
        loaded = load_manifest(self.write_manifest([case]), self.inventory)
        self.assertEqual(
            loaded[0].dependencies,
            ("LLVM 23", "NSLink", "SelfHost.Basic"),
        )

    def test_empty_manifest_is_explicitly_configurable(self) -> None:
        path = self.write_manifest([])
        with self.assertRaisesRegex(ManifestError, "contains no cases"):
            load_manifest(path, self.inventory)
        self.assertEqual(load_manifest(path, self.inventory, require_cases=False), ())


if __name__ == "__main__":
    unittest.main()
