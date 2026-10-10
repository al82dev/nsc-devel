from __future__ import annotations

import argparse
import sys
import unittest
from collections import Counter
from pathlib import Path
from unittest.mock import Mock, patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


IDENTITY_OPERATIONS = {
    "compiler-version-constants",
    "generated-identity-exports",
    "distinct-build-identities",
    "semantic-version",
}


class Builder3RoutePrerequisiteTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.catalog = builder.CanonicalCatalog.load()

    def test_every_selfhost_case_has_a_local_fixture_and_typed_contract(self) -> None:
        cases = tuple(
            case
            for case in self.catalog.cases
            if case.executable and case.component.startswith("SelfHost.")
        )
        self.assertTrue(cases)
        suite_root = builder.SUITE_ROOT.resolve()
        for case in cases:
            manifest = case.manifest or {}
            inputs = manifest.get("inputs")
            oracle = manifest.get("oracle")
            self.assertIsInstance(inputs, dict, case.case_id)
            self.assertIsInstance(oracle, dict, case.case_id)
            fixture = inputs.get("fixture_project")
            argument = inputs.get("case_argument")
            self.assertIsInstance(fixture, str, case.case_id)
            self.assertTrue(fixture, case.case_id)
            fixture_path = (suite_root / fixture).resolve()
            self.assertIn(suite_root, fixture_path.parents, case.case_id)
            self.assertTrue(fixture_path.is_file(), case.case_id)
            self.assertEqual(fixture_path.suffix, ".nsproj", case.case_id)
            self.assertIsInstance(argument, str, case.case_id)
            self.assertTrue(argument, case.case_id)
            sandbox = inputs.get("sandbox_argument")
            self.assertIn(sandbox, (None, "fresh-case-directory"), case.case_id)
            exit_code = oracle.get("exit_code")
            self.assertIsInstance(exit_code, int, case.case_id)
            self.assertNotIsInstance(exit_code, bool, case.case_id)

    def test_identity_rows_use_only_implemented_operations(self) -> None:
        cases = tuple(
            case
            for case in self.catalog.cases
            if case.executable
            and case.executor == "nsc_suite_controller+nsc"
            and not case.component.startswith("SelfHost.")
            and case.component not in {"BuildSystem", "Packaging"}
        )

        self.assertTrue(cases)
        self.assertEqual(
            {(case.manifest or {})["inputs"]["operation"] for case in cases},
            IDENTITY_OPERATIONS,
        )

    def test_buildsystem_rows_reach_exactly_one_gate_branch(self) -> None:
        engine = builder.ExecutionEngine(
            builder.BuilderConfig(), Mock(), lambda _line: None, lambda *_args: None, lambda *_args: None
        )
        module = engine._load_gate("run_buildsystem_gate.py")
        routes = {
            1: "run_identity",
            2: "run_llvm_dependencies",
            3: "run_profiles",
            4: "run_runtime",
            5: "run_installation",
            76: "run_root_graph",
        }
        mocks = {name: Mock(return_value=(True, "")) for name in routes.values()}
        cases = tuple(
            case
            for case in self.catalog.cases
            if case.executable and case.component == "BuildSystem"
        )
        expected: Counter[str] = Counter()

        with patch.multiple(module, **mocks), patch.object(
            module, "validate_root_config_arguments", return_value=None
        ):
            for case in cases:
                inputs = (case.manifest or {}).get("inputs", {})
                ordinal = inputs.get("contract_ordinal")
                variant = inputs.get("variant")
                self.assertIn(ordinal, routes, case.case_id)
                self.assertIsInstance(variant, int, case.case_id)
                expected[routes[ordinal]] += 1
                satisfied, detail = module.run_case(
                    case.manifest,
                    argparse.Namespace(
                        source_root=builder.SUITE_ROOT,
                        cmake_executable=Path("C:/synthetic/cmake.exe"),
                        repository_root=builder.SOURCE_ROOT,
                        installation_root=Path("A:/synthetic-installation"),
                        root_config_argument=(),
                    ),
                    Path("A:/synthetic-buildsystem-case"),
                    {},
                )
                self.assertTrue(satisfied, (case.case_id, detail))

        self.assertEqual(
            Counter({name: mock.call_count for name, mock in mocks.items()}), expected
        )

    def test_packaging_rows_reach_exactly_one_gate_branch(self) -> None:
        engine = builder.ExecutionEngine(
            builder.BuilderConfig(), Mock(), lambda _line: None, lambda *_args: None, lambda *_args: None
        )
        module = engine._load_gate("run_packaging_gate.py")
        routes = {1: "run_deploy_batch", 3: "run_package_native_sharp"}
        mocks = {name: Mock(return_value=(True, "")) for name in routes.values()}
        cases = tuple(
            case
            for case in self.catalog.cases
            if case.executable and case.component == "Packaging"
        )
        expected: Counter[str] = Counter()

        with patch.multiple(module, **mocks):
            for case in cases:
                inputs = (case.manifest or {}).get("inputs", {})
                ordinal = inputs.get("contract_ordinal")
                variant = inputs.get("variant")
                self.assertIn(ordinal, routes, case.case_id)
                self.assertIsInstance(variant, int, case.case_id)
                expected[routes[ordinal]] += 1
                satisfied, detail = module.run_case(
                    case.manifest,
                    argparse.Namespace(repository_root=builder.SOURCE_ROOT),
                    Path("A:/synthetic-packaging-case"),
                    {},
                )
                self.assertTrue(satisfied, (case.case_id, detail))

        self.assertEqual(
            Counter({name: mock.call_count for name, mock in mocks.items()}), expected
        )

    def test_python_gates_reject_coercible_route_integers(self) -> None:
        engine = builder.ExecutionEngine(
            builder.BuilderConfig(), Mock(), lambda _line: None, lambda *_args: None, lambda *_args: None
        )
        scenarios = (
            (
                "run_buildsystem_gate.py",
                argparse.Namespace(),
                {"contract_ordinal": True, "variant": 1},
                "BuildSystem manifest input contract_ordinal must be an exact integer",
            ),
            (
                "run_packaging_gate.py",
                argparse.Namespace(),
                {"contract_ordinal": 1, "variant": "1"},
                "Packaging manifest input variant must be an exact integer",
            ),
        )
        for module_name, options, inputs, message in scenarios:
            with self.subTest(module=module_name):
                module = engine._load_gate(module_name)
                with self.assertRaisesRegex(RuntimeError, message):
                    module.run_case(
                        {"inputs": inputs},
                        options,
                        Path("A:/synthetic-case"),
                        {},
                    )


if __name__ == "__main__":
    unittest.main()
