from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


COMMANDS = (
    ("Configurar Debug", ["cmake", "-S", "."], Path("A:/nsc-devel")),
    ("Construir Debug", ["cmake", "--build", "debug"], Path("A:/nsc-devel")),
    ("Configurar Release", ["cmake", "-S", "."], Path("A:/nsc-devel")),
)


class Builder3CommandCancellationTests(unittest.TestCase):
    def test_failed_command_preserves_first_causal_diagnostic(self) -> None:
        samples = (
            (
                "[12/20] Linking CXX executable nsc_component_tests.exe\n"
                "lld-link: error: undefined symbol: Product::missing()\n"
                "ninja: build stopped: subcommand failed.\n",
                "lld-link: error: undefined symbol: Product::missing()",
            ),
            (
                "-- Configuring incomplete\n"
                "CMake Error at CMakeLists.txt:56 (message):\n"
                "  NSLink is required for product cases\n",
                "CMake Error at CMakeLists.txt:56 (message): — "
                "NSLink is required for product cases",
            ),
            (
                "Source.cpp(42,7): error C2039: 'missing': is not a member\n",
                "Source.cpp(42,7): error C2039: 'missing': is not a member",
            ),
        )
        for captured, expected in samples:
            with self.subTest(expected=expected):
                outcome = builder.infrastructure_command_outcome(
                    index=1,
                    label="Construir Debug",
                    exit_code=4294967295,
                    duration_seconds=0.25,
                    cancelled=False,
                    evidence="full.log",
                    captured_output=captured,
                )

                self.assertEqual(outcome.status, "FAIL")
                self.assertEqual(
                    outcome.detail,
                    f"exit code 4294967295: {expected}",
                )
                self.assertEqual(outcome.evidence, "full.log")

    def test_failed_command_without_diagnostic_retains_exit_code(self) -> None:
        outcome = builder.infrastructure_command_outcome(
            index=1,
            label="Construir Debug",
            exit_code=7,
            duration_seconds=0.25,
            cancelled=False,
            evidence="full.log",
            captured_output="ninja: build stopped: subcommand failed.\n",
        )

        self.assertEqual(outcome.detail, "exit code 7")

    def test_active_cancelled_command_is_not_reported_as_failure(self) -> None:
        outcome = builder.infrastructure_command_outcome(
            index=1,
            label="Configurar Debug",
            exit_code=130,
            duration_seconds=0.25,
            cancelled=True,
            evidence="full.log",
        )

        self.assertEqual(outcome.status, "CANCELLED")
        self.assertEqual(outcome.key, "INFRA|1")
        self.assertIn("cancel", outcome.detail)

    def test_unstarted_commands_receive_terminal_cancelled_rows(self) -> None:
        outcomes = builder.cancelled_infrastructure_outcomes(
            COMMANDS, first_unstarted_index=2, evidence="full.log"
        )

        self.assertEqual([item.key for item in outcomes], ["INFRA|2", "INFRA|3"])
        self.assertEqual(
            [item.case_id for item in outcomes],
            ["Construir Debug", "Configurar Release"],
        )
        self.assertTrue(all(item.status == "CANCELLED" for item in outcomes))

    def test_cancel_before_first_command_closes_entire_plan(self) -> None:
        outcomes = builder.cancelled_infrastructure_outcomes(
            COMMANDS, first_unstarted_index=1, evidence="full.log"
        )

        self.assertEqual(len(outcomes), len(COMMANDS))
        self.assertEqual([item.key for item in outcomes], ["INFRA|1", "INFRA|2", "INFRA|3"])


if __name__ == "__main__":
    unittest.main()
