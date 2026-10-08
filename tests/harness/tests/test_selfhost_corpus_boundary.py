from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]


def load_module(name: str, relative: str):
    path = REPO_ROOT / relative
    specification = importlib.util.spec_from_file_location(name, path)
    if specification is None or specification.loader is None:
        raise RuntimeError(f"cannot load test module: {path}")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


FRONTEND = load_module("nsc_verify_frontend", "selfhost/bootstrap/verify_frontend.py")
SEMANTICS = load_module("nsc_verify_semantics", "selfhost/bootstrap/verify_semantics.py")
IR = load_module("nsc_verify_ir", "selfhost/bootstrap/verify_ir.py")


class SelfHostCorpusBoundaryTests(unittest.TestCase):
    def test_replacement_harness_cases_do_not_mutate_frozen_corpus(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT) as directory:
            root = Path(directory)
            tests = root / "tests"
            cases = tests / "cases" / "new-case"
            cases.mkdir(parents=True)
            historical = tests / "historical.ns"
            replacement = cases / "replacement.ns"
            historical.write_text("int Historical() { return 1; }\n", encoding="utf-8")
            replacement.write_text("int Replacement() { return 2; }\n", encoding="utf-8")

            frontend = FRONTEND.source_corpus(root, ["tests"], set())
            semantics = SEMANTICS.source_corpus(root, ["tests"])
            ir = IR.source_corpus(root, ["tests"])
            for selected in (frontend, semantics, ir):
                self.assertEqual(selected, [historical])


if __name__ == "__main__":
    unittest.main()
