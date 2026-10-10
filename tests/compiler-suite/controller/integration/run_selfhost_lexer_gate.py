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
    materialize_contained_project,
    required_oracle_exit_code,
    validate_manifest_cases,
)

PROFILE_RANK = {"PR": 0, "NIGHTLY": 1}


def write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8", newline="\n")


def run_process(command: list[str], cwd: Path, environment: dict[str, str], timeout: float) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, env=environment, text=True, encoding="utf-8",
                          errors="replace", capture_output=True, timeout=timeout, check=False)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--nsc-executable", type=Path, required=True)
    parser.add_argument("--nslink-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    parser.add_argument("--optimization", choices=("O0", "O1", "O2", "O3"), required=True)
    parser.add_argument("--profile", choices=("PR", "NIGHTLY"), default="NIGHTLY")
    options = parser.parse_args()

    nsc = options.nsc_executable.resolve()
    nslink = options.nslink_executable.resolve()
    if not nsc.is_file() or not nslink.is_file():
        raise RuntimeError("SelfHost.Lexer requires explicit NSC and NSLink executables")

    root = options.repository_root.resolve()
    inventory = load_component_inventory(root, "SelfHost.Lexer")
    cases = component_manifest_rows(
        options.source_root, root, "SelfHost.Lexer"
    )
    manifest_by_id = validate_manifest_cases(inventory, cases)
    if manifest_by_id.keys() != inventory.design_by_id.keys():
        raise RuntimeError("SelfHost.Lexer manifest differs from the canonical denominator")
    known = {str(row["case_id"]) for row in cases if row.get("expected_outcome") == "KNOWN_FAILURE"}
    tagged = {
        str(row["case_id"])
        for row in cases
        if "known-product-defect" in row.get("tags", [])
    }
    if known != tagged:
        raise RuntimeError("SelfHost.Lexer defect outcome and tags differ")
    known_product_defects = frozenset(known)

    selected = [row for row in cases if PROFILE_RANK[str(row["profile"])] <= PROFILE_RANK[options.profile]]
    work_root = options.work_parent.resolve() / f"selfhost-lexer-{options.build_mode.lower()}-{options.optimization.lower()}"
    work_root.mkdir(parents=True, exist_ok=True)
    executable = work_root / "selfhost-lexer-tests.exe"
    project = materialize_contained_project(
        root,
        options.source_root / "selfhost" / "modules" / "SelfHostLexer.nsproj",
        work_root / "project",
    )
    environment = dict(os.environ)
    environment["NSC_LINKER"] = str(nslink)
    environment["PATH"] = str(nslink.parent) + os.pathsep + environment.get("PATH", "")
    command = [str(nsc), "--emit-project-oop-exe", str(project), f"-{options.optimization}",
               f"--profile={options.build_mode.lower()}", "-g0", "-o", str(executable)]
    write_text(work_root / "compile.command.json", json.dumps(command, indent=2) + "\n")
    compiled = run_process(command, work_root, environment, 900.0)
    write_text(work_root / "compile.stdout.log", compiled.stdout)
    write_text(work_root / "compile.stderr.log", compiled.stderr)
    if compiled.returncode != 0 or not executable.is_file():
        raise RuntimeError(f"SelfHost.Lexer fixture compilation failed with {compiled.returncode}")

    failures = 0
    results: list[dict[str, object]] = []
    for row in selected:
        case_id = str(row["case_id"])
        started = time.monotonic()
        completed = run_process([str(executable), case_id], work_root, environment, 180.0)
        duration = time.monotonic() - started
        case_root = work_root / "cases" / case_id
        write_text(case_root / "stdout.log", completed.stdout)
        write_text(case_root / "stderr.log", completed.stderr)
        passed = completed.returncode == required_oracle_exit_code(
            row, "SelfHost.Lexer"
        )
        if case_id in known_product_defects:
            status = "UNEXPECTED_PASS" if passed else "KNOWN_PRODUCT_DEFECT"
            failures += 1
        else:
            status = "PASS" if passed else "FAIL"
            failures += int(not passed)
        results.append({"case_id": case_id, "contract_id": row["contract_id"], "status": status,
                        "exit_code": completed.returncode, "duration_seconds": duration})

    payload = {"component": "SelfHost.Lexer", "profile": options.profile, "selected": len(selected),
               "required": len(selected) - sum(str(row["case_id"]) in known_product_defects for row in selected),
               "known_product_defects": sum(str(row["case_id"]) in known_product_defects for row in selected),
               "failures": failures, "results": results}
    write_text(work_root / "results.json", json.dumps(payload, indent=2) + "\n")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"selfhost_lexer_gate: error: {error}", file=sys.stderr)
        raise SystemExit(2)
