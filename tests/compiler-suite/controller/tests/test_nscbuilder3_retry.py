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


if __name__ == "__main__":
    unittest.main()
