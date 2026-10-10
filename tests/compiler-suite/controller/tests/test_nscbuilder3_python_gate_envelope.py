from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3PythonGateEnvelopeTests(unittest.TestCase):
    def test_exact_boolean_and_string_tuple_is_preserved(self) -> None:
        self.assertEqual(builder.parse_python_gate_result((True, "")), (True, ""))
        self.assertEqual(
            builder.parse_python_gate_result((False, "contract failed")),
            (False, "contract failed"),
        )

    def test_container_shape_is_not_coerced(self) -> None:
        for value in (
            [True, ""],
            (True,),
            (True, "", "extra"),
            "ok",
            None,
        ):
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, "two-item tuple"):
                    builder.parse_python_gate_result(value)

    def test_satisfaction_and_detail_types_are_not_coerced(self) -> None:
        for value, message in (
            ((1, ""), "boolean"),
            (("false", ""), "boolean"),
            ((False, None), "detail"),
            ((False, ["broken"]), "detail"),
        ):
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, message):
                    builder.parse_python_gate_result(value)

    def test_gate_exception_retains_case_evidence_as_infrastructure_error(self) -> None:
        engine = builder.ExecutionEngine(
            builder.BuilderConfig(),
            SimpleNamespace(root=Path("A:/synthetic-history")),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )
        case = SimpleNamespace(
            manifest={},
            expected_outcome="PASS",
            disposition="Executable",
        )
        variant = SimpleNamespace(case=case)
        module = SimpleNamespace(
            run_process=Mock(),
            run_case=Mock(side_effect=RuntimeError("gate broke")),
        )
        case_root = Path("A:/synthetic-history/case")
        with patch.object(engine, "_case_root", return_value=case_root), patch.object(
            engine, "_emit_case_logs"
        ) as emit_logs:
            result = engine._run_python_gate_case(module, variant, SimpleNamespace())
        self.assertEqual(result[0], "INFRASTRUCTURE_ERROR")
        self.assertIn("RuntimeError: gate broke", result[1])
        self.assertEqual(result[2], str(case_root))
        emit_logs.assert_called_once_with(case_root)

    def test_malformed_gate_result_is_infrastructure_error_not_product_failure(self) -> None:
        engine = builder.ExecutionEngine(
            builder.BuilderConfig(),
            SimpleNamespace(root=Path("A:/synthetic-history")),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )
        case = SimpleNamespace(
            manifest={},
            expected_outcome="KNOWN_FAILURE",
            disposition="KnownProductDefect",
        )
        variant = SimpleNamespace(case=case)
        module = SimpleNamespace(
            run_process=Mock(),
            run_case=Mock(return_value=("false", "broken")),
        )
        case_root = Path("A:/synthetic-history/case")
        with patch.object(engine, "_case_root", return_value=case_root), patch.object(
            engine, "_emit_case_logs"
        ):
            result = engine._run_python_gate_case(module, variant, SimpleNamespace())
        self.assertEqual(result[0], "INFRASTRUCTURE_ERROR")
        self.assertIn("boolean", result[1])
        self.assertEqual(result[2], str(case_root))


if __name__ == "__main__":
    unittest.main()
