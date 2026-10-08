from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS_ROOT = Path(__file__).resolve().parents[1]
if str(HARNESS_ROOT) not in sys.path:
    sys.path.insert(0, str(HARNESS_ROOT))

from nsc_test.catalog import CatalogError, TestCatalog  # noqa: E402


class TestCatalogTests(unittest.TestCase):
    def test_missing_catalog_is_empty(self) -> None:
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[3]) as directory:
            catalog = TestCatalog.load(Path(directory) / "missing")
            self.assertEqual(catalog.cases, ())

    def test_valid_manifest_is_loaded_and_sorted(self) -> None:
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[3]) as directory:
            root = Path(directory)
            source = root / "Main.ns"
            source.write_text("int Main() { return 0; }\n", encoding="utf-8")
            self.write_manifest(root / "case.json", "nsc.test.catalog", "parser", source.name)
            catalog = TestCatalog.load(root)
            self.assertEqual(len(catalog.cases), 1)
            self.assertEqual(catalog.require("nsc.test.catalog").component, "parser")

    def test_duplicate_identifier_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[3]) as directory:
            root = Path(directory)
            for name in ("one", "two"):
                child = root / name
                child.mkdir()
                source = child / "Main.ns"
                source.write_text("int Main() { return 0; }\n", encoding="utf-8")
                self.write_manifest(child / "case.json", "nsc.test.duplicate", "parser", source.name)
            with self.assertRaisesRegex(CatalogError, "duplicate case id"):
                TestCatalog.load(root)

    def test_auxiliary_json_is_not_treated_as_manifest(self) -> None:
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[3]) as directory:
            root = Path(directory)
            source = root / "Main.ns"
            source.write_text("int Main() { return 0; }\n", encoding="utf-8")
            (root / "expected.json").write_text('{"value": 42}', encoding="utf-8")
            self.write_manifest(root / "sample.case.json", "nsc.test.auxiliary", "parser", source.name)
            catalog = TestCatalog.load(root)
            self.assertEqual(tuple(case.identifier for case in catalog.cases), ("nsc.test.auxiliary",))

    def test_case_path_cannot_escape_manifest_directory(self) -> None:
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[3]) as directory:
            root = Path(directory)
            source = root / "Main.ns"
            source.write_text("int Main() { return 0; }\n", encoding="utf-8")
            child = root / "child"
            child.mkdir()
            self.write_manifest(child / "case.json", "nsc.test.escape", "parser", "../Main.ns")
            with self.assertRaisesRegex(CatalogError, "escapes its directory"):
                TestCatalog.load(root)

    def test_cli_lists_case_without_executing_nsc(self) -> None:
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[3]) as directory:
            root = Path(directory)
            fake_nsc = root / "nsc.exe"
            fake_nsc.write_bytes(b"not executable and must not be started")
            source = root / "Main.ns"
            source.write_text("int Main() { return 0; }\n", encoding="utf-8")
            self.write_manifest(root / "case.json", "nsc.test.list", "lexer", source.name)
            completed = subprocess.run(
                [
                    sys.executable,
                    str(HARNESS_ROOT / "run_nsc_tests.py"),
                    "--nsc",
                    str(fake_nsc),
                    "--catalog",
                    str(root),
                    "--list",
                ],
                check=False,
                capture_output=True,
                text=True,
                encoding="utf-8",
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertIn("nsc.test.list\tlexer\tcompile-only", completed.stdout)
            self.assertIn("cases=1", completed.stdout)

    @staticmethod
    def write_manifest(path: Path, identifier: str, component: str, source: str) -> None:
        payload = {
            "id": identifier,
            "component": component,
            "kind": "compile-only",
            "sources": [source],
            "optimization": "O0",
            "expected": {"exit_code": 0},
        }
        path.write_text(json.dumps(payload), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
