from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


def case(number: int) -> builder.CaseRecord:
    return builder.CaseRecord(
        case_id=f"TI-RETRY-{number:03d}",
        contract_id="CI-RETRY-001",
        component="Infrastructure",
        wave=0,
        purpose="synthetic retry projection",
        layer="unit",
        profile="PR",
        build_modes=("Debug",),
        optimizations=("NA",),
        executor="nsc_component_tests",
        disposition="Executable",
        objective="project one terminal status",
        expected_outcome="PASS",
        manifest={"timeout_class": "tiny"},
    )


def result(variant: builder.RunVariant, status: str) -> builder.CaseOutcome:
    return builder.CaseOutcome(
        key=variant.key,
        case_id=variant.case.case_id,
        contract_id=variant.case.contract_id,
        component=variant.case.component,
        wave=variant.case.wave,
        build_mode=variant.build_mode,
        optimization=variant.optimization,
        status=status,
    )


class Builder3RetryTests(unittest.TestCase):
    def setUp(self) -> None:
        self.visible = tuple(
            builder.RunVariant(case(index), "Debug", "NA") for index in range(1, 6)
        )

    def test_retry_contains_only_terminal_failures_in_original_plan_order(self) -> None:
        outcomes = (
            result(self.visible[0], "PASS"),
            result(self.visible[1], "FAIL"),
            result(self.visible[2], "KNOWN_FAILURE"),
            result(self.visible[3], "BLOCKED"),
            result(self.visible[4], "INFRASTRUCTURE_ERROR"),
        )

        projected = builder.retry_variants(self.visible, tuple(reversed(outcomes)))

        self.assertEqual(projected, (self.visible[1], self.visible[3], self.visible[4]))

    def test_terminal_result_outside_visible_plan_is_rejected(self) -> None:
        foreign = builder.RunVariant(case(99), "Debug", "NA")
        with self.assertRaisesRegex(ValueError, "outside the visible plan"):
            builder.retry_variants(self.visible, (result(foreign, "FAIL"),))

    def test_duplicate_outcome_key_is_rejected(self) -> None:
        duplicate = result(self.visible[0], "FAIL")
        with self.assertRaisesRegex(ValueError, "duplicate outcome key"):
            builder.retry_variants(self.visible, (duplicate, duplicate))

    def test_duplicate_visible_variant_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "duplicate variant keys"):
            builder.retry_variants(
                (self.visible[0], self.visible[0]),
                (result(self.visible[0], "FAIL"),),
            )

    def test_active_retry_excludes_only_exact_disabled_variants(self) -> None:
        failed = (self.visible[1], self.visible[3], self.visible[4])

        active = builder.active_retry_variants(failed, {self.visible[3].key})

        self.assertEqual(active, (self.visible[1], self.visible[4]))

    def test_disabled_variant_outside_failed_plan_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "outside the failed plan"):
            builder.active_retry_variants(
                (self.visible[1],), {self.visible[2].key}
            )

    def test_active_retry_rejects_duplicate_failed_keys(self) -> None:
        with self.assertRaisesRegex(ValueError, "duplicate variant keys"):
            builder.active_retry_variants(
                (self.visible[1], self.visible[1]), set()
            )

    def test_row_progress_identifies_ready_active_and_terminal_states(self) -> None:
        self.assertEqual(builder.row_progress_text("LISTO"), "0%")
        self.assertEqual(builder.row_progress_text("RUNNING"), "EN CURSO")
        self.assertEqual(builder.row_progress_text("COMPILING"), "EN CURSO")
        self.assertEqual(builder.row_progress_text("PASS"), "100%")
        self.assertEqual(builder.row_progress_text("FAIL"), "100%")

    def test_row_progress_keeps_nonexecuted_rows_explicit(self) -> None:
        self.assertEqual(builder.row_progress_text("PENDIENTE"), "NO EJECUTABLE")
        self.assertEqual(builder.row_progress_text("FILTRADO"), "FUERA")

    def test_row_progress_does_not_invent_unknown_state(self) -> None:
        self.assertEqual(builder.row_progress_text("UNKNOWN"), "")

    def test_row_action_exposes_only_exact_executable_variants(self) -> None:
        self.assertEqual(builder.row_action_text("LISTO", True), "Ejecutar")
        self.assertEqual(builder.row_action_text("PASS", True), "Volver a correr")
        self.assertEqual(builder.row_action_text("FAIL", True), "Volver a correr")
        self.assertEqual(builder.row_action_text("PENDIENTE", False), "")

    def test_row_action_marks_active_variant_without_offering_a_second_run(self) -> None:
        self.assertEqual(builder.row_action_text("RUNNING", True), "En ejecución")
        self.assertEqual(builder.row_action_text("COMPILING", True), "En ejecución")


if __name__ == "__main__":
    unittest.main()
