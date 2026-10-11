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

from nsc_test.command import NscCommandBuilder  # noqa: E402
from nsc_test.model import TestCase  # noqa: E402
from nsc_test.process import ProcessResult  # noqa: E402
from nsc_test.verify import FailureKind, ResultVerifier  # noqa: E402
from nsc_test.workspace import CaseWorkspace, remove_tree_iterative  # noqa: E402


class CommandBuilderTests(unittest.TestCase):
    def test_freezes_nsc_nslink_and_replaces_source_placeholder(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory(dir=repo) as directory:
            root = Path(directory)
            nsc = root / "nsc.exe"
            nslink = root / "nslink.exe"
            source = root / "Main.ns"
            nsc.write_bytes(b"nsc")
            nslink.write_bytes(b"nslink")
            source.write_text("int Main() { return 0; }\n", encoding="utf-8")
            case = self.write_case(
                root,
                {
                    "id": "nsc.test.command",
                    "component": "driver",
                    "kind": "link-run",
                    "sources": ["Main.ns"],
                    "arguments": [
                        "--emit-exe",
                        "{source:0}",
                        "-o",
                        "{workspace}/Main.exe",
                    ],
                    "expected": {"exit_code": 0},
                },
            )
            work_root = repo / "_test-command-work"
            with CaseWorkspace.create(case, work_root) as workspace:
                request = NscCommandBuilder(nsc, nslink).build(case, workspace)
                self.assertEqual(request.command[0], str(nsc.resolve()))
                self.assertEqual(
                    request.environment,
                    {
                        "TEMP": str(workspace.root),
                        "TMP": str(workspace.root),
                        "NSC_LINKER": str(nslink.resolve()),
                    },
                )
                self.assertIn("-O0", request.command)
                self.assertEqual(request.command.count(str(workspace.mapped(source))), 1)
                self.assertIn(str(workspace.root / "Main.exe"), request.command)
                optimized = NscCommandBuilder(nsc, nslink, optimization="O3").build(case, workspace)
                self.assertIn("-O3", optimized.command)
                self.assertNotIn("-O0", optimized.command)
            if work_root.exists():
                remove_tree_iterative(work_root)

    def test_link_case_requires_explicit_nslink(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory(dir=repo) as directory:
            root = Path(directory)
            nsc = root / "nsc.exe"
            source = root / "Main.ns"
            nsc.write_bytes(b"nsc")
            source.write_text("int Main() { return 0; }\n", encoding="utf-8")
            case = self.write_case(
                root,
                {
                    "id": "nsc.test.linker-required",
                    "component": "linker",
                    "kind": "link-run",
                    "sources": ["Main.ns"],
                    "expected": {"exit_code": 0},
                },
            )
            work_root = repo / "_test-command-work"
            with CaseWorkspace.create(case, work_root) as workspace:
                with self.assertRaisesRegex(ValueError, "requires an explicit NSLink"):
                    NscCommandBuilder(nsc).build(case, workspace)
            if work_root.exists():
                remove_tree_iterative(work_root)

    @staticmethod
    def write_case(root: Path, payload: dict[str, object]) -> TestCase:
        manifest = root / "case.json"
        manifest.write_text(json.dumps(payload), encoding="utf-8")
        return TestCase.from_manifest(manifest)


class ResultVerifierTests(unittest.TestCase):
    def test_verifies_multiline_regular_expression(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory(dir=repo) as directory:
            root = Path(directory)
            source = root / "Main.ns"
            source.write_text("int Main() { return 0; }\n", encoding="utf-8")
            manifest = root / "case.json"
            manifest.write_text(
                json.dumps(
                    {
                        "id": "nsc.test.regex",
                        "component": "hir",
                        "kind": "dump",
                        "sources": ["Main.ns"],
                        "expected": {
                            "exit_code": 0,
                            "stdout": {"mode": "regex", "text": "first.*last"},
                        },
                    }
                ),
                encoding="utf-8",
            )
            case = TestCase.from_manifest(manifest)
            work_root = repo / "_test-verifier-work"
            with CaseWorkspace.create(case, work_root) as workspace:
                process = ProcessResult(
                    command=("nsc.exe",),
                    working_directory=workspace.root,
                    exit_code=0,
                    stdout="first\nintermediate\nlast\n",
                    stderr="",
                    duration_seconds=0.1,
                )
                result = ResultVerifier().verify(case, workspace, process)
                self.assertTrue(result.passed)
            if work_root.exists():
                remove_tree_iterative(work_root)

    def test_verifies_exit_text_and_artifact(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory(dir=repo) as directory:
            root = Path(directory)
            source = root / "Main.ns"
            source.write_text("int Main() { return 0; }\n", encoding="utf-8")
            manifest = root / "case.json"
            manifest.write_text(
                json.dumps(
                    {
                        "id": "nsc.test.verify",
                        "component": "driver",
                        "kind": "compile-only",
                        "sources": ["Main.ns"],
                        "expected": {
                            "exit_code": 0,
                            "stdout": {"mode": "contains", "text": "compiled"},
                            "stderr": {"mode": "not-contains", "text": "error"},
                            "artifacts": ["Main.obj"],
                        },
                    }
                ),
                encoding="utf-8",
            )
            case = TestCase.from_manifest(manifest)
            work_root = repo / "_test-verifier-work"
            with CaseWorkspace.create(case, work_root) as workspace:
                (workspace.root / "Main.obj").write_bytes(b"object")
                process = ProcessResult(
                    command=("nsc.exe",),
                    working_directory=workspace.root,
                    exit_code=0,
                    stdout="compiled Main.ns\n",
                    stderr="",
                    duration_seconds=0.1,
                )
                result = ResultVerifier().verify(case, workspace, process)
                self.assertTrue(result.passed)
                self.assertEqual(result.failures, ())
            if work_root.exists():
                remove_tree_iterative(work_root)

    def test_reports_all_independent_failures(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory(dir=repo) as directory:
            root = Path(directory)
            source = root / "Main.ns"
            source.write_text("int Main() { return 0; }\n", encoding="utf-8")
            manifest = root / "case.json"
            manifest.write_text(
                json.dumps(
                    {
                        "id": "nsc.test.failures",
                        "component": "driver",
                        "kind": "compile-only",
                        "sources": ["Main.ns"],
                        "expected": {
                            "exit_code": 0,
                            "stdout": "expected\n",
                            "artifacts": ["missing.obj"],
                            "run_exit_code": 42,
                        },
                    }
                ),
                encoding="utf-8",
            )
            case = TestCase.from_manifest(manifest)
            work_root = repo / "_test-verifier-work"
            with CaseWorkspace.create(case, work_root) as workspace:
                process = ProcessResult(
                    command=("nsc.exe",),
                    working_directory=workspace.root,
                    exit_code=1,
                    stdout="different\n",
                    stderr="",
                    duration_seconds=0.1,
                )
                result = ResultVerifier().verify(case, workspace, process)
                self.assertFalse(result.passed)
                self.assertEqual(
                    {failure.kind for failure in result.failures},
                    {
                        FailureKind.COMPILER_EXIT,
                        FailureKind.STDOUT,
                        FailureKind.MISSING_ARTIFACT,
                        FailureKind.PRODUCT_NOT_RUN,
                    },
                )
            if work_root.exists():
                remove_tree_iterative(work_root)


class CliExecutionTests(unittest.TestCase):
    def test_cli_executes_selected_case_and_writes_verified_report(self) -> None:
        repo = Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory(dir=repo) as directory:
            root = Path(directory)
            manifest = root / "case.json"
            manifest.write_text(
                json.dumps(
                    {
                        "id": "nsc.test.cli-execution",
                        "component": "infrastructure",
                        "kind": "specialized",
                        "arguments": ["-c", "print('controlled-output')"],
                        "expected": {"exit_code": 0, "stdout": "controlled-output\n"},
                    }
                ),
                encoding="utf-8",
            )
            completed = subprocess.run(
                [
                    sys.executable,
                    str(HARNESS_ROOT / "run_nsc_tests.py"),
                    "--nsc",
                    sys.executable,
                    "--catalog",
                    str(root),
                    "--work-root",
                    str(root / "work"),
                    "--results-root",
                    str(root / "results"),
                    "--run-id",
                    "controlled-run",
                    "--all",
                ],
                check=False,
                capture_output=True,
                text=True,
                encoding="utf-8",
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertIn("passed=1", completed.stdout)
            payload = json.loads(
                (root / "results" / "controlled-run" / "summary.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(payload["passed"], 1)
            self.assertEqual(payload["failed"], 0)


if __name__ == "__main__":
    unittest.main()
