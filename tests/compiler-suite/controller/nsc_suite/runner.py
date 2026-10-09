"""Iterative, causal execution coordinator for selected compiler-suite cases."""

from __future__ import annotations

from collections.abc import Callable, Iterable

from .events import CancellationToken, EventKind, EventWriter
from .manifest import CaseManifest
from .result import CaseResult, CaseStatus, ResultLedger
from .workspace import CaseWorkspace, WorkspaceManager


CaseExecutor = Callable[[CaseManifest, CaseWorkspace], CaseResult]


class RunnerError(RuntimeError):
    """Raised when the selected graph cannot produce a complete causal result set."""


class SuiteRunner:
    def __init__(
        self,
        workspace_manager: WorkspaceManager,
        event_writer: EventWriter,
        cancellation: CancellationToken,
    ) -> None:
        self.workspace_manager = workspace_manager
        self.event_writer = event_writer
        self.cancellation = cancellation

    def run(
        self,
        run_id: str,
        cases: Iterable[CaseManifest],
        executor: CaseExecutor,
    ) -> tuple[CaseResult, ...]:
        ordered = tuple(sorted(cases, key=lambda case: case.case_id))
        if not ordered:
            raise RunnerError("runner requires at least one selected case")
        ledger = ResultLedger(ordered)
        pending = {case.case_id: case for case in ordered}
        self.event_writer.emit(EventKind.RUN_STARTED, payload={"selected": len(ordered)})

        while pending:
            if self.cancellation.requested:
                self._cancel_pending(pending, ledger)
                self.event_writer.emit(
                    EventKind.RUN_CANCELLED,
                    payload={"reason": self.cancellation.reason},
                )
                break

            progressed = False
            for case_id in sorted(tuple(pending)):
                case = pending[case_id]
                decision = ledger.dependency_decision(case_id)
                if decision.waiting_for:
                    continue
                if decision.blocked_by:
                    blocked = ledger.record_blocked_if_decided(case_id)
                    if blocked is None:
                        raise RunnerError(f"failed to materialize blocker for {case_id}")
                    self._emit_finished(blocked)
                else:
                    result = self._execute(run_id, case, executor)
                    ledger.record(result)
                    self._emit_finished(result)
                del pending[case_id]
                progressed = True
                if self.cancellation.requested:
                    break
            if not progressed and not self.cancellation.requested:
                unresolved = ", ".join(sorted(pending))
                raise RunnerError(f"selected graph made no progress: {unresolved}")

        results = ledger.results
        self.event_writer.emit(
            EventKind.RUN_FINISHED,
            payload={
                "selected": len(ordered),
                "completed": len(results),
                "cancelled": self.cancellation.requested,
            },
        )
        return results

    def _execute(
        self,
        run_id: str,
        case: CaseManifest,
        executor: CaseExecutor,
    ) -> CaseResult:
        workspace = self.workspace_manager.prepare(run_id, case.case_id)
        self.event_writer.emit(EventKind.CASE_STARTED, case_id=case.case_id)
        try:
            result = executor(case, workspace)
        except Exception as error:
            return CaseResult(
                case.case_id,
                CaseStatus.INFRASTRUCTURE_ERROR,
                failures=(f"executor raised {type(error).__name__}: {error}",),
            )
        if result.case_id != case.case_id:
            return CaseResult(
                case.case_id,
                CaseStatus.INFRASTRUCTURE_ERROR,
                failures=(f"executor returned result for {result.case_id}",),
            )
        return result

    def _cancel_pending(
        self,
        pending: dict[str, CaseManifest],
        ledger: ResultLedger,
    ) -> None:
        reason = self.cancellation.reason or "cancellation requested"
        for case_id in sorted(pending):
            result = CaseResult(
                case_id,
                CaseStatus.CANCELLED,
                failures=(reason,),
            )
            ledger.record(result)
            self._emit_finished(result)
        pending.clear()

    def _emit_finished(self, result: CaseResult) -> None:
        self.event_writer.emit(
            EventKind.CASE_FINISHED,
            case_id=result.case_id,
            payload={
                "status": result.status.value,
                "failures": list(result.failures),
                "blocked_by": list(result.blocked_by),
            },
        )
