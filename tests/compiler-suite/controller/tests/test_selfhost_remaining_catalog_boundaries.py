from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import ModuleType
from unittest import mock


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
REPOSITORY_ROOT = CONTROLLER_ROOT.parents[2]
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

import run_selfhost_bootstrap_infrastructure_gate as bootstrap_gate
import run_selfhost_contracts_gate as contracts_gate
import run_selfhost_documentation_gate as documentation_gate
import run_selfhost_project_gate as project_gate


class SelfHostRemainingCatalogBoundaryTests(unittest.TestCase):
    def run_gate(self, module: ModuleType) -> int:
        with mock.patch.object(
            sys,
            "argv",
            [module.__file__ or module.__name__, "--repository-root", str(REPOSITORY_ROOT)],
        ):
            return module.main()

    def test_live_selfhost_catalogs_satisfy_exact_boundaries(self) -> None:
        for module in (
            project_gate,
            bootstrap_gate,
            contracts_gate,
            documentation_gate,
        ):
            with self.subTest(module=module.__name__):
                self.assertEqual(self.run_gate(module), 0)

    def test_retired_gates_do_not_consume_removed_classification_sources(self) -> None:
        removed_names = (
            "PlanContradictions.jsonl",
            "InfrastructurePending",
            "validate_pending_manifest",
        )
        for module in (
            project_gate,
            bootstrap_gate,
            contracts_gate,
            documentation_gate,
        ):
            source = Path(module.__file__ or "").read_text(encoding="utf-8")
            with self.subTest(module=module.__name__):
                for name in removed_names:
                    self.assertNotIn(name, source)


if __name__ == "__main__":
    unittest.main()
