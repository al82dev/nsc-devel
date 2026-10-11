"""Spanish PySide6 GUI projection of the canonical runtime models."""

from __future__ import annotations

from ._runtime import legacy


launch_gui = legacy().launch_gui

__all__ = ("launch_gui",)
