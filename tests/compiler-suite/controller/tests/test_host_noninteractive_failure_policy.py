from __future__ import annotations

import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
HOST_MAIN = REPOSITORY_ROOT / "tests" / "compiler-suite" / "host" / "src" / "main.cpp"


class HostNonInteractiveFailurePolicyTests(unittest.TestCase):
    """Guard the Windows process policy that prevents modal test failures."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.source = HOST_MAIN.read_text(encoding="utf-8")

    def test_process_error_mode_suppresses_system_fault_dialogs(self) -> None:
        for flag in (
            "SEM_FAILCRITICALERRORS",
            "SEM_NOGPFAULTERRORBOX",
            "SEM_NOOPENFILEERRORBOX",
        ):
            self.assertIn(flag, self.source)

    def test_crt_reports_are_routed_to_stderr_without_abort_ui(self) -> None:
        self.assertIn("_set_error_mode(_OUT_TO_STDERR)", self.source)
        self.assertIn(
            "_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT)",
            self.source,
        )
        for report_type in ("_CRT_WARN", "_CRT_ERROR", "_CRT_ASSERT"):
            self.assertIn(
                f"_CrtSetReportFile({report_type}, _CRTDBG_FILE_STDERR)",
                self.source,
            )

    def test_policy_is_installed_before_argument_processing(self) -> None:
        policy = self.source.index("configureNonInteractiveFailureReporting();")
        arguments = self.source.index("parseOptions(argumentCount, arguments)")
        self.assertLess(policy, arguments)


if __name__ == "__main__":
    unittest.main()
