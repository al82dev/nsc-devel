from __future__ import annotations

import io
import json
import sys
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch


MODULAR_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder import build, catalog, execution, history, reporting
from nsc_builder._runtime import legacy
from nsc_builder.cli import main
from nsc_builder.gui import launch_gui


class ModularParityTests(unittest.TestCase):
    def test_cohesive_modules_expose_the_exact_runtime_algorithms(self) -> None:
        runtime = legacy()
        pairs = (
            (build.clean_test_build_trees, runtime.clean_test_build_trees),
            (catalog.CanonicalCatalog, runtime.CanonicalCatalog),
            (execution.ExecutionEngine, runtime.ExecutionEngine),
            (history.EvidenceStore, runtime.EvidenceStore),
            (history.timeout_limit_for_average, runtime.timeout_limit_for_average),
            (reporting.condensed_error_report, runtime.condensed_error_report),
            (launch_gui, runtime.launch_gui),
        )
        for exported, expected in pairs:
            with self.subTest(symbol=expected.__name__):
                self.assertIs(exported, expected)

    def test_timeout_policy_and_visible_actions_match_current_builder(self) -> None:
        runtime = legacy()
        for average in (None, 0.0, 0.999, 1.0, 30.0, 120.0, 600.0, 1800.0):
            with self.subTest(average=average):
                self.assertEqual(
                    history.timeout_limit_for_average(average),
                    runtime.timeout_limit_for_average(average),
                )
        for status in ("LISTO", "RUNNING", "COMPILING", "PASS", "TIMEOUT"):
            with self.subTest(status=status):
                self.assertEqual(
                    reporting.row_action_text(status, True),
                    runtime.row_action_text(status, True),
                )

    def test_cli_audit_contract_uses_the_same_runtime_boundary(self) -> None:
        runtime = legacy()
        expected = {"cases": 5707, "variants": 5707}
        output = io.StringIO()
        with patch.object(runtime, "catalog_audit", return_value=expected):
            with redirect_stdout(output):
                exit_code = main(["--audit-catalog"])

        self.assertEqual(exit_code, 0)
        self.assertEqual(json.loads(output.getvalue()), expected)


if __name__ == "__main__":
    unittest.main()
