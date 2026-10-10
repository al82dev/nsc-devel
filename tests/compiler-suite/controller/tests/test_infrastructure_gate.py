from __future__ import annotations

import sys
import unittest
from collections import Counter
from pathlib import Path
from types import SimpleNamespace


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

    def test_event_validation_rejects_coercible_protocol_scalars(self) -> None:
        cases = (SimpleNamespace(case_id="7", expected_outcome="PASS"),)
        events = [
            {"sequence": 0, "kind": "RUN_STARTED", "case_id": None, "payload": {"selected": 1}},
            {"sequence": 1, "kind": "CASE_STARTED", "case_id": "7", "payload": {}},
            {
                "sequence": 2,
                "kind": "CASE_FINISHED",
                "case_id": "7",
                "payload": {"status": "PASS"},
            },
            {
                "sequence": 3,
                "kind": "RUN_FINISHED",
                "case_id": None,
                "payload": {"selected": 1, "completed": 1, "cancelled": False},
            },
        ]
        bad_sequence = [dict(event) for event in events]
        bad_sequence[0]["sequence"] = False
        bad_sequence[1]["sequence"] = True
        with self.assertRaisesRegex(RuntimeError, "exact integers"):
            validate_events(cases, bad_sequence)

        bad_identity = [dict(event) for event in events]
        bad_identity[1]["case_id"] = 7
        bad_identity[2]["case_id"] = 7
        with self.assertRaisesRegex(RuntimeError, "invalid case identity"):
            validate_events(cases, bad_identity)

        bad_count = [dict(event) for event in events]
        bad_count[0] = dict(events[0], payload={"selected": 1.0})
        with self.assertRaisesRegex(RuntimeError, "selected count"):
            validate_events(cases, bad_count)

    def test_event_validation_rejects_contradictory_case_status(self) -> None:
        case = SimpleNamespace(case_id="7", expected_outcome="PASS")
        events = [
            {"sequence": 0, "kind": "RUN_STARTED", "case_id": None, "payload": {"selected": 1}},
            {"sequence": 1, "kind": "CASE_STARTED", "case_id": "7", "payload": {}},
            {
                "sequence": 2,
                "kind": "CASE_FINISHED",
                "case_id": "7",
                "payload": {"status": "FAIL"},
            },
            {
                "sequence": 3,
                "kind": "RUN_FINISHED",
                "case_id": None,
                "payload": {"selected": 1, "completed": 1, "cancelled": False},
            },
        ]
        with self.assertRaisesRegex(RuntimeError, "declared outcome"):
            validate_events((case,), events)

        events[2]["payload"] = {"status": 1}
        with self.assertRaisesRegex(RuntimeError, "invalid status"):
            validate_events((case,), events)


if __name__ == "__main__":
    unittest.main()
