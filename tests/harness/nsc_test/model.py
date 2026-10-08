"""Validated immutable model for NativeSharp test case manifests."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Any


IDENTIFIER = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]*$")
COMPONENT = re.compile(r"^[a-z0-9][a-z0-9-]*$")
ALLOWED_KEYS = {
    "id",
    "component",
    "kind",
    "sources",
    "project",
    "arguments",
    "optimization",
    "stdin",
    "expected",
    "timeout_seconds",
    "tags",
    "requires",
    "working_files",
}
EXPECTED_KEYS = {
    "exit_code",
    "stdout",
    "stderr",
    "artifacts",
    "run_exit_code",
}


class ManifestError(ValueError):
    """Raised when a case manifest violates the stable harness contract."""


class CaseKind(str, Enum):
    LEX = "lex"
    PARSE = "parse"
    SEMANTIC = "semantic"
    DUMP = "dump"
    EMIT_OBJECT = "emit-object"
    EMIT_LIBRARY = "emit-library"
    COMPILE_ONLY = "compile-only"
    LINK_RUN = "link-run"
    PROJECT = "project"
    METADATA = "metadata"
    SPECIALIZED = "specialized"


@dataclass(frozen=True)
class ExpectedOutcome:
    exit_code: int
    stdout: str | None = None
    stderr: str | None = None
    artifacts: tuple[str, ...] = ()
    run_exit_code: int | None = None


@dataclass(frozen=True)
class TestCase:
    identifier: str
    component: str
    kind: CaseKind
    manifest_path: Path
    sources: tuple[Path, ...]
    project: Path | None
    arguments: tuple[str, ...]
    optimization: str
    stdin: str | None
    expected: ExpectedOutcome
    timeout_seconds: float
    tags: tuple[str, ...]
    requires: tuple[str, ...]
    working_files: tuple[Path, ...]

    @staticmethod
    def from_manifest(path: Path) -> "TestCase":
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise ManifestError(f"cannot read manifest {path}: {error}") from error
        if not isinstance(payload, dict):
            raise ManifestError(f"manifest must contain a JSON object: {path}")
        unknown = sorted(set(payload) - ALLOWED_KEYS)
        if unknown:
            raise ManifestError(f"unknown fields in {path}: {', '.join(unknown)}")

        identifier = required_string(payload, "id", path)
        if IDENTIFIER.fullmatch(identifier) is None:
            raise ManifestError(f"invalid case id in {path}: {identifier}")
        component = required_string(payload, "component", path)
        if COMPONENT.fullmatch(component) is None:
            raise ManifestError(f"invalid component in {path}: {component}")
        kind_text = required_string(payload, "kind", path)
        try:
            kind = CaseKind(kind_text)
        except ValueError as error:
            raise ManifestError(f"invalid case kind in {path}: {kind_text}") from error

        base = path.parent.resolve()
        sources = tuple(resolve_relative_files(base, string_list(payload, "sources", path)))
        project_value = optional_string(payload, "project", path)
        project = resolve_relative_file(base, project_value) if project_value is not None else None
        if not sources and project is None and kind is not CaseKind.SPECIALIZED:
            raise ManifestError(f"case requires sources or project: {path}")

        optimization = str(payload.get("optimization", "O0"))
        if optimization != "O0":
            raise ManifestError(f"only O0 is authorized during migration: {path}")
        arguments = tuple(string_list(payload, "arguments", path))
        stdin = optional_string(payload, "stdin", path)
        timeout = payload.get("timeout_seconds", 60.0)
        if not isinstance(timeout, (int, float)) or isinstance(timeout, bool) or timeout <= 0:
            raise ManifestError(f"timeout_seconds must be positive in {path}")
        expected = parse_expected(payload.get("expected"), path)
        tags = tuple(string_list(payload, "tags", path))
        requires = tuple(string_list(payload, "requires", path))
        working_files = tuple(
            resolve_relative_files(base, string_list(payload, "working_files", path))
        )
        return TestCase(
            identifier=identifier,
            component=component,
            kind=kind,
            manifest_path=path.resolve(),
            sources=sources,
            project=project,
            arguments=arguments,
            optimization=optimization,
            stdin=stdin,
            expected=expected,
            timeout_seconds=float(timeout),
            tags=tags,
            requires=requires,
            working_files=working_files,
        )


def required_string(payload: dict[str, Any], key: str, path: Path) -> str:
    value = payload.get(key)
    if not isinstance(value, str) or not value:
        raise ManifestError(f"{key} must be a non-empty string in {path}")
    return value


def optional_string(payload: dict[str, Any], key: str, path: Path) -> str | None:
    value = payload.get(key)
    if value is None:
        return None
    if not isinstance(value, str):
        raise ManifestError(f"{key} must be a string in {path}")
    return value


def string_list(payload: dict[str, Any], key: str, path: Path) -> list[str]:
    value = payload.get(key, [])
    if not isinstance(value, list) or any(not isinstance(item, str) or not item for item in value):
        raise ManifestError(f"{key} must be an array of non-empty strings in {path}")
    return value


def resolve_relative_file(base: Path, value: str) -> Path:
    candidate = (base / value).resolve()
    if not candidate.is_relative_to(base):
        raise ManifestError(f"case path escapes its directory: {value}")
    if not candidate.is_file():
        raise ManifestError(f"case file does not exist: {candidate}")
    return candidate


def resolve_relative_files(base: Path, values: list[str]) -> list[Path]:
    return [resolve_relative_file(base, value) for value in values]


def parse_expected(value: Any, path: Path) -> ExpectedOutcome:
    if not isinstance(value, dict):
        raise ManifestError(f"expected must be a JSON object in {path}")
    unknown = sorted(set(value) - EXPECTED_KEYS)
    if unknown:
        raise ManifestError(f"unknown expected fields in {path}: {', '.join(unknown)}")
    exit_code = value.get("exit_code")
    if not isinstance(exit_code, int) or isinstance(exit_code, bool):
        raise ManifestError(f"expected.exit_code must be an integer in {path}")
    stdout = value.get("stdout")
    stderr = value.get("stderr")
    run_exit_code = value.get("run_exit_code")
    if stdout is not None and not isinstance(stdout, str):
        raise ManifestError(f"expected.stdout must be a string in {path}")
    if stderr is not None and not isinstance(stderr, str):
        raise ManifestError(f"expected.stderr must be a string in {path}")
    if run_exit_code is not None and (
        not isinstance(run_exit_code, int) or isinstance(run_exit_code, bool)
    ):
        raise ManifestError(f"expected.run_exit_code must be an integer in {path}")
    artifacts = value.get("artifacts", [])
    if not isinstance(artifacts, list) or any(
        not isinstance(item, str) or not item for item in artifacts
    ):
        raise ManifestError(f"expected.artifacts must be an array of strings in {path}")
    return ExpectedOutcome(
        exit_code=exit_code,
        stdout=stdout,
        stderr=stderr,
        artifacts=tuple(artifacts),
        run_exit_code=run_exit_code,
    )
