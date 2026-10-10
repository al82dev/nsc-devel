from __future__ import annotations

import json
import sys
import tempfile
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
from component_gate_support import component_manifest_rows, read_jsonl
from nsc_suite.manifest import ManifestError
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

    def test_raw_component_loader_uses_the_canonical_manifest_schema(self) -> None:
        mutations = (
            ("class", 1, "class must be a non-empty string"),
            ("executor", [], "executor must be a non-empty string"),
            ("inputs", [], "inputs must be an object"),
            ("oracle", [], "oracle must be a non-empty object"),
            ("dependencies", {}, "dependencies must be an array"),
            ("profile", [], "profile must be a non-empty string"),
            ("build_modes", "Release", "build_modes must be an array"),
            ("optimizations", "O0", "optimizations must be an array"),
            ("isolation", [], "isolation must be a non-empty string"),
            ("timeout_class", [], "timeout_class must be a non-empty string"),
            ("expected_outcome", "INVALID", "invalid expected_outcome"),
            ("unknown_field", True, "unknown fields"),
        )
        for component in ("BuildSystem", "SelfHost.Diagnostics"):
            for field, value, message in mutations:
                with self.subTest(component=component, field=field):
                    rows = [dict(row) for row in self.rows]
                    target = next(
                        row for row in rows if row.get("component") == component
                    )
                    target[field] = value
                    with tempfile.TemporaryDirectory(
                        prefix="raw-manifest-schema-", dir=Path.cwd()
                    ) as root:
                        source_root = Path(root)
                        manifest = source_root / "manifest" / "cases.jsonl"
                        manifest.parent.mkdir(parents=True)
                        manifest.write_text(
                            "".join(json.dumps(row) + "\n" for row in rows),
                            encoding="utf-8",
                            newline="\n",
                        )
                        with self.assertRaisesRegex(ManifestError, message):
                            component_manifest_rows(
                                source_root, REPOSITORY_ROOT, component
                            )

    def test_all_raw_manifest_consumers_use_the_shared_boundary(self) -> None:
        consumers = (
            "run_buildsystem_gate.py",
            "run_packaging_gate.py",
            "run_selfhost_basic_gate.py",
            "run_selfhost_diagnostics_gate.py",
            "run_selfhost_lexer_gate.py",
            "run_selfhost_source_gate.py",
            "run_selfhost_types_gate.py",
        )
        for name in consumers:
            with self.subTest(consumer=name):
                text = (INTEGRATION_ROOT / name).read_text(encoding="utf-8")
                self.assertIn("component_manifest_rows(", text)


if __name__ == "__main__":
    unittest.main()
