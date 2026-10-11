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
    validate_executable_manifest,
)

PROFILE_RANK = {"PR": 0, "NIGHTLY": 1}


def write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8", newline="\n")


def run_process(
    command: list[str], cwd: Path, environment: dict[str, str]
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        env=environment,
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        check=False,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--nsc-executable", type=Path, required=True)
    parser.add_argument("--nslink-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    parser.add_argument("--optimization", choices=("O0", "O1", "O2", "O3"), required=True)
    parser.add_argument("--profile", choices=("PR", "NIGHTLY"), default="NIGHTLY")
    options = parser.parse_args()

    nsc = options.nsc_executable.resolve()
    nslink = options.nslink_executable.resolve()
    if not nsc.is_file() or not nslink.is_file():
        raise RuntimeError("SelfHost.Basic requires explicit existing NSC and NSLink executables")

    source_root = options.source_root.resolve()
    repository_root = source_root.parents[1]
    inventory = load_component_inventory(repository_root, "SelfHost.Basic")
    cases = component_manifest_rows(
        source_root, repository_root, "SelfHost.Basic"
    )
    validate_executable_manifest(inventory, cases)

    selected = [
        row
        for row in cases
        if PROFILE_RANK[str(row["profile"])] <= PROFILE_RANK[options.profile]
    ]
    work_root = options.work_parent.resolve() / (
        f"selfhost-basic-{options.build_mode.lower()}-{options.optimization.lower()}"
    )
    work_root.mkdir(parents=True, exist_ok=True)
    executable = work_root / "selfhost-basic-tests.exe"
    project = materialize_contained_project(
        repository_root,
        source_root / "selfhost" / "modules" / "SelfHostBasic.nsproj",
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
    compiled = run_process(compile_command, work_root, environment)
    write_text(work_root / "compile.stdout.log", compiled.stdout)
    write_text(work_root / "compile.stderr.log", compiled.stderr)
    if compiled.returncode != 0 or not executable.is_file():
        raise RuntimeError(f"SelfHost.Basic fixture compilation failed with {compiled.returncode}")

    results: list[dict[str, object]] = []
    failed = 0
    for row in selected:
        case_id = str(row["case_id"])
        started = time.monotonic()
        completed = run_process([str(executable), case_id], work_root, environment)
        duration = time.monotonic() - started
        case_root = work_root / "cases" / case_id
        write_text(case_root / "stdout.log", completed.stdout)
        write_text(case_root / "stderr.log", completed.stderr)
        passed = completed.returncode == required_oracle_exit_code(
            row, "SelfHost.Basic"
        )
        if not passed:
            failed += 1
        results.append(
            {
                "case_id": case_id,
                "contract_id": row["contract_id"],
                "status": "PASS" if passed else "FAIL",
                "exit_code": completed.returncode,
                "duration_seconds": duration,
            }
        )

    payload = {
        "component": "SelfHost.Basic",
        "build_mode": options.build_mode,
        "optimization": options.optimization,
        "profile": options.profile,
        "selected": len(selected),
        "passed": len(selected) - failed,
        "failed": failed,
        "results": results,
    }
    write_text(work_root / "results.json", json.dumps(payload, indent=2) + "\n")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"selfhost_basic_gate: error: {error}", file=sys.stderr)
        raise SystemExit(2)
