from __future__ import annotations

import sys
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite.oracle import compare_structured


class OracleTests(unittest.TestCase):
    def test_equal_structures_have_no_failures(self) -> None:
        value = {"tokens": [{"kind": "identifier", "offset": 4}], "valid": True}
        self.assertEqual(compare_structured(value, value), ())

    def test_reports_exact_nested_paths(self) -> None:
        failures = compare_structured(
            {"tokens": [{"kind": "identifier", "offset": 4}]},
            {"tokens": [{"kind": "integer", "offset": 5}]},
        )
        self.assertEqual(
            [(failure.path, failure.reason) for failure in failures],
            [
                ("$.tokens[0].kind", "value-mismatch"),
                ("$.tokens[0].offset", "value-mismatch"),
            ],
        )

    def test_distinguishes_bool_integer_and_float_types(self) -> None:
        self.assertEqual(compare_structured(True, 1)[0].reason, "type-mismatch")
        self.assertEqual(compare_structured(1, 1.0)[0].reason, "type-mismatch")

    def test_reports_missing_unexpected_and_length_mismatches(self) -> None:
        failures = compare_structured(
            {"required": [1, 2], "missing": 3},
            {"required": [1], "unexpected": 4},
        )
        self.assertEqual(
            [(failure.path, failure.reason) for failure in failures],
            [
                ("$.missing", "missing-key"),
                ("$.required", "length-mismatch"),
                ("$.unexpected", "unexpected-key"),
            ],
        )

    def test_deep_structure_uses_explicit_worklist(self) -> None:
        expected: list = []
        actual: list = []
        expected_cursor = expected
        actual_cursor = actual
        for _ in range(5000):
            expected_next: list = []
            actual_next: list = []
            expected_cursor.append(expected_next)
            actual_cursor.append(actual_next)
            expected_cursor = expected_next
            actual_cursor = actual_next
        expected_cursor.append("end")
        actual_cursor.append("changed")
        failures = compare_structured(expected, actual)
        self.assertEqual(len(failures), 1)
        self.assertEqual(failures[0].reason, "value-mismatch")


if __name__ == "__main__":
    unittest.main()
