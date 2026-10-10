from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite.workspace import WorkspaceError, WorkspaceManager


class WorkspaceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(
            prefix="nsc-suite-workspace-", dir=Path.cwd()
        )
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_creates_separated_case_directories_and_metadata(self) -> None:
        workspace = WorkspaceManager(self.root).prepare("run-001", "TB-BAS-001")
        self.assertTrue(workspace.inputs.is_dir())
        self.assertTrue(workspace.outputs.is_dir())
        self.assertTrue(workspace.logs.is_dir())
        self.assertTrue(workspace.artifacts.is_dir())
        payload = json.loads(workspace.metadata.read_text(encoding="utf-8"))
        self.assertEqual(payload["case_id"], "TB-BAS-001")
        self.assertEqual(Path(payload["root"]), workspace.root)

    def test_refuses_to_reuse_case_workspace(self) -> None:
        manager = WorkspaceManager(self.root)
        manager.prepare("run-001", "TB-BAS-001")
        with self.assertRaisesRegex(WorkspaceError, "already exists"):
            manager.prepare("run-001", "TB-BAS-001")

    def test_rejects_path_segments(self) -> None:
        manager = WorkspaceManager(self.root)
        for value in ("", "../escape", "nested/path", "nested\\path", ".hidden"):
            with self.subTest(value=value):
                with self.assertRaisesRegex(WorkspaceError, "invalid"):
                    manager.prepare("run-001", value)

    def test_run_root_rejects_unsafe_id_before_creating_state(self) -> None:
        manager = WorkspaceManager(self.root)
        for value in ("", "../escape", "nested/path", "nested\\path", ".hidden"):
            with self.subTest(value=value), self.assertRaisesRegex(
                WorkspaceError, "invalid run_id"
            ):
                manager.run_root(value)
        self.assertEqual(tuple(self.root.iterdir()), ())

    def test_root_is_canonical_and_on_a_drive(self) -> None:
        manager = WorkspaceManager(self.root / "child" / "..")
        self.assertEqual(manager.root, self.root.resolve())
        with self.assertRaisesRegex(WorkspaceError, "must be on A"):
            WorkspaceManager(Path("C:/not-authorized"))


if __name__ == "__main__":
    unittest.main()
