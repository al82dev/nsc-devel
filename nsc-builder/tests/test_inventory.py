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
from nsc_builder import toolchain
from nsc_builder import utils
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
    "toolchain": toolchain,
    "utils": utils,
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
        self.assertEqual(len(actual), 91)

    def test_every_inventory_owner_exposes_its_symbols(self) -> None:
        for owner, names in MODULE_OWNERSHIP.items():
            module = MODULES[owner]
            for name in names:
                with self.subTest(owner=owner, name=name):
                    self.assertTrue(hasattr(module, name))
                    self.assertEqual(getattr(module, name).__module__, module.__name__)

    def test_old_script_is_outside_the_modular_tree(self) -> None:
        self.assertEqual(paths.SOURCE_ROOT, REPOSITORY_ROOT)
        self.assertFalse(
            (REPOSITORY_ROOT / "nscbuilder3.py").is_relative_to(MODULAR_ROOT)
        )

    def test_package_has_no_runtime_import_of_the_old_script(self) -> None:
        package_root = MODULAR_ROOT / "nsc_builder"
        for path in package_root.glob("*.py"):
            source = path.read_text(encoding="utf-8")
            with self.subTest(path=path.name):
                self.assertNotIn("._runtime", source)
                self.assertNotIn("nscbuilder3.py", source)

    def test_every_direct_subprocess_launch_has_an_explicit_window_policy(self) -> None:
        package_root = MODULAR_ROOT / "nsc_builder"
        launches = {"Popen", "run", "call", "check_call", "check_output"}
        observed: list[str] = []
        for path in package_root.glob("*.py"):
            tree = ast.parse(path.read_text(encoding="utf-8"))
            for node in ast.walk(tree):
                if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Attribute):
                    continue
                owner = node.func.value
                if (
                    not isinstance(owner, ast.Name)
                    or owner.id != "subprocess"
                    or node.func.attr not in launches
                ):
                    continue
                location = f"{path.name}:{node.lineno}"
                observed.append(location)
                self.assertIn(
                    "creationflags",
                    {keyword.arg for keyword in node.keywords},
                    f"subprocess launch has no explicit window policy: {location}",
                )
        self.assertEqual(len(observed), 5)


if __name__ == "__main__":
    unittest.main()
