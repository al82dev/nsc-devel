from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class CompletedDualProcess:
    def __init__(self) -> None:
        self.pid = 2468
        self.returncode = 0
        self.stdout = ["standard output\n"]
        self.stderr = ["standard error\n"]

    def poll(self) -> int:
        return 0


class RunningDualProcess:
    def __init__(self) -> None:
        self.pid = 1357
        self.returncode: int | None = None
        self.stdout: list[str] = []
        self.stderr: list[str] = []

    def poll(self) -> int | None:
        return self.returncode


class Builder3PythonGateProcessTests(unittest.TestCase):
    @patch.object(builder.subprocess, "Popen", return_value=CompletedDualProcess())
    def test_separate_capture_preserves_both_completed_process_channels(
        self, _popen: object
    ) -> None:
        emitted: list[str] = []
        controller = builder.ProcessController(emitted.append)

        completed = controller.run_completed(
            ["synthetic"], Path("A:/nsc-devel"), timeout=10.0
        )

        self.assertEqual(completed.returncode, 0)
        self.assertEqual(completed.stdout, "standard output\n")
        self.assertEqual(completed.stderr, "standard error\n")
        self.assertIn("standard output\n", emitted)
        self.assertIn("standard error\n", emitted)

    def test_separate_capture_honours_cancellation_before_launch(self) -> None:
        emitted: list[str] = []
        controller = builder.ProcessController(emitted.append)
        controller.cancelled.set()

        with patch.object(builder.subprocess, "Popen") as popen:
            completed = controller.run_completed(["unused"], Path("A:/nsc-devel"))

        popen.assert_not_called()
        self.assertEqual(completed.returncode, 130)
        self.assertIn("cancelled before launch", completed.stderr)
        self.assertEqual(controller.last_termination_reason, "cancelled")

    @patch.object(builder.time, "monotonic", side_effect=(0.0, 2.0))
    @patch.object(builder.subprocess, "Popen")
    def test_separate_capture_honours_active_timeout(
        self, popen: object, _monotonic: object
    ) -> None:
        controller = builder.ProcessController(lambda _line: None)
        process = RunningDualProcess()
        popen.return_value = process

        def terminate(owned: RunningDualProcess) -> None:
            owned.returncode = 130

        with patch.object(controller, "_terminate_tree", side_effect=terminate):
            completed = controller.run_completed(
                ["synthetic"], Path("A:/nsc-devel"), timeout=1.0
            )

        self.assertEqual(completed.returncode, 130)
        self.assertIn("timed out", completed.stderr)
        self.assertEqual(controller.last_termination_reason, "timeout")

    @patch.object(builder.subprocess, "Popen")
    def test_separate_capture_honours_active_cancellation(self, popen: object) -> None:
        controller = builder.ProcessController(lambda _line: None)
        process = RunningDualProcess()

        def launch(*_args: object, **_kwargs: object) -> RunningDualProcess:
            controller.cancelled.set()
            return process

        popen.side_effect = launch

        def terminate(owned: RunningDualProcess) -> None:
            owned.returncode = 130

        with patch.object(controller, "_terminate_tree", side_effect=terminate):
            completed = controller.run_completed(["synthetic"], Path("A:/nsc-devel"))

        self.assertEqual(completed.returncode, 130)
        self.assertIn("cancelled", completed.stderr)
        self.assertEqual(controller.last_termination_reason, "cancelled")

    def test_gate_runner_is_injected_and_restored_without_changing_oracle(self) -> None:
        engine = builder.ExecutionEngine(
            builder.BuilderConfig(),
            SimpleNamespace(root=Path("A:/synthetic-history")),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )
        case_root = Path("A:/synthetic-history/case")
        original_runner = Mock(name="original_runner")
        module = SimpleNamespace(run_process=original_runner)

        def run_case(_manifest: object, _options: object, root: Path, env: object):
            completed = module.run_process(["tool"], root, env, timeout=7.0)
            return completed.stdout == "out" and completed.stderr == "err", "oracle"

        module.run_case = run_case
        variant = SimpleNamespace(
            case=SimpleNamespace(
                manifest={}, expected_outcome="PASS", disposition="Executable"
            )
        )
        completed = subprocess.CompletedProcess(["tool"], 0, "out", "err")
        with patch.object(engine, "_case_root", return_value=case_root), patch.object(
            engine, "_emit_case_logs"
        ), patch.object(engine.process, "run_completed", return_value=completed) as runner:
            result = engine._run_python_gate_case(module, variant, SimpleNamespace())

        self.assertEqual(result[0], "PASS")
        self.assertIs(module.run_process, original_runner)
        runner.assert_called_once_with(["tool"], case_root, unittest.mock.ANY, 7.0)

    def test_gate_runner_is_restored_after_oracle_exception(self) -> None:
        engine = builder.ExecutionEngine(
            builder.BuilderConfig(),
            SimpleNamespace(root=Path("A:/synthetic-history")),
            lambda _line: None,
            lambda *_args: None,
            lambda *_args: None,
        )
        original_runner = Mock(name="original_runner")
        module = SimpleNamespace(
            run_process=original_runner,
            run_case=Mock(side_effect=RuntimeError("oracle failed")),
        )
        variant = SimpleNamespace(
            case=SimpleNamespace(
                manifest={}, expected_outcome="PASS", disposition="Executable"
            )
        )
        with patch.object(
            engine, "_case_root", return_value=Path("A:/synthetic-history/case")
        ), patch.object(engine, "_emit_case_logs"):
            result = engine._run_python_gate_case(module, variant, SimpleNamespace())

        self.assertEqual(result[0], "INFRASTRUCTURE_ERROR")
        self.assertIs(module.run_process, original_runner)


if __name__ == "__main__":
    unittest.main()
