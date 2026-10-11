"""Command-line and graphical entrypoint for NSC Builder 3.

The application audits the canonical catalogue on request and otherwise opens
the complete Builder 3 graphical interface.
"""

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

from .runtime_support import catalog_audit
from .gui import launch_gui


def main(argv: Sequence[str] | None = None) -> int:
    """Expose GUI launch and non-mutating catalogue diagnostics from one script."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--audit-catalog",
        action="store_true",
        help="validate and print the complete Phase 2 catalogue without launching the GUI",
    )
    parser.add_argument(
        "--gui-smoke",
        action="store_true",
        help="construct and close the GUI without configuring, building or running tests",
    )
    arguments = parser.parse_args(argv)
    try:
        if arguments.audit_catalog:
            summary = catalog_audit()
            print(json.dumps(summary, indent=2, ensure_ascii=False, sort_keys=True))
            return 0
        return launch_gui(smoke_only=arguments.gui_smoke)
    except Exception as error:
        print(f"nscbuilder3: error: {error}", file=sys.stderr)
        return 2
