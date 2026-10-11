"""Command-line entry point preserving the current Builder 3 contract."""

from __future__ import annotations

from ._runtime import legacy


main = legacy().main

__all__ = ("main",)
