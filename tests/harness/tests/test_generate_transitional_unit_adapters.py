"""Directed tests for deterministic historical unit adapter generation."""

from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "generate_transitional_unit_adapters",
    ROOT / "tools" / "generate_transitional_unit_adapters.py",
)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class GenerateTransitionalUnitAdaptersTests(unittest.TestCase):
    def test_generates_one_translation_unit_per_owner(self) -> None:
        with tempfile.TemporaryDirectory(dir="A:\\") as temporary:
            root = Path(temporary)
            source = root / "source"
            output = root / "output"
            source.mkdir()
            (source / "One.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
            (source / "Two.cpp").write_text("int main(void) { return 1; }\n", encoding="utf-8")
            registry = root / "registry.tsv"
            registry.write_text(
                "case.one\tfrontend\tOne.cpp\tnscCaseOneEntry\n"
                "case.two\tdriver\tTwo.cpp\tnscCaseTwoEntry\n",
                encoding="utf-8",
            )
            entries = MODULE.load_registry(registry, source)
            definitions, cmake = MODULE.generate(entries, output)
            adapters = sorted(output.glob("*Adapter.cpp"))
            self.assertEqual(len(adapters), 2)
            self.assertIn("nscCaseOneEntry", adapters[0].read_text(encoding="utf-8"))
            self.assertIn("case.two", definitions.read_text(encoding="utf-8"))
            self.assertIn(adapters[1].as_posix(), cmake.read_text(encoding="utf-8"))

    def test_rejects_duplicate_identity_owner_and_symbol(self) -> None:
        with tempfile.TemporaryDirectory(dir="A:\\") as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            (source / "One.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
            registry = root / "registry.tsv"
            registry.write_text(
                "case.one\tfrontend\tOne.cpp\tnscSameEntry\n"
                "case.one\tdriver\tOne.cpp\tnscSameEntry\n",
                encoding="utf-8",
            )
            with self.assertRaises(MODULE.GenerationError):
                MODULE.load_registry(registry, source)

    def test_rejects_argument_entry_point(self) -> None:
        with tempfile.TemporaryDirectory(dir="A:\\") as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            (source / "Args.cpp").write_text(
                "int main(int argc, char** argv) { return argc + (argv != nullptr); }\n",
                encoding="utf-8",
            )
            registry = root / "registry.tsv"
            registry.write_text(
                "case.args\tfrontend\tArgs.cpp\tnscArgsEntry\n", encoding="utf-8"
            )
            with self.assertRaises(MODULE.GenerationError):
                MODULE.load_registry(registry, source)

    def test_removes_only_stale_adapter_sources(self) -> None:
        with tempfile.TemporaryDirectory(dir="A:\\") as temporary:
            output = Path(temporary)
            stale = output / "OldAdapter.cpp"
            preserved = output / "receipt.txt"
            stale.write_text("stale", encoding="utf-8")
            preserved.write_text("keep", encoding="utf-8")
            MODULE.clean_generated_sources(output)
            self.assertFalse(stale.exists())
            self.assertTrue(preserved.exists())


if __name__ == "__main__":
    unittest.main()
