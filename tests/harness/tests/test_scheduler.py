from __future__ import annotations

import sys
import threading
import time
import unittest
from pathlib import Path


HARNESS_ROOT = Path(__file__).resolve().parents[1]
if str(HARNESS_ROOT) not in sys.path:
    sys.path.insert(0, str(HARNESS_ROOT))

from nsc_test.scheduler import CaseScheduler, ScheduledItem  # noqa: E402


class SchedulerTests(unittest.TestCase):
    def test_parallel_batches_preserve_result_order_and_exclusive_barriers(self) -> None:
        lock = threading.Lock()
        active = 0
        maximum = 0
        exclusive_observations: list[int] = []

        def action(name: str, exclusive: bool = False) -> str:
            nonlocal active, maximum
            with lock:
                active += 1
                maximum = max(maximum, active)
                if exclusive:
                    exclusive_observations.append(active)
            time.sleep(0.04)
            with lock:
                active -= 1
            return name

        items = (
            ScheduledItem("a", False, lambda: action("a")),
            ScheduledItem("b", False, lambda: action("b")),
            ScheduledItem("x", True, lambda: action("x", exclusive=True)),
            ScheduledItem("c", False, lambda: action("c")),
            ScheduledItem("d", False, lambda: action("d")),
        )
        results = CaseScheduler[str](jobs=4).run(items)
        self.assertEqual(results, ("a", "b", "x", "c", "d"))
        self.assertGreater(maximum, 1)
        self.assertEqual(exclusive_observations, [1])

    def test_jobs_one_is_strictly_sequential(self) -> None:
        observed: list[str] = []
        items = tuple(
            ScheduledItem(name, False, lambda value=name: observed.append(value) or value)
            for name in ("a", "b", "c")
        )
        self.assertEqual(CaseScheduler[str](1).run(items), ("a", "b", "c"))
        self.assertEqual(observed, ["a", "b", "c"])

    def test_duplicate_identifier_is_rejected(self) -> None:
        items = (
            ScheduledItem("same", False, lambda: 1),
            ScheduledItem("same", False, lambda: 2),
        )
        with self.assertRaisesRegex(ValueError, "duplicate scheduled item"):
            CaseScheduler[int](2).run(items)


if __name__ == "__main__":
    unittest.main()
