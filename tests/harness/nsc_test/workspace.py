"""Isolated case workspaces rooted on the authorized A: drive."""

from __future__ import annotations

import os
import re
import shutil
import tempfile
from dataclasses import dataclass
from pathlib import Path

from .model import TestCase


SAFE_NAME = re.compile(r"[^A-Za-z0-9_.-]+")


@dataclass
class CaseWorkspace:
    root: Path
    files: dict[Path, Path]
    keep: bool = False

    @staticmethod
    def create(case: TestCase, work_root: Path) -> "CaseWorkspace":
        resolved_root = work_root.resolve()
        if resolved_root.drive.upper() != "A:":
            raise ValueError(f"case work root must be on A: {resolved_root}")
        resolved_root.mkdir(parents=True, exist_ok=True)
        safe_identifier = SAFE_NAME.sub("-", case.identifier).strip("-") or "case"
        directory = Path(tempfile.mkdtemp(prefix=f"{safe_identifier}-", dir=resolved_root))
        mapping: dict[Path, Path] = {}
        manifest_root = case.manifest_path.parent.resolve()
        inputs = list(case.sources) + list(case.working_files)
        if case.project is not None:
            inputs.append(case.project)
        for source in inputs:
            source = source.resolve()
            try:
                relative = source.relative_to(manifest_root)
            except ValueError as error:
                raise ValueError(f"case input is outside manifest directory: {source}") from error
            destination = directory / relative
            if destination in mapping.values():
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
            mapping[source] = destination
        return CaseWorkspace(root=directory, files=mapping)

    def mapped(self, source: Path) -> Path:
        resolved = source.resolve()
        if resolved not in self.files:
            raise KeyError(f"source is not staged in workspace: {resolved}")
        return self.files[resolved]

    def cleanup(self) -> None:
        if self.keep or not self.root.exists():
            return
        remove_tree_iterative(self.root)

    def __enter__(self) -> "CaseWorkspace":
        return self

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> None:
        self.cleanup()


def remove_tree_iterative(root: Path) -> None:
    """Delete a known workspace using an explicit post-order worklist."""

    resolved = root.resolve()
    if resolved.drive.upper() != "A:":
        raise ValueError(f"refusing to remove a tree outside A: {resolved}")
    pending: list[tuple[Path, bool]] = [(resolved, False)]
    while pending:
        current, visited = pending.pop()
        if current.is_symlink() or current.is_file():
            current.unlink(missing_ok=True)
            continue
        if visited:
            current.rmdir()
            continue
        pending.append((current, True))
        with os.scandir(current) as entries:
            for entry in entries:
                pending.append((Path(entry.path), False))
