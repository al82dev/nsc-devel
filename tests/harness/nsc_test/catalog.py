"""Iterative discovery and deterministic selection of NSC test cases."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

from .model import ManifestError, TestCase


class CatalogError(ValueError):
    """Raised when the test catalog is incomplete or ambiguous."""


@dataclass(frozen=True)
class TestCatalog:
    root: Path
    cases: tuple[TestCase, ...]

    @staticmethod
    def load(root: Path) -> "TestCatalog":
        resolved = root.resolve()
        if not resolved.exists():
            return TestCatalog(root=resolved, cases=())
        if not resolved.is_dir():
            raise CatalogError(f"catalog is not a directory: {resolved}")

        manifests: list[Path] = []
        pending = [resolved]
        while pending:
            current = pending.pop()
            with os.scandir(current) as entries:
                for entry in entries:
                    path = Path(entry.path)
                    if entry.is_dir(follow_symlinks=False):
                        pending.append(path)
                    elif entry.is_file(follow_symlinks=False) and (
                        path.name.lower() == "case.json"
                        or path.name.lower().endswith(".case.json")
                    ):
                        manifests.append(path)
        manifests.sort(key=lambda value: value.as_posix().lower())

        cases: list[TestCase] = []
        identifiers: dict[str, Path] = {}
        for manifest in manifests:
            try:
                case = TestCase.from_manifest(manifest)
            except ManifestError as error:
                raise CatalogError(str(error)) from error
            previous = identifiers.get(case.identifier)
            if previous is not None:
                raise CatalogError(
                    f"duplicate case id {case.identifier}: {previous} and {manifest}"
                )
            identifiers[case.identifier] = manifest
            cases.append(case)
        cases.sort(key=lambda value: value.identifier)
        return TestCatalog(root=resolved, cases=tuple(cases))

    def require(self, identifier: str) -> TestCase:
        for case in self.cases:
            if case.identifier == identifier:
                return case
        raise CatalogError(f"unknown case id: {identifier}")

    def by_component(self, component: str) -> tuple[TestCase, ...]:
        return tuple(case for case in self.cases if case.component == component)
