"""Load the frozen single-file implementation once for behavioral parity."""

from __future__ import annotations

import importlib.util
import sys
from functools import lru_cache
from pathlib import Path
from types import ModuleType


PACKAGE_ROOT = Path(__file__).resolve().parent
MODULAR_ROOT = PACKAGE_ROOT.parent
REPOSITORY_ROOT = MODULAR_ROOT.parent
CURRENT_IMPLEMENTATION = REPOSITORY_ROOT / "nscbuilder3.py"
_MODULE_NAME = "_nscbuilder3_current_runtime"


@lru_cache(maxsize=1)
def legacy() -> ModuleType:
    """Load the current implementation by absolute path and bind shared models."""
    if not CURRENT_IMPLEMENTATION.is_file():
        raise RuntimeError(
            f"current NSC Builder 3 implementation does not exist: {CURRENT_IMPLEMENTATION}"
        )
    spec = importlib.util.spec_from_file_location(_MODULE_NAME, CURRENT_IMPLEMENTATION)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load NSC Builder 3: {CURRENT_IMPLEMENTATION}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[_MODULE_NAME] = module
    try:
        spec.loader.exec_module(module)
    except Exception:
        sys.modules.pop(_MODULE_NAME, None)
        raise
    from .models import bind_runtime_models

    bind_runtime_models(module)
    return module


__all__ = (
    "CURRENT_IMPLEMENTATION",
    "MODULAR_ROOT",
    "PACKAGE_ROOT",
    "REPOSITORY_ROOT",
    "legacy",
)
