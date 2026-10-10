from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

from component_gate_support import component_manifest_rows
from selfhost_inventory import (
    load_component_inventory,
    json_values_equal_exact,
    materialize_contained_project,
    parse_json_value,
    required_oracle_exit_code,
    validate_manifest_cases,
)



def write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8", newline="\n")


def run_process(
    command: list[str], cwd: Path, environment: dict[str, str], timeout: float
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        env=environment,
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=timeout,
        check=False,
    )


def validate_lineage(root: Path, cases: list[dict[str, object]]) -> frozenset[str]:
    inventory = load_component_inventory(root, "SelfHost.Diagnostics")
    manifest_by_id = validate_manifest_cases(inventory, cases)
    if manifest_by_id.keys() != inventory.design_by_id.keys():
        raise RuntimeError("Diagnostics manifest differs from the canonical denominator")
    known = {
        str(row["case_id"])
        for row in cases
        if row.get("expected_outcome") == "KNOWN_FAILURE"
    }
    tagged = {
        str(row["case_id"])
        for row in cases
        if "known-product-defect" in row.get("tags", [])
    }
    if known != tagged:
        raise RuntimeError("Diagnostics defect outcome and tags differ")
    return frozenset(known)


def oracle_satisfied(
    row: dict[str, object], completed: subprocess.CompletedProcess[str]
) -> tuple[bool, str]:
    expected_exit_code = required_oracle_exit_code(row, "SelfHost.Diagnostics")
    oracle = row["oracle"]
    if completed.returncode != expected_exit_code:
        return False, f"exit code {completed.returncode}"
    expected_json = oracle.get("json")
    if expected_json is None:
        return True, ""
    try:
        actual_json = parse_json_value(completed.stdout.strip())
    except json.JSONDecodeError as error:
        return False, f"invalid JSON output: {error.msg}"
    except ValueError as error:
        return False, f"invalid JSON output: {error}"
    if not json_values_equal_exact(expected_json, actual_json):
        return False, f"JSON mismatch: {actual_json!r}"
    return True, ""


def main() -> int:
    """Compile the real NativeSharp fixture and evaluate every Diagnostics case."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--nsc-executable", type=Path, required=True)
    parser.add_argument("--nslink-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    parser.add_argument("--optimization", choices=("O0", "O1", "O2", "O3"), required=True)
    options = parser.parse_args()

    nsc = options.nsc_executable.resolve()
    nslink = options.nslink_executable.resolve()
    if not nsc.is_file() or not nslink.is_file():
        raise RuntimeError("SelfHost.Diagnostics requires explicit NSC and NSLink executables")

    root = options.repository_root.resolve()
    cases = component_manifest_rows(
        options.source_root, root, "SelfHost.Diagnostics"
    )
    known_product_defects = validate_lineage(root, cases)

    work_root = options.work_parent.resolve() / (
        f"selfhost-diagnostics-{options.build_mode.lower()}-{options.optimization.lower()}"
    )
    work_root.mkdir(parents=True, exist_ok=True)
    executable = work_root / "selfhost-diagnostics-tests.exe"
    project = materialize_contained_project(
        root,
        options.source_root / "selfhost" / "modules" / "SelfHostDiagnostics.nsproj",
        work_root / "project",
    )
    environment = dict(os.environ)
    environment["NSC_LINKER"] = str(nslink)
    environment["PATH"] = str(nslink.parent) + os.pathsep + environment.get("PATH", "")
    compile_command = [
        str(nsc),
        "--emit-project-oop-exe",
        str(project),
        f"-{options.optimization}",
        f"--profile={options.build_mode.lower()}",
        "-g0",
        "-o",
        str(executable),
    ]
    write_text(work_root / "compile.command.json", json.dumps(compile_command, indent=2) + "\n")
    compiled = run_process(compile_command, work_root, environment, 900.0)
    write_text(work_root / "compile.stdout.log", compiled.stdout)
    write_text(work_root / "compile.stderr.log", compiled.stderr)
    if compiled.returncode != 0 or not executable.is_file():
        raise RuntimeError(
            f"SelfHost.Diagnostics fixture compilation failed with {compiled.returncode}"
        )

    results: list[dict[str, object]] = []
    failures = 0
    for row in cases:
        case_id = str(row["case_id"])
        started = time.monotonic()
        completed = run_process([str(executable), case_id], work_root, environment, 180.0)
        duration = time.monotonic() - started
        case_root = work_root / "cases" / case_id
        write_text(case_root / "stdout.log", completed.stdout)
        write_text(case_root / "stderr.log", completed.stderr)
        satisfied, detail = oracle_satisfied(row, completed)
        if case_id in known_product_defects:
            status = "UNEXPECTED_PASS" if satisfied else "KNOWN_PRODUCT_DEFECT"
            failures += 1
        else:
            status = "PASS" if satisfied else "FAIL"
            failures += int(not satisfied)
        results.append(
            {
                "case_id": case_id,
                "contract_id": row["contract_id"],
                "status": status,
                "detail": detail,
                "exit_code": completed.returncode,
                "duration_seconds": duration,
            }
        )

    payload = {
        "component": "SelfHost.Diagnostics",
        "build_mode": options.build_mode,
        "optimization": options.optimization,
        "selected": len(cases),
        "required": len(cases) - len(known_product_defects),
        "known_product_defects": len(known_product_defects),
        "failures": failures,
        "results": results,
    }
    write_text(work_root / "results.json", json.dumps(payload, indent=2) + "\n")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"selfhost_diagnostics_gate: error: {error}", file=sys.stderr)
        raise SystemExit(2)
