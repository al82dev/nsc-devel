"""Windows process flags shared by non-interactive suite executors."""

from __future__ import annotations

import os
import subprocess


HIDDEN_PROCESS_FLAGS = (
    getattr(subprocess, "CREATE_NO_WINDOW", 0) if os.name == "nt" else 0
)


__all__ = ("HIDDEN_PROCESS_FLAGS",)
