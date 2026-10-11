from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path
from unittest.mock import patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class FakeProcess:
    def __init__(self, *, running: bool = True, wait_timeouts: int = 0) -> None:
        self.pid = 4321
        self.returncode = None if running else 0
        self.wait_timeouts = wait_timeouts
        self.kill_calls = 0
        self.wait_calls: list[float | None] = []

    def poll(self) -> int | None:
        return self.returncode

    def kill(self) -> None:
        self.kill_calls += 1
        self.returncode = -9

    def wait(self, timeout: float | None = None) -> int:
        self.wait_calls.append(timeout)
        if self.wait_timeouts:
            self.wait_timeouts -= 1
            raise subprocess.TimeoutExpired("fake", timeout)
        if self.returncode is None:
            self.returncode = 1
        return self.returncode


class Builder3ProcessTerminationTests(unittest.TestCase):
    def test_cancel_before_launch_is_emitted_to_progressive_log(self) -> None:
        emitted: list[str] = []
        controller = builder.ProcessController(emitted.append)
        controller.cancelled.set()

        code, captured = controller.run(["unused"], Path("A:/nsc-devel"))

        self.assertEqual(code, 130)
        self.assertIn("cancelled before launch", captured)
        self.assertTrue(any("cancelled before launch" in line for line in emitted))
        self.assertEqual(controller.last_termination_reason, "cancelled")

    @patch.object(builder.os, "name", "nt")
    @patch.object(builder.subprocess, "run")
    def test_already_exited_process_is_not_touched(self, run: object) -> None:
        process = FakeProcess(running=False)

        builder.ProcessController._terminate_tree(process)  # type: ignore[arg-type]

        run.assert_not_called()
        self.assertEqual(process.kill_calls, 0)
        self.assertEqual(process.wait_calls, [])

    @patch.object(builder.os, "name", "nt")
    @patch.object(builder.subprocess, "run")
    def test_successful_taskkill_is_followed_by_wait(self, run: object) -> None:
        run.return_value = subprocess.CompletedProcess([], 0)
        process = FakeProcess()

        builder.ProcessController._terminate_tree(process)  # type: ignore[arg-type]

        self.assertEqual(process.kill_calls, 0)
        self.assertEqual(process.wait_calls, [5.0])

    @patch.object(builder.os, "name", "nt")
    @patch.object(builder.subprocess, "run")
    def test_failed_taskkill_falls_back_to_owned_child_kill(self, run: object) -> None:
        run.return_value = subprocess.CompletedProcess([], 1)
        process = FakeProcess()

        builder.ProcessController._terminate_tree(process)  # type: ignore[arg-type]

        self.assertEqual(process.kill_calls, 1)
        self.assertEqual(process.wait_calls, [5.0])

    @patch.object(builder.os, "name", "nt")
    @patch.object(builder.subprocess, "run")
    def test_wait_timeout_forces_child_and_waits_again(self, run: object) -> None:
        run.return_value = subprocess.CompletedProcess([], 0)
        process = FakeProcess(wait_timeouts=1)

        builder.ProcessController._terminate_tree(process)  # type: ignore[arg-type]

        self.assertEqual(process.kill_calls, 1)
        self.assertEqual(process.wait_calls, [5.0, 5.0])


if __name__ == "__main__":
    unittest.main()
