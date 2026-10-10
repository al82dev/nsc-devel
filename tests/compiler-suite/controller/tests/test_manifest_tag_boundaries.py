from __future__ import annotations

import sys
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
SUITE_ROOT = REPOSITORY_ROOT / "tests" / "compiler-suite"
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

import run_buildsystem_gate as buildsystem_gate
import run_packaging_gate as packaging_gate
from component_gate_support import read_jsonl
from selfhost_inventory import load_component_inventory, validate_manifest_cases


class ManifestTagBoundaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.rows = read_jsonl(SUITE_ROOT / "manifest" / "cases.jsonl")

    def malformed_cases(self, component: str) -> list[dict[str, object]]:
        """Replace one live defect tag array with the scalar form that used to pass."""
        cases = [dict(row) for row in self.rows if row.get("component") == component]
        target = next(
            row
            for row in cases
            if "known-product-defect" in row.get("tags", [])
        )
        target["tags"] = "known-product-defect"
        return cases

    def test_raw_gate_boundaries_reject_scalar_defect_tags(self) -> None:
        scenarios = (
            (
                "BuildSystem",
                lambda cases: buildsystem_gate.validate_lineage(
                    REPOSITORY_ROOT, cases
                ),
            ),
            (
                "Packaging",
                lambda cases: packaging_gate.validate_lineage(
                    REPOSITORY_ROOT, cases
                ),
            ),
            (
                "SelfHost.Diagnostics",
                lambda cases: validate_manifest_cases(
                    load_component_inventory(REPOSITORY_ROOT, "SelfHost.Diagnostics"),
                    cases,
                ),
            ),
            (
                "SelfHost.Lexer",
                lambda cases: validate_manifest_cases(
                    load_component_inventory(REPOSITORY_ROOT, "SelfHost.Lexer"),
                    cases,
                ),
            ),
        )
        for component, validate in scenarios:
            with self.subTest(component=component), self.assertRaisesRegex(
                RuntimeError, "field tags must be an array"
            ):
                validate(self.malformed_cases(component))


if __name__ == "__main__":
    unittest.main()
