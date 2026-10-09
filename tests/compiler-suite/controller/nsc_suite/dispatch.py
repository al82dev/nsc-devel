"""Exact executor dispatch without fallback between suite authorities."""

from __future__ import annotations

from collections.abc import Callable, Mapping

from .manifest import CaseManifest
from .result import CaseResult, CaseStatus
from .workspace import CaseWorkspace


Executor = Callable[[CaseManifest, CaseWorkspace], CaseResult]


class ExecutorRouter:
    def __init__(self, executors: Mapping[str, Executor]) -> None:
        self.executors = dict(executors)

    def __call__(self, case: CaseManifest, workspace: CaseWorkspace) -> CaseResult:
        executor = self.executors.get(case.executor)
        if executor is None:
            return CaseResult(
                case.case_id,
                CaseStatus.INFRASTRUCTURE_ERROR,
                failures=(f"no executor configured for {case.executor}",),
            )
        return executor(case, workspace)
