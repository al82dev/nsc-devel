"""Deterministic final reports for the new compiler suite."""

from __future__ import annotations

import json
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from .result import CaseResult, CaseStatus


class ReportError(ValueError):
    """Raised when a final report would be incomplete or overwrite evidence."""


@dataclass(frozen=True)
class ToolIdentity:
    name: str
    path: str
    sha256: str

    def __post_init__(self) -> None:
        if not isinstance(self.name, str) or not self.name.strip():
            raise ReportError("tool identity requires a non-empty name")
        if not isinstance(self.path, str) or not self.path.strip():
            raise ReportError("tool identity requires a non-empty path")
        if not isinstance(self.sha256, str) or re.fullmatch(
            r"[0-9a-f]{64}", self.sha256
        ) is None:
            raise ReportError("tool identity requires a lowercase SHA-256")


@dataclass(frozen=True)
class RunMetadata:
    run_id: str
    revision: str
    build_mode: str
    optimization: str
    profile: str
    scope: str
    selected_case_ids: tuple[str, ...]
    tools: tuple[ToolIdentity, ...] = ()

    def __post_init__(self) -> None:
        for field, value in (
            ("run_id", self.run_id),
            ("revision", self.revision),
        ):
            if not isinstance(value, str) or not value.strip():
                raise ReportError(f"run metadata requires a non-empty {field}")
        domains = (
            ("build_mode", self.build_mode, {"Debug", "Release"}),
            ("optimization", self.optimization, {"NA", "O0", "O1", "O2", "O3"}),
            ("profile", self.profile, {"PR", "NIGHTLY", "WEEKLY", "explicit"}),
            ("scope", self.scope, {"all", "focused"}),
        )
        for field, value, allowed in domains:
            if not isinstance(value, str) or value not in allowed:
                raise ReportError(f"run metadata has invalid {field}: {value!r}")
        if not isinstance(self.selected_case_ids, tuple) or not self.selected_case_ids:
            raise ReportError("run metadata requires selected case IDs")
        if any(
            not isinstance(case_id, str) or not case_id
            for case_id in self.selected_case_ids
        ):
            raise ReportError("run metadata contains an invalid selected case ID")
        if len(self.selected_case_ids) != len(set(self.selected_case_ids)):
            raise ReportError("selected case IDs contain duplicates")
        if not isinstance(self.tools, tuple) or any(
            not isinstance(tool, ToolIdentity) for tool in self.tools
        ):
            raise ReportError("run metadata tools must be ToolIdentity values")
        tool_names = tuple(tool.name for tool in self.tools)
        if len(tool_names) != len(set(tool_names)):
            raise ReportError("run metadata contains duplicate tool names")


class ReportWriter:
    FILENAMES = ("results.json", "summary.md", "failures.log")

    def __init__(self, run_root: Path) -> None:
        self.run_root = run_root.resolve()
        if self.run_root.drive.upper() != "A:":
            raise ReportError(f"compiler suite reports must be on A: {self.run_root}")

    def write(self, metadata: RunMetadata, results: Iterable[CaseResult]) -> None:
        ordered = tuple(sorted(results, key=lambda result: result.case_id))
        _validate_complete(metadata, ordered)
        self.run_root.mkdir(parents=True, exist_ok=True)
        destinations = tuple(self.run_root / name for name in self.FILENAMES)
        existing = tuple(path for path in destinations if path.exists())
        if existing:
            raise ReportError(
                "final report already exists: " + ", ".join(str(path) for path in existing)
            )

        payload = _result_payload(metadata, ordered)
        contents = (
            json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            _summary_markdown(metadata, ordered),
            _failure_log(ordered),
        )
        temporary: list[Path] = []
        try:
            for destination, content in zip(destinations, contents, strict=True):
                candidate = destination.with_suffix(destination.suffix + ".tmp")
                with candidate.open("x", encoding="utf-8", newline="\n") as stream:
                    stream.write(content)
                    stream.flush()
                    os.fsync(stream.fileno())
                temporary.append(candidate)
            for candidate, destination in zip(temporary, destinations, strict=True):
                os.replace(candidate, destination)
        finally:
            for candidate in temporary:
                if candidate.exists():
                    candidate.unlink()


def _validate_complete(metadata: RunMetadata, results: tuple[CaseResult, ...]) -> None:
    selected = metadata.selected_case_ids
    if not selected:
        raise ReportError("final report requires at least one selected case")
    if len(selected) != len(set(selected)):
        raise ReportError("selected case IDs contain duplicates")
    result_ids = tuple(result.case_id for result in results)
    if len(result_ids) != len(set(result_ids)):
        raise ReportError("results contain duplicate case IDs")
    missing = sorted(set(selected) - set(result_ids))
    unexpected = sorted(set(result_ids) - set(selected))
    if missing or unexpected:
        raise ReportError(
            f"result coverage mismatch; missing={missing}; unexpected={unexpected}"
        )


def _result_payload(metadata: RunMetadata, results: tuple[CaseResult, ...]) -> dict:
    counts = {status.value: 0 for status in CaseStatus}
    for result in results:
        counts[result.status.value] += 1
    return {
        "schema": 1,
        "run": {
            "run_id": metadata.run_id,
            "revision": metadata.revision,
            "build_mode": metadata.build_mode,
            "optimization": metadata.optimization,
            "profile": metadata.profile,
            "scope": metadata.scope,
            "selected": len(metadata.selected_case_ids),
            "tools": [
                {"name": tool.name, "path": tool.path, "sha256": tool.sha256}
                for tool in sorted(metadata.tools, key=lambda item: item.name)
            ],
        },
        "counts": counts,
        "results": [
            {
                "case_id": result.case_id,
                "status": result.status.value,
                "duration_seconds": result.duration_seconds,
                "failures": list(result.failures),
                "blocked_by": list(result.blocked_by),
            }
            for result in results
        ],
    }


def _summary_markdown(metadata: RunMetadata, results: tuple[CaseResult, ...]) -> str:
    counts = {status: 0 for status in CaseStatus}
    for result in results:
        counts[result.status] += 1
    lines = [
        "# NativeSharp compiler suite — resumen",
        "",
        f"- Corrida: `{metadata.run_id}`",
        f"- Revisión: `{metadata.revision}`",
        f"- Alcance: `{metadata.scope}`",
        f"- Perfil: `{metadata.profile}`",
        f"- Build mode: `{metadata.build_mode}`",
        f"- Optimización: `{metadata.optimization}`",
        f"- Seleccionados: {len(metadata.selected_case_ids)}",
        "",
        "| Estado | Total |",
        "|---|---:|",
    ]
    lines.extend(f"| {status.value} | {counts[status]} |" for status in CaseStatus)
    lines.append("")
    return "\n".join(lines)


def _failure_log(results: tuple[CaseResult, ...]) -> str:
    lines: list[str] = []
    for result in results:
        if result.status is CaseStatus.PASS:
            continue
        lines.extend((f"case={result.case_id}", f"status={result.status.value}"))
        lines.extend(f"failure={failure}" for failure in result.failures)
        lines.extend(f"blocked_by={producer}" for producer in result.blocked_by)
        lines.append("")
    return "\n".join(lines)
