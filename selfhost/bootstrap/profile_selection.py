"""Resolve the self-host optimization subset selected by the outer runner."""
from __future__ import annotations

import os
import re
from collections.abc import Iterable


ENVIRONMENT_VARIABLE = "NSC_SELFHOST_PROFILES"


def selected_profiles(
    available: Iterable[str],
    raw: str | None = None,
) -> list[str]:
    canonical = list(dict.fromkeys(str(profile).strip().upper() for profile in available))
    if not canonical or any(not profile for profile in canonical):
        raise ValueError("self-host contract has no valid optimization profiles")
    selection = os.environ.get(ENVIRONMENT_VARIABLE) if raw is None else raw
    if selection is None or not selection.strip():
        return canonical
    requested = list(dict.fromkeys(
        token.upper() for token in re.split(r"[,;\s]+", selection.strip()) if token
    ))
    invalid = [profile for profile in requested if profile not in canonical]
    if not requested or invalid:
        detail = ", ".join(invalid) if invalid else "empty selection"
        raise ValueError(f"invalid self-host optimization profile selection: {detail}")
    return requested
