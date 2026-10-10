from __future__ import annotations

import sys
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = CONTROLLER_ROOT.parents[2]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite import (
    CaseResult,
    CaseStatus,
    InventoryIndex,
    ResultLedger,
    Selection,
    SelectionError,
    load_manifest,
    select_cases,
)


class LiveManifestClosureTests(unittest.TestCase):
    def test_every_live_component_selection_reaches_causal_closure(self) -> None:
        inventory_root = (
            REPOSITORY_ROOT
            / "docs"
            / "compiler-testing-reengineering"
            / "inventory"
        )
        inventory = InventoryIndex.from_csv(
            inventory_root / "components.csv",
            inventory_root / "contracts.csv",
        )
        cases = load_manifest(
            REPOSITORY_ROOT
            / "tests"
            / "compiler-suite"
            / "manifest"
            / "cases.jsonl",
            inventory,
        )
        components = sorted({case.component for case in cases})
        completed_selections = 0
        for build_mode in ("Debug", "Release"):
            for profile in ("PR", "NIGHTLY", "WEEKLY"):
                for component in components:
                    try:
                        selected = select_cases(
                            cases,
                            Selection(
                                components=frozenset({component}),
                                profile=profile,
                                build_mode=build_mode,
                            ),
                        )
                    except SelectionError as error:
                        self.assertEqual(str(error), "selection matched no cases")
                        continue
                    ledger = ResultLedger(selected)
                    pending = {case.case_id for case in selected}
                    while pending:
                        progressed = False
                        for case_id in sorted(tuple(pending)):
                            decision = ledger.dependency_decision(case_id)
                            if decision.waiting_for:
                                continue
                            ledger.record(CaseResult(case_id, CaseStatus.PASS))
                            pending.remove(case_id)
                            progressed = True
                        self.assertTrue(
                            progressed,
                            msg=(build_mode, profile, component, sorted(pending)),
                        )
                    completed_selections += 1
        self.assertGreater(completed_selections, 0)


if __name__ == "__main__":
    unittest.main()
