from __future__ import annotations

import sys
import unittest
from collections import Counter
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT))

import nscbuilder3 as builder


class Builder3EvidenceTests(unittest.TestCase):
    def test_known_result_sets_map_to_exact_global_status(self) -> None:
        self.assertEqual(
            builder.EvidenceStore._overall_status(Counter({"PASS": 4})),
            "PASSED",
        )
        self.assertEqual(
            builder.EvidenceStore._overall_status(
                Counter({"PASS": 4, "KNOWN_FAILURE": 2})
            ),
            "COMPLETED_WITH_KNOWN_DEFECTS",
        )
        for status in sorted(builder.TERMINAL_FAILURES):
            with self.subTest(status=status):
                self.assertEqual(
                    builder.EvidenceStore._overall_status(Counter({status: 1})),
                    "FAILED",
                )

    def test_empty_result_set_cannot_be_published_as_passed(self) -> None:
        with self.assertRaisesRegex(ValueError, "no case results"):
            builder.EvidenceStore._overall_status(Counter())

    def test_unknown_result_state_cannot_be_published_as_passed(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown result states"):
            builder.EvidenceStore._overall_status(Counter({"PASS": 1, "MYSTERY": 1}))

    def test_cancellation_materializes_every_unstarted_variant(self) -> None:
        catalog = builder.CanonicalCatalog.load()
        cases = tuple(case for case in catalog.cases if case.executable)[:3]
        variants = tuple(
            builder.RunVariant(case, case.build_modes[0], case.optimizations[0])
            for case in cases
        )

        outcomes = builder.ExecutionEngine._cancelled_outcomes(variants)

        self.assertEqual([outcome.key for outcome in outcomes], [item.key for item in variants])
        self.assertTrue(all(outcome.status == "CANCELLED" for outcome in outcomes))
        self.assertTrue(all(outcome.detail for outcome in outcomes))


if __name__ == "__main__":
    unittest.main()
