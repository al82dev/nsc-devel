from __future__ import annotations

import ast
import sys
import unittest
from pathlib import Path


MODULAR_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = MODULAR_ROOT.parent
PACKAGE_ROOT = MODULAR_ROOT / "nsc_builder"
sys.path.insert(0, str(MODULAR_ROOT))

from nsc_builder.inventory import MODULE_OWNERSHIP


AUTHORIZED_MODULAR_DIVERGENCES = {
    ("configuration", "BuilderConfig"),
    ("catalog", "CanonicalCatalog"),
    ("build", "clean_test_build_trees"),
    ("execution", "ExecutionEngine"),
    ("execution", "expected_abnormal_termination_detail"),
    ("execution", "parse_identity_envelope"),
    ("gui", "launch_gui"),
    ("history", "EvidenceStore"),
    ("history", "history_record"),
    ("history", "load_history_execution"),
    ("reporting", "condensed_error_report"),
}


def definitions(path: Path) -> dict[str, ast.AST]:
    tree = ast.parse(path.read_text(encoding="utf-8"))
    return {
        node.name: node
        for node in tree.body
        if isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef))
    }


class ExtractionEquivalenceTests(unittest.TestCase):
    def test_extracted_definition_asts_match_the_current_builder(self) -> None:
        root_definitions = definitions(REPOSITORY_ROOT / "nscbuilder3.py")
        manual_owners = {"models", "paths", "toolchain"}
        for owner, names in MODULE_OWNERSHIP.items():
            if owner in manual_owners:
                continue
            module_definitions = definitions(PACKAGE_ROOT / f"{owner}.py")
            for name in names:
                with self.subTest(owner=owner, name=name):
                    self.assertIn(name, root_definitions)
                    self.assertIn(name, module_definitions)
                    if (owner, name) in AUTHORIZED_MODULAR_DIVERGENCES:
                        continue
                    self.assertEqual(
                        ast.dump(module_definitions[name], include_attributes=False),
                        ast.dump(root_definitions[name], include_attributes=False),
                    )

    def test_functional_divergences_are_explicit_and_still_present(self) -> None:
        root_definitions = definitions(REPOSITORY_ROOT / "nscbuilder3.py")
        for owner, name in AUTHORIZED_MODULAR_DIVERGENCES:
            module_definitions = definitions(PACKAGE_ROOT / f"{owner}.py")
            with self.subTest(owner=owner, name=name):
                self.assertNotEqual(
                    ast.dump(module_definitions[name], include_attributes=False),
                    ast.dump(root_definitions[name], include_attributes=False),
                )

    def test_module_ownership_is_unique(self) -> None:
        total = sum(len(names) for names in MODULE_OWNERSHIP.values())
        unique = set().union(*MODULE_OWNERSHIP.values())
        self.assertEqual(total, len(unique))
        self.assertEqual(len(unique), 91)

    def test_package_relative_import_graph_is_acyclic(self) -> None:
        graph: dict[str, set[str]] = {}
        module_names = {path.stem for path in PACKAGE_ROOT.glob("*.py")}
        for path in PACKAGE_ROOT.glob("*.py"):
            if path.stem == "__init__":
                continue
            dependencies: set[str] = set()
            tree = ast.parse(path.read_text(encoding="utf-8"))
            for node in ast.walk(tree):
                if isinstance(node, ast.ImportFrom) and node.level == 1 and node.module:
                    dependency = node.module.split(".", 1)[0]
                    if dependency in module_names:
                        dependencies.add(dependency)
            graph[path.stem] = dependencies

        visiting: set[str] = set()
        visited: set[str] = set()

        def visit(module: str) -> None:
            if module in visiting:
                self.fail(f"cyclic modular import at {module}")
            if module in visited:
                return
            visiting.add(module)
            for dependency in graph.get(module, set()):
                visit(dependency)
            visiting.remove(module)
            visited.add(module)

        for module in graph:
            visit(module)


if __name__ == "__main__":
    unittest.main()
