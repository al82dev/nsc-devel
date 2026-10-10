"""Isolated A:-drive workspaces for compiler-driven suite cases."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path


SAFE_SEGMENT = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]*$")


class WorkspaceError(ValueError):
    """Raised when a workspace would be ambiguous, unsafe, or reusable."""


@dataclass(frozen=True)
class CaseWorkspace:
    root: Path
    inputs: Path
    outputs: Path
    logs: Path
    artifacts: Path
    metadata: Path


class WorkspaceManager:
    def __init__(self, root: Path) -> None:
        resolved = root.resolve()
        if resolved.drive.upper() != "A:":
            raise WorkspaceError(f"compiler suite workspace root must be on A: {resolved}")
        self.root = resolved

    def run_root(self, run_id: str) -> Path:
        """Resolve one safe run directory without creating filesystem state."""
        _validate_segment(run_id, "run_id")
        return self.root / run_id

    def prepare(self, run_id: str, case_id: str) -> CaseWorkspace:
        _validate_segment(case_id, "case_id")
        run_root = self.run_root(run_id)
        case_root = run_root / case_id
        if case_root.exists():
            raise WorkspaceError(f"case workspace already exists: {case_root}")

        inputs = case_root / "inputs"
        outputs = case_root / "outputs"
        logs = case_root / "logs"
        artifacts = case_root / "artifacts"
        for directory in (inputs, outputs, logs, artifacts):
            directory.mkdir(parents=True, exist_ok=False)

        metadata = case_root / "workspace.json"
        payload = {
            "schema": 1,
            "run_id": run_id,
            "case_id": case_id,
            "root": str(case_root),
            "inputs": str(inputs),
            "outputs": str(outputs),
            "logs": str(logs),
            "artifacts": str(artifacts),
        }
        metadata.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        return CaseWorkspace(case_root, inputs, outputs, logs, artifacts, metadata)


def _validate_segment(value: str, field: str) -> None:
    if SAFE_SEGMENT.fullmatch(value) is None:
        raise WorkspaceError(f"invalid {field}: {value!r}")
