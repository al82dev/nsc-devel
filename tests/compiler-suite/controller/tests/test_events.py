from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite.events import CancellationToken, EventKind, EventWriter


class EventTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(
            prefix="nsc-suite-events-", dir=Path.cwd()
        )
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def read_events(self, path: Path) -> list[dict]:
        return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]

    def test_each_event_is_immediately_readable_and_sequenced(self) -> None:
        path = self.root / "events.jsonl"
        with EventWriter(path) as writer:
            writer.emit(EventKind.RUN_STARTED, payload={"selected": 1})
            self.assertEqual(len(self.read_events(path)), 1)
            writer.emit(EventKind.CASE_STARTED, case_id="TB-BAS-001")
            self.assertEqual(len(self.read_events(path)), 2)
            writer.emit(
                EventKind.CASE_FINISHED,
                case_id="TB-BAS-001",
                payload={"status": "PASS"},
            )
        events = self.read_events(path)
        self.assertEqual([event["sequence"] for event in events], [0, 1, 2])
        self.assertEqual(events[-1]["payload"]["status"], "PASS")

    def test_refuses_to_overwrite_or_append_to_existing_stream(self) -> None:
        path = self.root / "events.jsonl"
        path.write_text("preserve\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "already exists"):
            EventWriter(path)
        self.assertEqual(path.read_text(encoding="utf-8"), "preserve\n")

    def test_closed_writer_rejects_events(self) -> None:
        writer = EventWriter(self.root / "events.jsonl")
        writer.close()
        with self.assertRaisesRegex(ValueError, "closed"):
            writer.emit(EventKind.RUN_FINISHED)

    def test_unserializable_payload_does_not_write_partial_line(self) -> None:
        path = self.root / "events.jsonl"
        with EventWriter(path) as writer:
            with self.assertRaises(TypeError):
                writer.emit(EventKind.INFRASTRUCTURE_ERROR, payload={"bad": object()})
        self.assertEqual(path.read_text(encoding="utf-8"), "")

    def test_cancellation_preserves_first_reason(self) -> None:
        token = CancellationToken()
        self.assertFalse(token.requested)
        self.assertTrue(token.request("user request"))
        self.assertFalse(token.request("later timeout"))
        self.assertTrue(token.requested)
        self.assertEqual(token.reason, "user request")
        for reason in ("", "   ", 7, True, None):
            invalid = CancellationToken()
            with self.subTest(reason=reason), self.assertRaisesRegex(
                ValueError, "non-empty string"
            ):
                invalid.request(reason)
            self.assertFalse(invalid.requested)
            self.assertIsNone(invalid.reason)


if __name__ == "__main__":
    unittest.main()
