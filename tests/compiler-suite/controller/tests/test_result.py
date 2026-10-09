from __future__ import annotations

import sys
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite.manifest import CaseManifest
from nsc_suite.result import CaseResult, CaseStatus, ResultLedger


def case(case_id: str, dependencies: tuple[str, ...] = ()) -> CaseManifest:
    return CaseManifest(
        case_id=case_id,
        contract_id="CB-BAS-001",
        component="Host.Basic",
        case_class="nominal",
        executor="nsc_component_tests",
        layer="unit",
        inputs={},
        oracle={"value": 1},
        dependencies=dependencies,
        profile="PR",
        build_modes=("Debug",),
        optimizations=("NA",),
        isolation="in-process-reset",
        timeout_class="tiny",
        tags=(),
    )


class ResultTests(unittest.TestCase):
    def test_result_invariants_reject_ambiguous_states(self) -> None:
        with self.assertRaisesRegex(ValueError, "PASS cannot"):
            CaseResult("TB-BAS-001", CaseStatus.PASS, failures=("noise",))
        with self.assertRaisesRegex(ValueError, "requires at least one producer"):
            CaseResult("TB-BAS-001", CaseStatus.BLOCKED)
        with self.assertRaisesRegex(ValueError, "requires failure evidence"):
            CaseResult("TB-BAS-001", CaseStatus.FAIL)

    def test_ledger_is_append_only(self) -> None:
        ledger = ResultLedger((case("TB-BAS-001"),))
        result = CaseResult("TB-BAS-001", CaseStatus.PASS)
        ledger.record(result)
        with self.assertRaisesRegex(ValueError, "already has a final result"):
            ledger.record(result)

    def test_consumer_waits_until_all_producers_finish(self) -> None:
        ledger = ResultLedger(
            (
                case("TB-BAS-001"),
                case("TB-BAS-002"),
                case("TB-BAS-003", ("TB-BAS-001", "TB-BAS-002")),
            )
        )
        initial = ledger.dependency_decision("TB-BAS-003")
        self.assertFalse(initial.runnable)
        self.assertEqual(initial.waiting_for, ("TB-BAS-001", "TB-BAS-002"))
        ledger.record(CaseResult("TB-BAS-001", CaseStatus.PASS))
        partial = ledger.dependency_decision("TB-BAS-003")
        self.assertEqual(partial.waiting_for, ("TB-BAS-002",))
        ledger.record(CaseResult("TB-BAS-002", CaseStatus.PASS))
        self.assertTrue(ledger.dependency_decision("TB-BAS-003").runnable)

    def test_failed_producer_blocks_consumer_by_identity(self) -> None:
        ledger = ResultLedger(
            (case("TB-BAS-001"), case("TB-BAS-002", ("TB-BAS-001",)))
        )
        ledger.record(
            CaseResult("TB-BAS-001", CaseStatus.FAIL, failures=("oracle mismatch",))
        )
        blocked = ledger.record_blocked_if_decided("TB-BAS-002")
        self.assertIsNotNone(blocked)
        self.assertEqual(blocked.blocked_by, ("TB-BAS-001",))
        self.assertEqual(ledger.results[-1].status, CaseStatus.BLOCKED)

    def test_waiting_consumer_is_not_prematurely_blocked(self) -> None:
        ledger = ResultLedger(
            (
                case("TB-BAS-001"),
                case("TB-BAS-002"),
                case("TB-BAS-003", ("TB-BAS-001", "TB-BAS-002")),
            )
        )
        ledger.record(
            CaseResult("TB-BAS-001", CaseStatus.FAIL, failures=("producer failed",))
        )
        self.assertIsNone(ledger.record_blocked_if_decided("TB-BAS-003"))
        ledger.record(CaseResult("TB-BAS-002", CaseStatus.PASS))
        blocked = ledger.record_blocked_if_decided("TB-BAS-003")
        self.assertEqual(blocked.blocked_by, ("TB-BAS-001",))


if __name__ == "__main__":
    unittest.main()
