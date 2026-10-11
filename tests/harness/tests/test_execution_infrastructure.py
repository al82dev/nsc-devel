from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS_ROOT = Path(__file__).resolve().parents[1]
if str(HARNESS_ROOT) not in sys.path:
    sys.path.insert(0, str(HARNESS_ROOT))

from nsc_test.model import TestCase  # noqa: E402
from nsc_test.process import ProcessExecutor, ProcessRequest  # noqa: E402
from nsc_test.report import ReportWriter, TestRunRecord  # noqa: E402
from nsc_test.workspace import CaseWorkspace, remove_tree_iterative  # noqa: E402


class ProcessExecutorTests(unittest.TestCase):
    def test_captures_stdout_stderr_and_exit_code(self) -> None:
        result = ProcessExecutor().execute(
            ProcessRequest(
                command=(
                    sys.executable,
                    "-c",
                    "import sys; print('out'); print('err', file=sys.stderr); raise SystemExit(7)",
                ),
                working_directory=Path(__file__).resolve().parents[3],
            )
        )
        self.assertEqual(result.exit_code, 7)
        self.assertEqual(result.stdout, "out\n")
        self.assertEqual(result.stderr, "err\n")


class WorkspaceTests(unittest.TestCase):
    def test_workspace_stages_case_files_and_cleans_itself(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory(dir=repo) as source_directory:
            source_root = Path(source_directory)
            source = source_root / "Main.ns"
            source.write_text("int Main() { return 0; }\n", encoding="utf-8")
            manifest = source_root / "case.json"
            manifest.write_text(
                json.dumps(
                    {
                        "id": "nsc.test.workspace",
                        "component": "driver",
                        "kind": "compile-only",
                        "sources": ["Main.ns"],
                        "expected": {"exit_code": 0},
                    }
                ),
                encoding="utf-8",
            )
            case = TestCase.from_manifest(manifest)
            work_root = repo / "_test-harness-work"
            workspace_path: Path
            with CaseWorkspace.create(case, work_root) as workspace:
                workspace_path = workspace.root
                self.assertEqual(workspace.mapped(source).read_text(encoding="utf-8"), source.read_text(encoding="utf-8"))
            self.assertFalse(workspace_path.exists())
            if work_root.exists():
                remove_tree_iterative(work_root)


class ReportWriterTests(unittest.TestCase):
    def test_writes_json_and_full_log(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        root = repo / "_test-harness-reports"
        process = ProcessExecutor().execute(
            ProcessRequest(
                command=(sys.executable, "-c", "print('report-output')"),
                working_directory=repo,
            )
        )
        output = ReportWriter(root).write(
            "run-001",
            Path(sys.executable),
            (TestRunRecord(case_id="nsc.test.report", component="driver", process=process),),
        )
        payload = json.loads((output / "summary.json").read_text(encoding="utf-8"))
        self.assertEqual(payload["selected"], 1)
        self.assertIn("report-output", (output / "report.log").read_text(encoding="utf-8"))
        remove_tree_iterative(root)


if __name__ == "__main__":
    unittest.main()
