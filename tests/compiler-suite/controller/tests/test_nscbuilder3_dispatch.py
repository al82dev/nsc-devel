from __future__ import annotations

import sys
import unittest
from collections import Counter
from pathlib import Path
from unittest.mock import Mock


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3DispatchTests(unittest.TestCase):
    def setUp(self) -> None:
        self.engine = builder.ExecutionEngine(
            builder.BuilderConfig(),
            Mock(),
            lambda _line: None,
            lambda *_values: None,
            lambda _current, _total: None,
        )
        self.routes = {
            "host": Mock(return_value=("PASS", "", "")),
            "identity": Mock(return_value=("PASS", "", "")),
            "selfhost": Mock(return_value=("PASS", "", "")),
            "buildsystem": Mock(return_value=("PASS", "", "")),
            "packaging": Mock(return_value=("PASS", "", "")),
        }
        self.engine._run_host = self.routes["host"]
        self.engine._run_identity = self.routes["identity"]
        self.engine._run_selfhost = self.routes["selfhost"]
        self.engine._run_buildsystem = self.routes["buildsystem"]
        self.engine._run_packaging = self.routes["packaging"]

    @staticmethod
    def expected_route(case: builder.CaseRecord) -> str:
        if case.executor in {
            "nsc_component_tests",
            "nsc_component_tests_destructive",
            "nsc_component_tests_source_extended",
        }:
            return "host"
        if case.executor != "nsc_suite_controller+nsc":
            raise AssertionError(f"unsupported live executor: {case.executor}")
        if case.component.startswith("SelfHost."):
            return "selfhost"
        if case.component == "BuildSystem":
            return "buildsystem"
        if case.component == "Packaging":
            return "packaging"
        return "identity"

    def test_every_live_case_reaches_exactly_one_supported_route(self) -> None:
        catalog = builder.CanonicalCatalog.load()
        expected_counts: Counter[str] = Counter()

        for case in catalog.cases:
            if not case.executable:
                continue
            route = self.expected_route(case)
            expected_counts[route] += 1
            before = {name: mock.call_count for name, mock in self.routes.items()}
            variant = builder.RunVariant(case, case.build_modes[0], case.optimizations[0])
            outcome = self.engine._run_variant(variant)
            self.assertEqual(outcome.status, "PASS", case.case_id)
            called = [
                name
                for name, mock in self.routes.items()
                if mock.call_count != before[name]
            ]
            self.assertEqual(called, [route], case.case_id)

        self.assertEqual(
            Counter({name: mock.call_count for name, mock in self.routes.items()}),
            expected_counts,
        )

    def test_unsupported_executor_fails_closed_without_calling_a_route(self) -> None:
        case = builder.CaseRecord(
            case_id="TI-DISPATCH-001",
            contract_id="CI-DISPATCH-001",
            component="Host.Basic",
            wave=1,
            purpose="synthetic dispatch guard",
            layer="historical",
            profile="WEEKLY",
            build_modes=("Debug",),
            optimizations=("NA",),
            executor="historical-weekly",
            disposition="Executable",
            objective="reject an executor not implemented by Builder 3",
            expected_outcome="PASS",
            manifest={},
        )

        outcome = self.engine._run_variant(builder.RunVariant(case, "Debug", "NA"))

        self.assertEqual(outcome.status, "INFRASTRUCTURE_ERROR")
        self.assertIn("unsupported executor", outcome.detail)
        self.assertTrue(all(mock.call_count == 0 for mock in self.routes.values()))


if __name__ == "__main__":
    unittest.main()
