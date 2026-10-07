#!/usr/bin/env python3
"""NativeSharp 3.0 conformance harness (T00).

Each JSON manifest is an auditable contract. Every execution receives a fresh
working directory; repeated observations must be byte-for-byte deterministic.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


SCHEMA = "nativesharp.conformance.v1"
DIAGNOSTIC_PATTERN = re.compile(r"\bNS3\d{3}\b")
CASE_ID_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,127}\Z")


class ManifestError(ValueError):
    pass


@dataclass(frozen=True)
class Observation:
    exit_code: int
    stdout: str
    stderr: str
    diagnostics: tuple[str, ...]
    mir: str
    abi: str


@dataclass(frozen=True)
class CaseResult:
    case_id: str
    status: str
    message: str = ""


def _string_list(value: Any, field: str) -> list[str]:
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise ManifestError(f"NS3099: '{field}' must be an array of strings")
    return value


def _relative_path(value: str, field: str) -> Path:
    path = Path(value)
    if not value or path.is_absolute() or path.anchor or ".." in path.parts:
        raise ManifestError(f"NS3099: '{field}' must stay inside its conformance case")
    return path


def _inside(root: Path, relative: Path, field: str) -> Path:
    root = root.resolve()
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root)
    except ValueError as error:
        raise ManifestError(f"NS3099: '{field}' escapes its conformance case") from error
    return candidate


def load_manifest(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ManifestError(f"NS3099: cannot read manifest '{path}': {error}") from error
    if not isinstance(data, dict):
        raise ManifestError("NS3099: manifest root must be an object")
    required = {"schema", "id", "kind", "target", "command", "expected"}
    missing = sorted(required - data.keys())
    if missing:
        raise ManifestError(f"NS3099: missing fields: {', '.join(missing)}")
    if data["schema"] != SCHEMA:
        raise ManifestError(f"NS3099: schema must be '{SCHEMA}'")
    if not isinstance(data["id"], str) or CASE_ID_PATTERN.fullmatch(data["id"]) is None:
        raise ManifestError("NS3099: 'id' must be a safe 1-128 character identifier")
    if data["kind"] not in ("normative", "regression"):
        raise ManifestError("NS3099: 'kind' must be 'normative' or 'regression'")
    if data["target"] != "windows-x64":
        raise ManifestError("NS3099: T00 supports only target 'windows-x64'")
    _string_list(data["command"], "command")
    if not data["command"]:
        raise ManifestError("NS3099: 'command' cannot be empty")
    if not isinstance(data["expected"], dict):
        raise ManifestError("NS3099: 'expected' must be an object")
    expected = data["expected"]
    if not isinstance(expected.get("exitCode", 0), int):
        raise ManifestError("NS3099: 'expected.exitCode' must be an integer")
    for field in ("stdout", "stderr", "mirFile", "abiFile"):
        if field in expected and not isinstance(expected[field], str):
            raise ManifestError(f"NS3099: 'expected.{field}' must be a string")
    for field in (
        "diagnostics",
        "stdoutContains",
        "stderrContains",
        "mirContains",
        "abiContains",
    ):
        if field in expected:
            _string_list(expected[field], f"expected.{field}")
    for code in expected.get("diagnostics", []):
        if DIAGNOSTIC_PATTERN.fullmatch(code) is None:
            raise ManifestError(
                "NS3099: 'expected.diagnostics' accepts only stable NS3xxx codes"
            )
    for evidence, checks in (("mirFile", "mirContains"), ("abiFile", "abiContains")):
        if evidence in expected:
            _relative_path(expected[evidence], f"expected.{evidence}")
        if expected.get(checks) and evidence not in expected:
            raise ManifestError(
                f"NS3099: 'expected.{checks}' requires 'expected.{evidence}'"
            )
    timeout = data.get("timeoutSeconds", 30.0)
    repeat = data.get("repeat", 1)
    if not isinstance(timeout, (int, float)) or timeout <= 0 or timeout > 600:
        raise ManifestError("NS3099: 'timeoutSeconds' must be in (0, 600]")
    if not isinstance(repeat, int) or repeat < 1 or repeat > 10:
        raise ManifestError("NS3099: 'repeat' must be an integer from 1 through 10")
    if "inputs" in data:
        inputs = _string_list(data["inputs"], "inputs")
        if len(inputs) != len(set(inputs)):
            raise ManifestError("NS3099: 'inputs' cannot contain duplicates")
        for index, relative in enumerate(inputs):
            _relative_path(relative, f"inputs[{index}]")
    return data


def _expand(token: str, case_dir: Path, compiler: Path | None) -> str:
    if "{compiler}" in token and compiler is None:
        raise ManifestError("NS3099: manifest requires --compiler for '{compiler}'")
    return (
        token.replace("{python}", sys.executable)
        .replace("{case_dir}", str(case_dir))
        .replace("{compiler}", str(compiler) if compiler is not None else "")
    )


def _observe(
    manifest: dict[str, Any],
    manifest_path: Path,
    case_dir: Path,
    compiler: Path | None,
) -> Observation:
    for relative in manifest.get("inputs", []):
        relative_path = _relative_path(relative, "inputs")
        source = _inside(manifest_path.parent, relative_path, "inputs")
        if not source.is_file():
            raise ManifestError(f"NS3099: input does not exist: '{relative}'")
        destination = _inside(case_dir, relative_path, "inputs")
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

    command = [_expand(token, case_dir, compiler) for token in manifest["command"]]
    environment = os.environ.copy()
    environment["NATIVESHARP_CONFORMANCE_CASE"] = manifest["id"]
    completed = subprocess.run(
        command,
        cwd=case_dir,
        env=environment,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=float(manifest.get("timeoutSeconds", 30.0)),
        check=False,
    )
    expected = manifest["expected"]
    mir_path = expected.get("mirFile")
    abi_path = expected.get("abiFile")
    mir = _inside(case_dir, _relative_path(mir_path, "expected.mirFile"), "expected.mirFile").read_text(
        encoding="utf-8"
    ) if isinstance(mir_path, str) else ""
    abi = _inside(case_dir, _relative_path(abi_path, "expected.abiFile"), "expected.abiFile").read_text(
        encoding="utf-8"
    ) if isinstance(abi_path, str) else ""
    diagnostics = tuple(DIAGNOSTIC_PATTERN.findall(completed.stderr))
    return Observation(completed.returncode, completed.stdout, completed.stderr, diagnostics, mir, abi)


def _matches(expected: dict[str, Any], observed: Observation) -> str | None:
    if observed.exit_code != expected.get("exitCode", 0):
        return f"exit code {observed.exit_code}, expected {expected.get('exitCode', 0)}"
    if "stdout" in expected and observed.stdout != expected["stdout"]:
        return "stdout differs"
    if "stderr" in expected and observed.stderr != expected["stderr"]:
        return "stderr differs"
    for text in expected.get("stdoutContains", []):
        if text not in observed.stdout:
            return f"stdout does not contain {text!r}"
    for text in expected.get("stderrContains", []):
        if text not in observed.stderr:
            return f"stderr does not contain {text!r}"
    expected_diagnostics = tuple(expected.get("diagnostics", []))
    if observed.diagnostics != expected_diagnostics:
        return f"diagnostics {observed.diagnostics!r}, expected {expected_diagnostics!r}"
    for text in expected.get("mirContains", []):
        if text not in observed.mir:
            return f"MIR does not contain {text!r}"
    for text in expected.get("abiContains", []):
        if text not in observed.abi:
            return f"ABI does not contain {text!r}"
    return None


def run_case(
    path: Path,
    work_root: Path | None = None,
    compiler: Path | None = None,
) -> CaseResult:
    try:
        manifest = load_manifest(path)
    except ManifestError as error:
        return CaseResult(path.stem, "invalid", str(error))
    observations: list[Observation] = []
    try:
        for _ in range(manifest.get("repeat", 1)):
            with tempfile.TemporaryDirectory(prefix=f"ns3-{manifest['id']}-", dir=work_root) as temporary:
                observations.append(_observe(manifest, path, Path(temporary), compiler))
    except subprocess.TimeoutExpired:
        return CaseResult(manifest["id"], "timeout", "case exceeded timeoutSeconds")
    except (ManifestError, OSError) as error:
        return CaseResult(manifest["id"], "invalid", str(error))
    if any(item != observations[0] for item in observations[1:]):
        return CaseResult(manifest["id"], "nondeterministic", "repeated observations differ")
    mismatch = _matches(manifest["expected"], observations[0])
    return CaseResult(manifest["id"], "failed" if mismatch else "passed", mismatch or "")


def _write_manifest(root: Path, name: str, content: dict[str, Any]) -> Path:
    path = root / f"{name}.json"
    path.write_text(json.dumps(content), encoding="utf-8")
    return path


def self_test() -> int:
    with tempfile.TemporaryDirectory(prefix="ns3-harness-selftest-") as temporary:
        root = Path(temporary)
        base = {
            "schema": SCHEMA,
            "kind": "normative",
            "target": "windows-x64",
            "expected": {"exitCode": 0, "stdout": "ok\n", "diagnostics": []},
        }
        valid = dict(base, id="valid", command=["{python}", "-c", "print('ok')"], repeat=2)
        invalid = {"schema": SCHEMA, "id": "invalid"}
        timeout = dict(base, id="timeout", command=["{python}", "-c", "import time; time.sleep(2)"], timeoutSeconds=0.05)
        nondeterministic = dict(base, id="nondeterministic", command=["{python}", "-c", "import time; print(time.time_ns())"], repeat=2)
        isolated = dict(base, id="isolated", command=["{python}", "-c", "from pathlib import Path; p=Path('token'); print('old' if p.exists() else 'ok'); p.write_text('x')"], repeat=2)
        traversal = dict(base, id="traversal", command=["{python}", "-c", "print('ok')"], inputs=["../outside.ns"])
        bad_diagnostic = dict(base, id="bad-diagnostic", command=["{python}", "-c", "print('ok')"], expected={"diagnostics": ["InvalidOperand"]})
        missing_evidence = dict(base, id="missing-evidence", command=["{python}", "-c", "print('ok')"], expected={"mirContains": ["function"]})
        cases = [
            (valid, "passed"),
            (invalid, "invalid"),
            (timeout, "timeout"),
            (nondeterministic, "nondeterministic"),
            (isolated, "passed"),
            (traversal, "invalid"),
            (bad_diagnostic, "invalid"),
            (missing_evidence, "invalid"),
        ]
        for index, (manifest, expected_status) in enumerate(cases):
            result = run_case(_write_manifest(root, f"case-{index}", manifest), root)
            if result.status != expected_status:
                print(f"FAIL: {result.case_id}: {result.status}: {result.message}", file=sys.stderr)
                return 1
    print("PASS: 8 conformance harness checks")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="NativeSharp 3.0 conformance harness")
    parser.add_argument("manifests", nargs="*", type=Path)
    parser.add_argument("--work-root", type=Path)
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()
    if arguments.self_test:
        return self_test()
    if not arguments.manifests:
        parser.error("provide at least one manifest or --self-test")
    compiler = arguments.compiler.resolve() if arguments.compiler is not None else None
    if compiler is not None and not compiler.is_file():
        print(f"INVALID: compiler does not exist: {compiler}", file=sys.stderr)
        return 1
    if arguments.work_root is not None:
        arguments.work_root.mkdir(parents=True, exist_ok=True)
    failed = False
    for manifest in arguments.manifests:
        result = run_case(manifest, arguments.work_root, compiler)
        print(f"{result.status.upper()}: {result.case_id}" + (f": {result.message}" if result.message else ""))
        failed = failed or result.status != "passed"
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
