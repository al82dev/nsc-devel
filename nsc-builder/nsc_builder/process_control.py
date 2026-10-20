"""Extracted process_control implementation for modular NSC Builder 3."""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
import math
import os
import queue
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
import uuid
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable, Sequence

from .constants import *


class ProcessController:
    """Run one child process with streamed output and scoped tree cancellation."""

    def __init__(
        self,
        output: Callable[[str], None],
        activity: Callable[[float, str], None] | None = None,
    ) -> None:
        self.output = output
        self.activity = activity
        self.cancelled = threading.Event()
        self.last_termination_reason: str | None = None
        self._lock = threading.Lock()
        self._termination_lock = threading.Lock()
        self._process: subprocess.Popen[str] | None = None
        self._variant_active = False
        self._variant_generation = 0
        self._variant_stop = threading.Event()
        self._variant_stop_reason: str | None = None

    def begin_variant(self) -> int:
        """Open a fresh cancellation scope that cannot leak into the next variant."""
        with self._lock:
            if self._variant_active:
                raise RuntimeError("a variant cancellation scope is already active")
            self._variant_generation += 1
            self._variant_active = True
            self._variant_stop.clear()
            self._variant_stop_reason = None
            self.last_termination_reason = None
            return self._variant_generation

    def end_variant(self, generation: int) -> str | None:
        """Close one exact scope and return its first terminal stop reason."""
        with self._lock:
            if not self._variant_active or generation != self._variant_generation:
                raise RuntimeError("variant cancellation scope identity mismatch")
            reason = self._variant_stop_reason
            self._variant_active = False
            self._variant_stop.clear()
            self._variant_stop_reason = None
            return reason

    def cancel_current(
        self, reason: str = "case-cancelled", generation: int | None = None
    ) -> bool:
        """Stop only the active variant tree and leave global cancellation clear."""
        if reason not in {"case-cancelled", "timeout", "cancelled"}:
            raise ValueError(f"unsupported variant stop reason: {reason}")
        with self._lock:
            if (
                not self._variant_active
                or (generation is not None and generation != self._variant_generation)
                or self._variant_stop_reason is not None
            ):
                return False
            self._variant_stop_reason = reason
            self.last_termination_reason = reason
            self._variant_stop.set()
            process = self._process
        if process is not None:
            self._terminate_owned(process)
        return True

    def _requested_stop_reason(self) -> str | None:
        """Read global or current-variant stop state without consuming either."""
        if self.cancelled.is_set():
            return "cancelled"
        if self._variant_stop.is_set():
            with self._lock:
                return self._variant_stop_reason
        return None

    def run(
        self,
        command: Sequence[str],
        cwd: Path,
        environment: dict[str, str] | None = None,
    ) -> tuple[int, str]:
        """Stream merged output and return exit code plus captured text."""
        stop_reason = self._requested_stop_reason()
        if stop_reason is not None:
            self.last_termination_reason = stop_reason
            message = f"{stop_reason} before launch\n"
            self.output(message)
            return 130, message
        flags = WINDOWS_HIDDEN_PROCESS_GROUP_FLAGS if os.name == "nt" else 0
        process = subprocess.Popen(
            [str(part) for part in command],
            cwd=cwd,
            env=environment,
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            creationflags=flags,
        )
        with self._lock:
            self._process = process
        output: list[str] = []
        pending: queue.Queue[str | None] = queue.Queue()
        started = time.monotonic()
        last_activity = started
        last_line = ""
        assert process.stdout is not None

        def read_output() -> None:
            """Drain the child pipe without blocking cancellation checks."""
            for line in process.stdout:
                pending.put(line)
            pending.put(None)

        reader = threading.Thread(target=read_output, name="nscbuilder3-output", daemon=True)
        reader.start()
        stream_closed = False
        while True:
            try:
                line = pending.get(timeout=0.1)
                if line is None:
                    stream_closed = True
                else:
                    output.append(line)
                    self.output(line)
                    last_line = line.strip() or last_line
            except queue.Empty:
                pass
            now = time.monotonic()
            if self.activity is not None and now - last_activity >= 0.25:
                self.activity(now - started, last_line)
                last_activity = now
            if process.poll() is not None and stream_closed and pending.empty():
                break
            stop_reason = self._requested_stop_reason()
            if stop_reason is not None:
                self.last_termination_reason = stop_reason
                self._terminate_owned(process)
                message = f"process stopped: {stop_reason}\n"
                output.append(message)
                self.output(message)
                break
        reader.join(timeout=1.0)
        while True:
            try:
                line = pending.get_nowait()
            except queue.Empty:
                break
            if line is not None:
                output.append(line)
                self.output(line)
        with self._lock:
            self._process = None
        return process.returncode if process.returncode is not None else 130, "".join(output)

    def run_completed(
        self,
        command: Sequence[str],
        cwd: Path,
        environment: dict[str, str] | None = None,
    ) -> subprocess.CompletedProcess[str]:
        """Run one owned child while preserving stdout and stderr separately."""
        arguments = [str(part) for part in command]
        stop_reason = self._requested_stop_reason()
        if stop_reason is not None:
            self.last_termination_reason = stop_reason
            message = f"{stop_reason} before launch\n"
            self.output(message)
            return subprocess.CompletedProcess(arguments, 130, "", message)
        flags = WINDOWS_HIDDEN_PROCESS_GROUP_FLAGS if os.name == "nt" else 0
        process = subprocess.Popen(
            arguments,
            cwd=cwd,
            env=environment,
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            creationflags=flags,
        )
        with self._lock:
            self._process = process
        pending: queue.Queue[tuple[str, str | None]] = queue.Queue()
        captured = {"stdout": [], "stderr": []}
        started = time.monotonic()
        last_activity = started
        last_line = ""
        assert process.stdout is not None
        assert process.stderr is not None

        def read_channel(name: str, stream: Iterable[str]) -> None:
            """Drain one child channel and identify its end in the shared queue."""
            for line in stream:
                pending.put((name, line))
            pending.put((name, None))

        readers = [
            threading.Thread(
                target=read_channel,
                args=("stdout", process.stdout),
                name="nscbuilder3-stdout",
                daemon=True,
            ),
            threading.Thread(
                target=read_channel,
                args=("stderr", process.stderr),
                name="nscbuilder3-stderr",
                daemon=True,
            ),
        ]
        for reader in readers:
            reader.start()
        closed: set[str] = set()
        while True:
            try:
                channel, line = pending.get(timeout=0.1)
                if line is None:
                    closed.add(channel)
                else:
                    captured[channel].append(line)
                    self.output(line)
                    last_line = line.strip() or last_line
            except queue.Empty:
                pass
            now = time.monotonic()
            if self.activity is not None and now - last_activity >= 0.25:
                self.activity(now - started, last_line)
                last_activity = now
            if process.poll() is not None and len(closed) == 2 and pending.empty():
                break
            stop_reason = self._requested_stop_reason()
            if stop_reason is not None:
                self.last_termination_reason = stop_reason
                self._terminate_owned(process)
                message = f"process stopped: {stop_reason}\n"
                captured["stderr"].append(message)
                self.output(message)
                break
        for reader in readers:
            reader.join(timeout=1.0)
        while True:
            try:
                channel, line = pending.get_nowait()
            except queue.Empty:
                break
            if line is not None:
                captured[channel].append(line)
                self.output(line)
        with self._lock:
            self._process = None
        return subprocess.CompletedProcess(
            arguments,
            process.returncode if process.returncode is not None else 130,
            "".join(captured["stdout"]),
            "".join(captured["stderr"]),
        )

    def cancel(self) -> None:
        """Cancel the complete operation and its currently owned child tree."""
        self.last_termination_reason = "cancelled"
        self.cancelled.set()
        with self._lock:
            if self._variant_active and self._variant_stop_reason is None:
                self._variant_stop_reason = "cancelled"
                self._variant_stop.set()
            process = self._process
        if process is not None:
            self._terminate_owned(process)

    def _terminate_owned(self, process: subprocess.Popen[str]) -> None:
        """Serialize concurrent UI, timer, and polling termination requests."""
        with self._termination_lock:
            self._terminate_tree(process)

    @staticmethod
    def _terminate_tree(process: subprocess.Popen[str]) -> None:
        """Terminate a known child tree without enumerating unrelated processes."""
        if process.poll() is not None:
            return
        if os.name == "nt":
            completed = subprocess.run(
                ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                capture_output=True,
                creationflags=WINDOWS_HIDDEN_PROCESS_FLAGS,
                check=False,
            )
            if completed.returncode != 0 and process.poll() is None:
                process.kill()
        else:
            os.killpg(os.getpgid(process.pid), signal.SIGTERM)
        try:
            process.wait(timeout=5.0)
        except subprocess.TimeoutExpired:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=5.0)
