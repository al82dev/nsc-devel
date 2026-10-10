from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3SelfHostOracleTests(unittest.TestCase):
    def test_live_selfhost_oracles_follow_the_closed_schema(self) -> None:
        cases = tuple(
            case
            for case in builder.CanonicalCatalog.load().cases
            if case.executable and case.component.startswith("SelfHost.")
        )
        self.assertTrue(cases)
        for case in cases:
            with self.subTest(case_id=case.case_id):
                exit_code, expected_json = builder.parse_selfhost_oracle(
                    (case.manifest or {}).get("oracle")
                )
                self.assertIs(type(exit_code), int)
                if expected_json is not None:
                    self.assertIsInstance(expected_json, dict)

    def test_exit_code_is_required_and_never_coerced(self) -> None:
        for oracle in ({}, {"exit_code": True}, {"exit_code": "0"}):
            with self.subTest(oracle=oracle):
                with self.assertRaisesRegex(ValueError, "exit_code"):
                    builder.parse_selfhost_oracle(oracle)

    def test_optional_description_must_remain_text(self) -> None:
        self.assertEqual(
            builder.parse_selfhost_oracle(
                {"exit_code": 0, "description": "expected success"}
            ),
            (0, None),
        )
        with self.assertRaisesRegex(ValueError, "description"):
            builder.parse_selfhost_oracle({"exit_code": 0, "description": 3})

    def test_optional_json_expectation_must_be_an_object(self) -> None:
        expected = {"status": "ok"}
        self.assertEqual(
            builder.parse_selfhost_oracle({"exit_code": 0, "json": expected}),
            (0, expected),
        )
        for value in (None, [], "{}", 1):
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, "json"):
                    builder.parse_selfhost_oracle({"exit_code": 0, "json": value})

    def test_unknown_oracle_fields_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown fields"):
            builder.parse_selfhost_oracle({"exit_code": 0, "fallback": True})


if __name__ == "__main__":
    unittest.main()
