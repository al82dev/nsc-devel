"""Crash-surviving JSONL events and cooperative cancellation."""

from __future__ import annotations

import json
import os
import threading
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Any


class EventKind(str, Enum):
    RUN_STARTED = "RUN_STARTED"
    CASE_STARTED = "CASE_STARTED"
    CASE_FINISHED = "CASE_FINISHED"
    RUN_CANCELLED = "RUN_CANCELLED"
    RUN_FINISHED = "RUN_FINISHED"
    INFRASTRUCTURE_ERROR = "INFRASTRUCTURE_ERROR"


@dataclass(frozen=True)
class SuiteEvent:
    sequence: int
    kind: EventKind
    case_id: str | None
    payload: dict[str, Any]


class EventWriter:
    """Write one complete, flushed, fsynced JSON object per event."""

    def __init__(self, path: Path) -> None:
        self.path = path.resolve()
        if self.path.exists():
            raise ValueError(f"event stream already exists: {self.path}")
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._stream = self.path.open("x", encoding="utf-8", newline="\n")
        self._sequence = 0
        self._closed = False

    def emit(
        self,
        kind: EventKind,
        *,
        case_id: str | None = None,
        payload: dict[str, Any] | None = None,
    ) -> SuiteEvent:
        if self._closed:
            raise ValueError("event stream is closed")
        event = SuiteEvent(self._sequence, kind, case_id, payload or {})
        encoded = json.dumps(
            {
                "sequence": event.sequence,
                "kind": event.kind.value,
                "case_id": event.case_id,
                "payload": event.payload,
            },
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        )
        self._stream.write(encoded + "\n")
        self._stream.flush()
        os.fsync(self._stream.fileno())
        self._sequence += 1
        return event

    def close(self) -> None:
        if not self._closed:
            self._stream.close()
            self._closed = True

    def __enter__(self) -> "EventWriter":
        return self

    def __exit__(self, exception_type, exception, traceback) -> None:
        self.close()


class CancellationToken:
    """Thread-safe cancellation flag whose first reason is authoritative."""

    def __init__(self) -> None:
        self._event = threading.Event()
        self._lock = threading.Lock()
        self._reason: str | None = None

    @property
    def requested(self) -> bool:
        return self._event.is_set()

    @property
    def reason(self) -> str | None:
        with self._lock:
            return self._reason

    def request(self, reason: str) -> bool:
        if not isinstance(reason, str) or not reason.strip():
            raise ValueError("cancellation reason must be a non-empty string")
        with self._lock:
            if self._event.is_set():
                return False
            self._reason = reason
            self._event.set()
            return True
