from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

from component_gate_support import (
    assert_component_results,
    read_json_object,
    read_jsonl,
    required_manifest_integer,
    required_nonnegative_integer,
)
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

    def test_jsonl_reader_rejects_duplicate_nested_keys(self) -> None:
        with tempfile.TemporaryDirectory(prefix="component-jsonl-", dir=Path.cwd()) as root:
            path = Path(root) / "catalog.jsonl"
            path.write_text(
                '{"case_id":"TC-001","summary":{"required":1,"required":2}}\n',
                encoding="utf-8",
                newline="\n",
            )
            with self.assertRaisesRegex(
                RuntimeError, "duplicate JSON object key: required"
            ):
                read_jsonl(path)

    def test_json_document_reader_rejects_duplicate_nested_keys(self) -> None:
        with tempfile.TemporaryDirectory(prefix="component-json-", dir=Path.cwd()) as root:
            path = Path(root) / "results.json"
            path.write_text(
                '{"counts":{"PASS":1,"PASS":2},"results":[]}\n',
                encoding="utf-8",
                newline="\n",
            )
            with self.assertRaisesRegex(
                RuntimeError, "duplicate JSON object key: PASS"
            ):
                read_json_object(path)

    def test_accepts_one_terminal_result_per_selected_case(self) -> None:
        assert_component_results(
            {
                "results": [
                    {"case_id": "TB-BAS-001", "status": "PASS"},
                    {"case_id": "TB-BAS-002", "status": "KNOWN_FAILURE"},
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
                        {"case_id": "TB-BAS-002", "status": "KNOWN_FAILURE"},
                        {"case_id": "TB-BAS-001", "status": "PASS"},
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
                        {"case_id": "TB-BAS-001", "status": "PASS"},
                        {"case_id": "TB-BAS-002", "status": "KNOWN_FAILURE"},
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

    def test_rejects_coercible_result_identities_and_counts(self) -> None:
        numeric_case = (case("7"),)
        with self.assertRaisesRegex(RuntimeError, "invalid case identity"):
            assert_component_results(
                {"results": [{"case_id": 7}], "counts": {"PASS": 1}},
                numeric_case,
                "Synthetic",
            )
        for count in (True, "1", 1.0, -1):
            with self.subTest(count=count), self.assertRaisesRegex(
                RuntimeError, "invalid result count"
            ):
                assert_component_results(
                    {
                        "results": [{"case_id": "7", "status": "PASS"}],
                        "counts": {"PASS": count},
                    },
                    numeric_case,
                    "Synthetic",
                )

    def test_rejects_row_statuses_that_contradict_declared_counts(self) -> None:
        payload = {
            "results": [
                {"case_id": "TB-BAS-001", "status": "FAIL"},
                {"case_id": "TB-BAS-002", "status": "FAIL"},
            ],
            "counts": {"PASS": 1, "KNOWN_FAILURE": 1, "FAIL": 0},
        }
        with self.assertRaisesRegex(RuntimeError, "row statuses"):
            assert_component_results(payload, self.cases, "Host.Basic")

        payload["results"][0]["status"] = 1
        with self.assertRaisesRegex(RuntimeError, "invalid row status"):
            assert_component_results(payload, self.cases, "Host.Basic")

    def test_manifest_integer_requires_presence_and_exact_integer_type(self) -> None:
        self.assertEqual(
            required_manifest_integer({"variant": 3}, "variant", "Synthetic"),
            3,
        )
        for value in (True, "3", 3.0, None):
            with self.subTest(value=value), self.assertRaisesRegex(
                RuntimeError, "must be an exact integer"
            ):
                required_manifest_integer({"variant": value}, "variant", "Synthetic")
        with self.assertRaisesRegex(RuntimeError, "omit variant"):
            required_manifest_integer({}, "variant", "Synthetic")

    def test_nonnegative_integer_rejects_coercions_and_negative_values(self) -> None:
        self.assertEqual(
            required_nonnegative_integer({"required": 0}, "required", "Synthetic"),
            0,
        )
        for value in (True, "0", 0.0, -1, None):
            with self.subTest(value=value), self.assertRaisesRegex(
                RuntimeError, "exact nonnegative integer"
            ):
                required_nonnegative_integer(
                    {"required": value}, "required", "Synthetic"
                )
        with self.assertRaisesRegex(RuntimeError, "omits required"):
            required_nonnegative_integer({}, "required", "Synthetic")


if __name__ == "__main__":
    unittest.main()
