"""Shared, deterministic case selection for compiler-suite executors."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable

from .manifest import BUILD_MODES, OPTIMIZATIONS, PROFILES, CaseManifest


PROFILE_RANK = {"PR": 0, "NIGHTLY": 1, "WEEKLY": 2}


class SelectionError(ValueError):
    """Raised when a selector is invalid or matches no executable cases."""


@dataclass(frozen=True)
class Selection:
    all_cases: bool = False
    case_ids: frozenset[str] = frozenset()
    contract_ids: frozenset[str] = frozenset()
    components: frozenset[str] = frozenset()
    layers: frozenset[str] = frozenset()
    profile: str | None = None
    build_mode: str | None = None
    optimization: str | None = None
    tags: frozenset[str] = frozenset()
    include_dependencies: bool = True

    def validate(self) -> None:
        has_filter = any(
            (
                self.case_ids,
                self.contract_ids,
                self.components,
                self.layers,
                self.profile,
                self.build_mode,
                self.optimization,
                self.tags,
            )
        )
        if self.all_cases and has_filter:
            raise SelectionError("all_cases cannot be combined with filters")
        if not self.all_cases and not has_filter:
            raise SelectionError("selection requires --all or at least one explicit filter")
        if self.profile is not None and self.profile not in PROFILES:
            raise SelectionError(f"unknown profile: {self.profile}")
        if self.build_mode is not None and self.build_mode not in BUILD_MODES:
            raise SelectionError(f"unknown build mode: {self.build_mode}")
        if self.optimization is not None and self.optimization not in OPTIMIZATIONS:
            raise SelectionError(f"unknown optimization: {self.optimization}")


def select_cases(
    cases: Iterable[CaseManifest], selection: Selection
) -> tuple[CaseManifest, ...]:
    """Apply intersecting filters and optionally add the transitive producer closure."""

    selection.validate()
    ordered = tuple(sorted(cases, key=lambda case: case.case_id))
    by_id = {case.case_id: case for case in ordered}
    if len(by_id) != len(ordered):
        raise SelectionError("case collection contains duplicate IDs")

    if selection.all_cases:
        matched = set(by_id)
    else:
        matched = {
            case.case_id
            for case in ordered
            if _matches(case, selection)
        }
    if not matched:
        raise SelectionError("selection matched no cases")

    if selection.include_dependencies:
        pending = sorted(matched)
        cursor = 0
        while cursor < len(pending):
            current = by_id[pending[cursor]]
            cursor += 1
            for dependency in sorted(current.dependencies):
                if dependency not in by_id:
                    raise SelectionError(
                        f"case {current.case_id} references missing dependency {dependency}"
                    )
                if dependency not in matched:
                    matched.add(dependency)
                    pending.append(dependency)

    return tuple(case for case in ordered if case.case_id in matched)


def _matches(case: CaseManifest, selection: Selection) -> bool:
    if selection.case_ids and case.case_id not in selection.case_ids:
        return False
    if selection.contract_ids and case.contract_id not in selection.contract_ids:
        return False
    if selection.components and case.component not in selection.components:
        return False
    if selection.layers and case.layer not in selection.layers:
        return False
    if selection.profile is not None:
        if PROFILE_RANK[case.profile] > PROFILE_RANK[selection.profile]:
            return False
    if selection.build_mode is not None and selection.build_mode not in case.build_modes:
        return False
    if selection.optimization is not None and selection.optimization not in case.optimizations:
        return False
    if selection.tags and not selection.tags.issubset(case.tags):
        return False
    return True
