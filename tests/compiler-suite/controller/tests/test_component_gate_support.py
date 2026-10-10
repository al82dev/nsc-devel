from __future__ import annotations

import sys
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

from component_gate_support import assert_component_results
from nsc_suite.manifest import CaseManifest


def case(case_id: str, expected_outcome: str = "PASS") -> CaseManifest:
    return CaseManifest(
        case_id=case_id,
        contract_id="CB-BAS-001",
        component="Host.Basic",
        case_class="nominal",
        executor="nsc_component_tests",
        layer="unit",
        inputs={},
        oracle={},
        dependencies=(),
        profile="PR",
        build_modes=("Debug", "Release"),
        optimizations=("NA",),
        isolation="in-process-reset",
        timeout_class="tiny",
        tags=("foundation",),
        expected_outcome=expected_outcome,
    )


class ComponentGateSupportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.cases = (
            case("TB-BAS-001"),
            case("TB-BAS-002", "KNOWN_FAILURE"),
        )

    def test_accepts_one_terminal_result_per_selected_case(self) -> None:
        assert_component_results(
            {
                "results": [
                    {"case_id": "TB-BAS-001"},
                    {"case_id": "TB-BAS-002"},
                ],
                "counts": {
                    "PASS": 1,
                    "KNOWN_FAILURE": 1,
                    "FAIL": 0,
                },
            },
            self.cases,
            "Host.Basic",
        )

    def test_rejects_missing_or_reordered_results(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "live manifest selection"):
            assert_component_results(
                {
                    "results": [
                        {"case_id": "TB-BAS-002"},
                        {"case_id": "TB-BAS-001"},
                    ],
                    "counts": {"PASS": 1, "KNOWN_FAILURE": 1},
                },
                self.cases,
                "Host.Basic",
            )

    def test_rejects_counts_that_differ_from_declared_outcomes(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "declared outcomes"):
            assert_component_results(
                {
                    "results": [
                        {"case_id": "TB-BAS-001"},
                        {"case_id": "TB-BAS-002"},
                    ],
                    "counts": {"PASS": 2, "KNOWN_FAILURE": 0},
                },
                self.cases,
                "Host.Basic",
            )

    def test_rejects_invalid_controller_result_schema(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "schema is invalid"):
            assert_component_results(
                {"results": {}, "counts": []},
                self.cases,
                "Host.Basic",
            )


if __name__ == "__main__":
    unittest.main()
