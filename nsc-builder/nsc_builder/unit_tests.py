"""Canonical selection rules for the dedicated unit-test workspace."""

from __future__ import annotations

from typing import Iterable

from .constants import PROFILE_RANK
from .models import CaseRecord


def select_unit_cases(
    cases: Iterable[CaseRecord],
    requested_case: str = "",
    profile: str = "Todos",
) -> tuple[CaseRecord, ...]:
    """Return unique canonical unit-layer rows matching the visible filters."""
    if profile != "Todos" and profile not in PROFILE_RANK:
        raise ValueError(f"unsupported unit-test profile: {profile}")
    selected: list[CaseRecord] = []
    seen: set[str] = set()
    for case in cases:
        if case.layer != "unit":
            continue
        if requested_case and case.case_id != requested_case:
            continue
        if profile != "Todos" and PROFILE_RANK.get(case.profile, 99) > PROFILE_RANK[profile]:
            continue
        if case.case_id in seen:
            raise ValueError(f"duplicate canonical unit-test case: {case.case_id}")
        seen.add(case.case_id)
        selected.append(case)
    if requested_case and not selected:
        raise ValueError(
            f"case is absent from the canonical unit layer: {requested_case}"
        )
    return tuple(selected)


__all__ = ("select_unit_cases",)
