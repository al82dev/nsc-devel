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
from nsc_builder import cli
from nsc_builder.cli import main
from nsc_builder.gui import launch_gui


class ModularParityTests(unittest.TestCase):
    def test_cohesive_algorithms_are_owned_by_modular_modules(self) -> None:
        values = (
            (build.clean_test_build_trees, "nsc_builder.build"),
            (catalog.CanonicalCatalog, "nsc_builder.catalog"),
            (execution.ExecutionEngine, "nsc_builder.execution"),
            (history.EvidenceStore, "nsc_builder.history"),
            (reporting.condensed_error_report, "nsc_builder.reporting"),
            (launch_gui, "nsc_builder.gui"),
        )
        for exported, owner in values:
            with self.subTest(symbol=exported.__name__):
                self.assertEqual(exported.__module__, owner)

    def test_timeout_policy_and_visible_actions_match_current_builder(self) -> None:
        expected_limits = {
            None: 2700,
            0.0: 5,
            0.999: 5,
            1.0: 6,
            30.0: 35,
            120.0: 135,
            600.0: 780,
            1800.0: 2160,
        }
        for average, expected in expected_limits.items():
            with self.subTest(average=average):
                self.assertEqual(history.timeout_limit_for_average(average), expected)
        expected_actions = {
            "LISTO": "Ejecutar",
            "RUNNING": "Cancelar prueba",
            "COMPILING": "En ejecución",
            "PASS": "Volver a correr",
            "TIMEOUT": "Volver a correr",
        }
        for status, expected in expected_actions.items():
            with self.subTest(status=status):
                self.assertEqual(reporting.row_action_text(status, True), expected)

    def test_cli_audit_contract_uses_the_same_runtime_boundary(self) -> None:
        expected = {"cases": 5711, "variants": 5711}
        output = io.StringIO()
        with patch.object(cli, "catalog_audit", return_value=expected):
            with redirect_stdout(output):
                exit_code = main(["--audit-catalog"])

        self.assertEqual(exit_code, 0)
        self.assertEqual(json.loads(output.getvalue()), expected)


if __name__ == "__main__":
    unittest.main()
