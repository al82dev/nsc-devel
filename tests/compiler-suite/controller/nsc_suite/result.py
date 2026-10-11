"""Causal result states for the new NativeSharp compiler suite."""

from __future__ import annotations

import math
from dataclasses import dataclass
from enum import Enum
from typing import Iterable

from .manifest import CASE_ID, CaseManifest


class CaseStatus(str, Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    KNOWN_FAILURE = "KNOWN_FAILURE"
    UNEXPECTED_PASS = "UNEXPECTED_PASS"
    BLOCKED = "BLOCKED"
    SKIPPED = "SKIPPED"
    CANCELLED = "CANCELLED"
    TIMEOUT = "TIMEOUT"
    INFRASTRUCTURE_ERROR = "INFRASTRUCTURE_ERROR"


@dataclass(frozen=True)
class CaseResult:
    case_id: str
    status: CaseStatus
    duration_seconds: float = 0.0
    failures: tuple[str, ...] = ()
    blocked_by: tuple[str, ...] = ()

    def __post_init__(self) -> None:
        if not isinstance(self.case_id, str) or not self.case_id:
            raise ValueError("case result requires a case_id")
        if not isinstance(self.status, CaseStatus):
            raise ValueError("case result requires a CaseStatus")
        if (
            type(self.duration_seconds) not in {int, float}
            or not math.isfinite(self.duration_seconds)
            or self.duration_seconds < 0
        ):
            raise ValueError("duration_seconds must be a finite nonnegative number")
        if not isinstance(self.failures, tuple) or any(
            not isinstance(failure, str) or not failure for failure in self.failures
        ):
            raise ValueError("failures must be a tuple of non-empty strings")
        if not isinstance(self.blocked_by, tuple) or any(
            not isinstance(producer, str) or not producer for producer in self.blocked_by
        ):
            raise ValueError("blocked_by must be a tuple of non-empty strings")
        if len(self.blocked_by) != len(set(self.blocked_by)):
            raise ValueError("blocked_by cannot contain duplicate producers")
        if self.status is CaseStatus.PASS and (self.failures or self.blocked_by):
            raise ValueError("PASS cannot contain failures or blockers")
        if self.status is CaseStatus.BLOCKED and not self.blocked_by:
            raise ValueError("BLOCKED requires at least one producer")
        if self.status is not CaseStatus.BLOCKED and self.blocked_by:
            raise ValueError("only BLOCKED can contain blocked_by")
        if self.status in {
            CaseStatus.FAIL,
            CaseStatus.KNOWN_FAILURE,
            CaseStatus.UNEXPECTED_PASS,
            CaseStatus.TIMEOUT,
            CaseStatus.INFRASTRUCTURE_ERROR,
        } and not self.failures:
            raise ValueError(f"{self.status.value} requires failure evidence")


@dataclass(frozen=True)
class DependencyDecision:
    runnable: bool
    waiting_for: tuple[str, ...] = ()
    blocked_by: tuple[str, ...] = ()


class ResultLedger:
    """Append-only result index that distinguishes waiting from causal blocking."""

    def __init__(self, cases: Iterable[CaseManifest]) -> None:
        ordered = tuple(cases)
        self._cases = {case.case_id: case for case in ordered}
        if len(self._cases) != len(ordered):
            raise ValueError("case collection contains duplicate IDs")
        self._results: dict[str, CaseResult] = {}

    @property
    def results(self) -> tuple[CaseResult, ...]:
        return tuple(self._results[case_id] for case_id in sorted(self._results))

    def record(self, result: CaseResult) -> None:
        if result.case_id not in self._cases:
            raise ValueError(f"result references unknown case: {result.case_id}")
        if result.case_id in self._results:
            raise ValueError(f"case already has a final result: {result.case_id}")
        self._results[result.case_id] = result

    def dependency_decision(self, case_id: str) -> DependencyDecision:
        case = self._cases.get(case_id)
        if case is None:
            raise ValueError(f"unknown case: {case_id}")
        waiting: list[str] = []
        blocked: list[str] = []
        for dependency in sorted(
            dependency
            for dependency in case.dependencies
            if CASE_ID.fullmatch(dependency) is not None
        ):
            result = self._results.get(dependency)
            if result is None:
                waiting.append(dependency)
            elif result.status is not CaseStatus.PASS:
                blocked.append(dependency)
        return DependencyDecision(
            runnable=not waiting and not blocked,
            waiting_for=tuple(waiting),
            blocked_by=tuple(blocked),
        )

    def record_blocked_if_decided(self, case_id: str) -> CaseResult | None:
        decision = self.dependency_decision(case_id)
        if decision.waiting_for or not decision.blocked_by:
            return None
        result = CaseResult(
            case_id=case_id,
            status=CaseStatus.BLOCKED,
            blocked_by=decision.blocked_by,
        )
        self.record(result)
        return result
