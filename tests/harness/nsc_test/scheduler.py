"""Deterministic bounded scheduling for isolated NSC test cases."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from typing import Callable, Generic, TypeVar


ResultT = TypeVar("ResultT")


@dataclass(frozen=True)
class ScheduledItem(Generic[ResultT]):
    identifier: str
    exclusive: bool
    action: Callable[[], ResultT]


class CaseScheduler(Generic[ResultT]):
    """Runs parallel batches separated by exclusive process barriers."""

    def __init__(self, jobs: int) -> None:
        if jobs < 1:
            raise ValueError("jobs must be at least one")
        self.jobs = jobs

    def run(self, items: tuple[ScheduledItem[ResultT], ...]) -> tuple[ResultT, ...]:
        identifiers: set[str] = set()
        for item in items:
            if item.identifier in identifiers:
                raise ValueError(f"duplicate scheduled item: {item.identifier}")
            identifiers.add(item.identifier)

        results: list[ResultT] = []
        batch: list[ScheduledItem[ResultT]] = []
        for item in items:
            if item.exclusive:
                results.extend(self._parallel_batch(batch))
                batch.clear()
                results.append(item.action())
            else:
                batch.append(item)
        results.extend(self._parallel_batch(batch))
        return tuple(results)

    def _parallel_batch(self, items: list[ScheduledItem[ResultT]]) -> list[ResultT]:
        if not items:
            return []
        if self.jobs == 1 or len(items) == 1:
            return [item.action() for item in items]
        with ThreadPoolExecutor(max_workers=min(self.jobs, len(items))) as executor:
            futures = [executor.submit(item.action) for item in items]
            return [future.result() for future in futures]
