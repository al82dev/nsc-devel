from __future__ import annotations

import sys
import unittest
from collections import Counter
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_ROOT = CONTROLLER_ROOT / "integration"
sys.path.insert(0, str(CONTROLLER_ROOT))
sys.path.insert(0, str(INTEGRATION_ROOT))

from run_infrastructure_gate import fixture_cases, validate_events, validate_results


class InfrastructureGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.fixture = CONTROLLER_ROOT.parent / "fixtures" / "infrastructure"
        cls.cases = fixture_cases(cls.fixture)

    def test_live_fixture_drives_results_and_event_protocol(self) -> None:
        results = [
            {"case_id": case.case_id, "status": case.expected_outcome}
            for case in self.cases
        ]
        counts = Counter(case.expected_outcome for case in self.cases)
        validate_results(self.cases, {"results": results, "counts": counts})

        events = [
            {
                "sequence": 0,
                "kind": "RUN_STARTED",
                "case_id": None,
                "payload": {"selected": len(self.cases)},
            }
        ]
        for case in self.cases:
            events.append(
                {
                    "sequence": len(events),
                    "kind": "CASE_STARTED",
                    "case_id": case.case_id,
                    "payload": {},
                }
            )
            events.append(
                {
                    "sequence": len(events),
                    "kind": "CASE_FINISHED",
                    "case_id": case.case_id,
                    "payload": {"status": case.expected_outcome},
                }
            )
        events.append(
            {
                "sequence": len(events),
                "kind": "RUN_FINISHED",
                "case_id": None,
                "payload": {
                    "selected": len(self.cases),
                    "completed": len(self.cases),
                    "cancelled": False,
                },
            }
        )
        validate_events(self.cases, events)

    def test_missing_result_and_event_are_rejected(self) -> None:
        results = [
            {"case_id": case.case_id, "status": case.expected_outcome}
            for case in self.cases[:-1]
        ]
        with self.assertRaisesRegex(RuntimeError, "live fixture selection"):
            validate_results(
                self.cases,
                {"results": results, "counts": {"PASS": len(results)}},
            )
        with self.assertRaisesRegex(RuntimeError, "protocol cardinality"):
            validate_events(self.cases, [])

    def test_result_validation_rejects_coercible_counts(self) -> None:
        results = [
            {"case_id": case.case_id, "status": case.expected_outcome}
            for case in self.cases
        ]
        counts = Counter(case.expected_outcome for case in self.cases)
        status = next(iter(counts))
        counts[status] = str(counts[status])
        with self.assertRaisesRegex(RuntimeError, "invalid result count"):
            validate_results(self.cases, {"results": results, "counts": counts})


if __name__ == "__main__":
    unittest.main()
