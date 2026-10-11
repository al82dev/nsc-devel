#!/usr/bin/env python3
"""SH51: observable builds, durable diagnostics and debugger handoff."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                          encoding="utf-8", errors="replace", check=False)


def load_log(path: Path, failures: list[str]) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        failures.append(f"unreadable JSON log {path.name}: {exception}")
        return {}
    if value.get("schema") != "nativesharp-compilation-log" or value.get("version") != 1:
        failures.append(f"invalid compilation log schema in {path.name}")
    return value


def load_time_trace(path: Path, failures: list[str]) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        failures.append(f"unreadable JSON time trace {path.name}: {exception}")
        return {}
    if value.get("schema") != "nativesharp-time-trace" or value.get("version") != 1:
        failures.append(f"invalid time trace schema in {path.name}")
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--selfhost-compiler", type=Path, required=True)
    parser.add_argument("--ctest", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    root = args.source_root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    failures: list[str] = []
    if contract.get("schema") != "nativesharp-selfhost-developer-experience-contract" or contract.get("gate") != "SH51":
        print("NSH5101: unsupported SH51 contract", file=sys.stderr)
        return 1

    help_result = run([str(args.compiler), "--help"], root)
    for option in contract["required_cli"]:
        if option not in help_result.stdout:
            failures.append(f"help omits required option {option}")

    success_log = output / "success.json"
    success = run([str(args.compiler), "--version", "--verbose", f"--log={success_log}",
                   "--log-format=json"], root)
    success_data = load_log(success_log, failures)
    if success.returncode != 0 or success_data.get("exit_code") != 0:
        failures.append("verbose successful compilation log did not preserve success")
    if "timing total_ms=" not in str(success_data.get("stdout", "")):
        failures.append("verbose log does not contain timing information")

    failure_log = output / "failure.json"
    missing = output / "missing-source.ns"
    failed = run([str(args.compiler), str(missing), "--quiet",
                  f"--log-on-error={failure_log}", "--log-format=json"], root)
    failure_data = load_log(failure_log, failures)
    if failed.returncode == 0 or failure_data.get("exit_code") != failed.returncode:
        failures.append("failure log did not preserve the compiler exit code")
    if "cannot open source file" not in str(failure_data.get("stderr", "")):
        failures.append("failure log omitted the actionable root diagnostic")
    if failed.stdout:
        failures.append("--quiet leaked normal stdout")

    trace_path = output / "time-trace.json"
    trace = run([str(args.compiler), "--version", f"--time-trace={trace_path}"], root)
    trace_data = load_time_trace(trace_path, failures)
    if (trace.returncode != 0 or trace_data.get("exit_code") != 0 or
            "timing total_ms=" not in trace.stdout):
        failures.append("time trace did not retain measured compiler output")

    selfhost_missing = output / "selfhost-missing.ns"
    diagnostic = run([str(args.selfhost_compiler), "--diagnostic-format=json",
                      str(selfhost_missing)], root)
    try:
        diagnostic_json = json.loads(diagnostic.stderr.strip())
    except json.JSONDecodeError as exception:
        diagnostic_json = {}
        failures.append(f"self-hosted compiler diagnostic is not JSON: {exception}")
    if diagnostic.returncode == 0 or not diagnostic_json.get("code"):
        failures.append("self-hosted compiler did not emit a coded failure diagnostic")

    handoff = root / contract["debugger_handoff"]
    handoff_text = handoff.read_text(encoding="utf-8", errors="replace") if handoff.is_file() else ""
    for marker in ("Visual Studio", "NativeSharp Debugger", "O0", "O3", "PDB"):
        if marker not in handoff_text:
            failures.append(f"debugger handoff omits {marker}")

    names = contract["ctests"]
    expression = "^(" + "|".join(re.escape(name) for name in names) + ")$"
    ctest = run([str(args.ctest), "--test-dir", str(args.build_dir), "-C", "Release",
                 "-R", expression, "--output-on-failure", "-j1"], root)
    if ctest.returncode != 0:
        failures.append(f"diagnostic/debug CTest matrix: {ctest.stdout}{ctest.stderr}")

    report = {
        "schema": "nativesharp-selfhost-developer-experience-report",
        "version": 1,
        "gate": "SH51",
        "tanda": 53,
        "status": "passed" if not failures else "failed",
        "compiler_sha256": digest(args.compiler),
        "selfhost_compiler_sha256": digest(args.selfhost_compiler),
        "contract_sha256": digest(args.contract),
        "observable_cli": len(contract["required_cli"]),
        "persistent_logs": 3,
        "structured_selfhost_diagnostic": bool(diagnostic_json.get("code")),
        "debugger_handoff_sha256": digest(handoff) if handoff.is_file() else None,
        "ctests": {"expected": len(names), "passed": len(names) if ctest.returncode == 0 else 0,
                    "names": names},
        "failures": failures,
    }
    report_path = args.report or output / "sh51-report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8", newline="\n")
    if failures:
        for failure in failures:
            print(f"NSH5102: {failure}", file=sys.stderr)
        return 1
    print(f"SH51 developer experience passed: {len(contract['required_cli'])}/"
          f"{len(contract['required_cli'])} CLI contracts, 3/3 logs, {len(names)}/{len(names)} debug tests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
