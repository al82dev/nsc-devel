"""Extracted runtime_support implementation for modular NSC Builder 3."""

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
from .utils import *
from .configuration import *
from .catalog import *


def make_run_id() -> str:
    """Create a sortable collision-resistant local run identifier."""
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    return f"{stamp}-{uuid.uuid4().hex[:8]}"


def compiler_receipt(config: BuilderConfig) -> dict[str, Any]:
    """Capture immutable identities for the externally supplied NSC and NSLink."""
    config.validated(require_tools=True)
    receipt: dict[str, Any] = {
        "captured_at": utc_now(),
        "tools": [],
        "llvm_toolchain": discover_llvm_toolchain(config.llvm_root_path()),
    }
    for name, text in (
        ("nsc", config.nsc_executable),
        ("nslink", config.nslink_executable),
    ):
        path = Path(text).resolve()
        receipt["tools"].append(
            {
                "name": name,
                "path": str(path),
                "size": path.stat().st_size,
                "mtime_ns": path.stat().st_mtime_ns,
                "sha256": sha256_file(path),
            }
        )
    return receipt


def file_receipt(name: str, path_value: str | Path) -> dict[str, Any]:
    """Capture one immutable file identity for build evidence."""
    path = Path(path_value).resolve()
    if not path.is_file():
        raise FileNotFoundError(f"{name} does not exist: {path}")
    return {
        "name": name,
        "path": str(path),
        "size": path.stat().st_size,
        "mtime_ns": path.stat().st_mtime_ns,
        "sha256": sha256_file(path),
    }


def catalog_audit() -> dict[str, Any]:
    """Load the complete catalogue and expose closure facts without running tests."""
    catalog = CanonicalCatalog.load()
    summary = catalog.summary()
    summary["manifest_rows"] = len(read_jsonl(MANIFEST_PATH))
    return summary
