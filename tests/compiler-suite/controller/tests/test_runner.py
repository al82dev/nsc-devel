from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite.events import CancellationToken, EventWriter
from nsc_suite.manifest import CaseManifest
from nsc_suite.result import CaseResult, CaseStatus
from nsc_suite.runner import SuiteRunner
from nsc_suite.workspace import WorkspaceManager


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


class RunnerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(
            prefix="nsc-suite-runner-", dir=Path.cwd()
        )
        self.root = Path(self.temporary.name)
        self.events_path = self.root / "run-001" / "events.jsonl"

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def run_cases(self, cases, executor, token=None):
        cancellation = token or CancellationToken()
        with EventWriter(self.events_path) as events:
            runner = SuiteRunner(WorkspaceManager(self.root), events, cancellation)
            results = runner.run("run-001", cases, executor)
        event_rows = [
            json.loads(line)
            for line in self.events_path.read_text(encoding="utf-8").splitlines()
        ]
        return results, event_rows

    def test_executes_producers_before_consumers(self) -> None:
        execution_order: list[str] = []

        def executor(item, workspace):
            execution_order.append(item.case_id)
            self.assertTrue(workspace.metadata.is_file())
            return CaseResult(item.case_id, CaseStatus.PASS)

        results, events = self.run_cases(
            (case("TB-BAS-002", ("TB-BAS-001",)), case("TB-BAS-001")), executor
        )
        self.assertEqual(execution_order, ["TB-BAS-001", "TB-BAS-002"])
        self.assertTrue(all(result.status is CaseStatus.PASS for result in results))
        self.assertEqual(events[0]["kind"], "RUN_STARTED")
        self.assertEqual(events[-1]["kind"], "RUN_FINISHED")

    def test_failed_producer_blocks_consumer_without_execution(self) -> None:
        executed: list[str] = []

        def executor(item, workspace):
            executed.append(item.case_id)
            return CaseResult(item.case_id, CaseStatus.FAIL, failures=("synthetic",))

        results, _ = self.run_cases(
            (case("TB-BAS-001"), case("TB-BAS-002", ("TB-BAS-001",))), executor
        )
        self.assertEqual(executed, ["TB-BAS-001"])
        self.assertEqual(results[1].status, CaseStatus.BLOCKED)
        self.assertEqual(results[1].blocked_by, ("TB-BAS-001",))
        self.assertFalse((self.root / "run-001" / "TB-BAS-002").exists())

    def test_executor_exception_is_infrastructure_error(self) -> None:
        def executor(item, workspace):
            raise RuntimeError("synthetic crash")

        results, _ = self.run_cases((case("TB-BAS-001"),), executor)
        self.assertEqual(results[0].status, CaseStatus.INFRASTRUCTURE_ERROR)
        self.assertIn("synthetic crash", results[0].failures[0])

    def test_cancellation_finishes_every_selected_case(self) -> None:
        token = CancellationToken()

        def executor(item, workspace):
            token.request("synthetic cancellation")
            return CaseResult(item.case_id, CaseStatus.PASS)

        results, events = self.run_cases(
            (case("TB-BAS-001"), case("TB-BAS-002"), case("TB-BAS-003")),
            executor,
            token,
        )
        self.assertEqual(results[0].status, CaseStatus.PASS)
        self.assertEqual(results[1].status, CaseStatus.CANCELLED)
        self.assertEqual(results[2].status, CaseStatus.CANCELLED)
        self.assertEqual(sum(event["kind"] == "RUN_CANCELLED" for event in events), 1)
        self.assertEqual(len(results), 3)


if __name__ == "__main__":
    unittest.main()
