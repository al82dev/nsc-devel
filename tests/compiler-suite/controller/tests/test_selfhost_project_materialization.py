from __future__ import annotations

import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
INTEGRATION_ROOT = REPOSITORY_ROOT / "tests" / "compiler-suite" / "controller" / "integration"
sys.path.insert(0, str(INTEGRATION_ROOT))

from selfhost_inventory import materialize_contained_project


PROJECT_NAMES = (
    "SelfHostBasic.nsproj",
    "SelfHostDiagnostics.nsproj",
    "SelfHostLexer.nsproj",
    "SelfHostSource.nsproj",
    "SelfHostTypes.nsproj",
)
TEST_TEMP_ROOT = REPOSITORY_ROOT / "_nscbuilder3" / "test-temp"


def temporary_directory() -> tempfile.TemporaryDirectory[str]:
    """Keep directed test artifacts on the repository-owned A: workspace."""
    TEST_TEMP_ROOT.mkdir(parents=True, exist_ok=True)
    return tempfile.TemporaryDirectory(dir=TEST_TEMP_ROOT)


class SelfHostProjectMaterializationTests(unittest.TestCase):
    def test_live_projects_are_materialized_without_parent_segments(self) -> None:
        project_root = REPOSITORY_ROOT / "tests" / "compiler-suite" / "selfhost" / "modules"
        with temporary_directory() as temporary:
            temporary_root = Path(temporary)
            for name in PROJECT_NAMES:
                with self.subTest(project=name):
                    generated = materialize_contained_project(
                        REPOSITORY_ROOT,
                        project_root / name,
                        temporary_root / Path(name).stem,
                    )
                    tree = ET.parse(generated)
                    compile_items = list(tree.getroot().iter("Compile"))
                    self.assertTrue(compile_items)
                    for item in compile_items:
                        include = item.get("Include")
                        self.assertIsNotNone(include)
                        self.assertNotIn("..", Path(include).parts)
                        copied = (generated.parent / include).resolve()
                        copied.relative_to(generated.parent.resolve())
                        self.assertTrue(copied.is_file())
                    for item in tree.getroot().iter("ProjectReference"):
                        reference = Path(item.get("Include", ""))
                        self.assertTrue(reference.is_absolute())
                        reference.resolve().relative_to(REPOSITORY_ROOT.resolve())
                        self.assertTrue(reference.is_file())

    def test_source_bytes_are_preserved_exactly(self) -> None:
        source_project = (
            REPOSITORY_ROOT
            / "tests"
            / "compiler-suite"
            / "selfhost"
            / "modules"
            / "SelfHostBasic.nsproj"
        )
        source_tree = ET.parse(source_project)
        with temporary_directory() as temporary:
            generated = materialize_contained_project(
                REPOSITORY_ROOT, source_project, Path(temporary) / "project"
            )
            generated_tree = ET.parse(generated)
            source_items = list(source_tree.getroot().iter("Compile"))
            generated_items = list(generated_tree.getroot().iter("Compile"))
            self.assertEqual(len(source_items), len(generated_items))
            for source_item, generated_item in zip(source_items, generated_items):
                original = (source_project.parent / source_item.get("Include", "")).resolve()
                copied = (generated.parent / generated_item.get("Include", "")).resolve()
                self.assertEqual(copied.read_bytes(), original.read_bytes())

    def test_destination_reuse_and_repository_escape_are_rejected(self) -> None:
        project = (
            REPOSITORY_ROOT
            / "tests"
            / "compiler-suite"
            / "selfhost"
            / "modules"
            / "SelfHostBasic.nsproj"
        )
        with temporary_directory() as temporary:
            destination = Path(temporary) / "project"
            materialize_contained_project(REPOSITORY_ROOT, project, destination)
            with self.assertRaisesRegex(RuntimeError, "already exists"):
                materialize_contained_project(REPOSITORY_ROOT, project, destination)
            with self.assertRaisesRegex(RuntimeError, "outside the repository"):
                materialize_contained_project(
                    REPOSITORY_ROOT,
                    REPOSITORY_ROOT.parent / "outside.nsproj",
                    Path(temporary) / "other",
                )


if __name__ == "__main__":
    unittest.main()
