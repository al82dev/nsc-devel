from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

import run_selfhost_diagnostics_gate as diagnostics_gate
import run_selfhost_source_gate as source_gate
import run_selfhost_types_gate as types_gate
from selfhost_inventory import json_values_equal_exact, read_jsonl, required_oracle_exit_code


def completed(returncode: int, stdout: str = "") -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess(["synthetic"], returncode, stdout, "")


class SelfHostOracleContractTests(unittest.TestCase):
    def test_selfhost_jsonl_rejects_duplicate_nested_keys(self) -> None:
        with tempfile.TemporaryDirectory(prefix="selfhost-jsonl-", dir=Path.cwd()) as root:
            path = Path(root) / "cases.jsonl"
            path.write_text(
                '{"case_id":"TS-001","oracle":{"exit_code":0,"exit_code":1}}\n',
                encoding="utf-8",
                newline="\n",
            )
            with self.assertRaisesRegex(
                RuntimeError, "duplicate JSON object key: exit_code"
            ):
                read_jsonl(path)

    def test_exact_integer_exit_code_is_preserved(self) -> None:
        row = {"oracle": {"exit_code": 7}}
        self.assertEqual(required_oracle_exit_code(row, "Synthetic"), 7)
        for evaluator in (
            diagnostics_gate.oracle_satisfied,
            source_gate.evaluate_case,
            types_gate.evaluate_case,
        ):
            with self.subTest(evaluator=evaluator.__module__):
                self.assertTrue(evaluator(row, completed(7))[0])

    def test_coercible_or_missing_exit_codes_are_rejected(self) -> None:
        for value in (True, "0", 0.0, None):
            with self.subTest(value=value), self.assertRaisesRegex(
                RuntimeError, "must be an exact integer"
            ):
                required_oracle_exit_code(
                    {"oracle": {"exit_code": value}}, "Synthetic"
                )
        with self.assertRaisesRegex(RuntimeError, "omits exit_code"):
            required_oracle_exit_code({"oracle": {}}, "Synthetic")
        with self.assertRaisesRegex(RuntimeError, "not an object"):
            required_oracle_exit_code({"oracle": []}, "Synthetic")

    def test_evaluators_do_not_restore_integer_coercion(self) -> None:
        row = {"oracle": {"exit_code": "0"}}
        for evaluator in (
            diagnostics_gate.oracle_satisfied,
            source_gate.evaluate_case,
            types_gate.evaluate_case,
        ):
            with self.subTest(evaluator=evaluator.__module__), self.assertRaisesRegex(
                RuntimeError, "must be an exact integer"
            ):
                evaluator(row, completed(0))

    def test_json_oracle_compares_nested_types_exactly_without_recursion(self) -> None:
        expected = {"version": 1, "values": [False, {"name": "nsc"}]}
        self.assertTrue(json_values_equal_exact(expected, expected.copy()))
        self.assertFalse(
            json_values_equal_exact(
                expected,
                {"version": True, "values": [0, {"name": "nsc"}]},
            )
        )
        self.assertFalse(json_values_equal_exact(1, 1.0))

        row = {"oracle": {"exit_code": 0, "json": {"version": 1}}}
        passed, detail = diagnostics_gate.oracle_satisfied(
            row,
            completed(0, '{"version": true}'),
        )
        self.assertFalse(passed)
        self.assertIn("JSON mismatch", detail)

    def test_json_oracle_rejects_duplicate_keys_without_last_wins(self) -> None:
        row = {"oracle": {"exit_code": 0, "json": {"version": 2}}}
        passed, detail = diagnostics_gate.oracle_satisfied(
            row,
            completed(0, '{"version":1,"version":2}'),
        )
        self.assertFalse(passed)
        self.assertIn("duplicate JSON object key: version", detail)


if __name__ == "__main__":
    unittest.main()
