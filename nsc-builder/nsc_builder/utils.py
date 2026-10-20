"""Extracted utils implementation for modular NSC Builder 3."""

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
from .paths import *


def utc_now() -> str:
    """Return an ISO-8601 timestamp used by durable evidence records."""
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def unique_json_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    """Build one object without applying JSON's ambiguous last-key-wins rule."""
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def write_json(path: Path, value: object) -> None:
    """Publish one deterministic JSON artifact after creating its directory."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, indent=2, ensure_ascii=False, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def write_json_atomic(path: Path, value: object) -> None:
    """Durably replace one JSON file without exposing partial contents."""
    content = json.dumps(value, indent=2, ensure_ascii=False, sort_keys=True) + "\n"
    write_text_atomic(path, content)


def write_text_atomic(path: Path, content: str) -> None:
    """Durably replace one UTF-8 text file without exposing partial contents."""
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    try:
        with temporary.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def sha256_file(path: Path) -> str:
    """Hash a file iteratively so large executables do not require one allocation."""
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def current_revision() -> str:
    """Read the local revision for evidence without contacting a remote."""
    completed = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=SOURCE_ROOT,
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        creationflags=WINDOWS_HIDDEN_PROCESS_FLAGS if os.name == "nt" else 0,
        check=False,
    )
    return completed.stdout.strip() if completed.returncode == 0 else "UNKNOWN"
