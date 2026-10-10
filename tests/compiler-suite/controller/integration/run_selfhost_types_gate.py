from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

from selfhost_inventory import (
    ComponentInventory,
    load_component_inventory,
    read_jsonl,
    required_oracle_exit_code,
    validate_classified_manifest,
)

PROFILE_RANK = {"PR": 0, "NIGHTLY": 1}


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


def validate_inventory(
    root: Path,
    cases: list[dict[str, object]],
) -> tuple[ComponentInventory, dict[str, frozenset[str]]]:
    inventory = load_component_inventory(root, "SelfHost.Types")
    path = (
        root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostTypesCaseClassifications.jsonl"
    )
    _manifest, classifications = validate_classified_manifest(
        inventory,
        cases,
        path,
        frozenset({"PlanContradiction", "InfrastructurePending"}),
    )
    return inventory, classifications


def evaluate_case(
    row: dict[str, object], completed: subprocess.CompletedProcess[str]
) -> tuple[bool, str]:
    expected = required_oracle_exit_code(row, "SelfHost.Types")
    if completed.returncode != expected:
        return False, f"exit code {completed.returncode}; expected {expected}"
    return True, ""


def main() -> int:
    """Build the real NativeSharp fixture and run every selected case in isolation."""
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
        raise RuntimeError("SelfHost.Types requires explicit NSC and NSLink executables")

    repository_root = options.repository_root.resolve()
    source_root = options.source_root.resolve()
    retired_fragment = source_root / "manifest" / "fragments" / "selfhost-types.jsonl"
    if retired_fragment.exists():
        raise RuntimeError("retired SelfHost.Types manifest fragment still exists")
    manifest = source_root / "manifest" / "cases.jsonl"
    cases = [
        row
        for row in read_jsonl(manifest)
        if row.get("component") == "SelfHost.Types"
    ]
    inventory, classifications = validate_inventory(repository_root, cases)
    known_product_defects = classifications.get("KnownProductDefect", frozenset())
    infrastructure_pending = classifications.get("InfrastructurePending", frozenset())
    plan_contradictions = classifications.get("PlanContradiction", frozenset())
    selected = [
        row
        for row in cases
        if PROFILE_RANK[str(row["profile"])] <= PROFILE_RANK[options.profile]
    ]

    work_root = options.work_parent.resolve() / (
        f"selfhost-types-{options.build_mode.lower()}-{options.optimization.lower()}"
    )
    work_root.mkdir(parents=True, exist_ok=True)
    executable = work_root / "selfhost-types-tests.exe"
    project = source_root / "selfhost" / "modules" / "SelfHostTypes.nsproj"
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
        raise RuntimeError(f"SelfHost.Types fixture compilation failed with {compiled.returncode}")

    results: list[dict[str, object]] = []
    failures = 0
    for row in selected:
        case_id = str(row["case_id"])
        case_root = work_root / "cases" / case_id
        case_root.mkdir(parents=True, exist_ok=True)
        started = time.monotonic()
        completed = run_process([str(executable), case_id], case_root, environment, 180.0)
        duration = time.monotonic() - started
        write_text(case_root / "stdout.log", completed.stdout)
        write_text(case_root / "stderr.log", completed.stderr)
        satisfied, detail = evaluate_case(row, completed)
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
        "component": "SelfHost.Types",
        "build_mode": options.build_mode,
        "optimization": options.optimization,
        "profile": options.profile,
        "canonical_denominator": len(inventory.case_ids),
        "selected_executable": len(selected),
        "required": len(cases) - len(known_product_defects),
        "known_product_defects": len(known_product_defects),
        "infrastructure_pending": len(infrastructure_pending),
        "plan_contradictions": len(plan_contradictions),
        "failures": failures,
        "results": results,
    }
    write_text(work_root / "results.json", json.dumps(payload, indent=2) + "\n")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"selfhost_types_gate: error: {error}", file=sys.stderr)
        raise SystemExit(2)
