"""Owned child-process execution, tree termination, and scoped cancellation."""

from __future__ import annotations

from ._runtime import legacy


ProcessController = legacy().ProcessController

__all__ = ("ProcessController",)
