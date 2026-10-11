from __future__ import annotations

import ast
import sys
import unittest
from pathlib import Path


MODULAR_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = MODULAR_ROOT.parent
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder import build
from nsc_builder import catalog
from nsc_builder import cli
from nsc_builder import configuration
from nsc_builder import execution
from nsc_builder import gui
from nsc_builder import history
from nsc_builder import models
from nsc_builder import paths
from nsc_builder import process_control
from nsc_builder import reporting
from nsc_builder import runtime_support
from nsc_builder.inventory import EXPECTED_TOP_LEVEL, MODULE_OWNERSHIP


MODULES = {
    "build": build,
    "catalog": catalog,
    "cli": cli,
    "configuration": configuration,
    "execution": execution,
    "gui": gui,
    "history": history,
    "models": models,
    "paths": paths,
    "process_control": process_control,
    "reporting": reporting,
    "runtime_support": runtime_support,
}


class ModularInventoryTests(unittest.TestCase):
    def test_inventory_covers_every_top_level_class_and_function(self) -> None:
        source = (REPOSITORY_ROOT / "nscbuilder3.py").read_text(encoding="utf-8")
        tree = ast.parse(source)
        actual = {
            node.name
            for node in tree.body
            if isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef))
        }

        self.assertEqual(actual, EXPECTED_TOP_LEVEL)
        self.assertEqual(len(actual), 85)

    def test_every_inventory_owner_exposes_its_symbols(self) -> None:
        for owner, names in MODULE_OWNERSHIP.items():
            if owner == "states":
                continue
            module = MODULES[owner]
            for name in names:
                with self.subTest(owner=owner, name=name):
                    self.assertTrue(hasattr(module, name))

    def test_old_script_is_outside_the_modular_tree(self) -> None:
        self.assertEqual(paths.SOURCE_ROOT, REPOSITORY_ROOT)
        self.assertFalse(
            (REPOSITORY_ROOT / "nscbuilder3.py").is_relative_to(MODULAR_ROOT)
        )


if __name__ == "__main__":
    unittest.main()
