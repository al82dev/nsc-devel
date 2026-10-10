from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
import sys
import time
from pathlib import Path


EXPECTED_IDS = tuple(f"TSH-{ordinal:05d}" for ordinal in range(188, 216))
KNOWN_PRODUCT_DEFECTS = frozenset({"TSH-00213", "TSH-00214"})


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def read_jsonl(path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8-sig").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        payload = json.loads(line)
        if not isinstance(payload, dict):
            raise RuntimeError(f"manifest row {path}:{line_number} is not an object")
        rows.append(payload)
    return rows


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


def validate_lineage(root: Path, cases: list[dict[str, object]]) -> None:
    inventory = root / "docs" / "compiler-testing-reengineering" / "inventory"
    design = [
        row
        for row in read_csv(inventory / "test-case-design.csv")
        if row["component"] == "SelfHost.Diagnostics"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Diagnostics"
    ]
    if tuple(row["case_id"] for row in design) != EXPECTED_IDS:
        raise RuntimeError("design does not contain canonical TSH-00188..TSH-00215")
    if len(contracts) != 4:
        raise RuntimeError("contract inventory does not contain four Diagnostics contracts")
    if tuple(str(row["case_id"]) for row in cases) != EXPECTED_IDS:
        raise RuntimeError("manifest does not contain the canonical 28 Diagnostics cases")
    if len({str(row["case_id"]) for row in cases}) != 28:
        raise RuntimeError("Diagnostics manifest contains duplicate case identifiers")
    known = {
        str(row["case_id"])
        for row in cases
        if row.get("expected_outcome") == "KNOWN_FAILURE"
    }
    if known != KNOWN_PRODUCT_DEFECTS:
        raise RuntimeError("Diagnostics known-product-defect classification changed")


def oracle_satisfied(
    row: dict[str, object], completed: subprocess.CompletedProcess[str]
) -> tuple[bool, str]:
    oracle = row["oracle"]
    if not isinstance(oracle, dict):
        return False, "manifest oracle is not an object"
    if completed.returncode != int(oracle["exit_code"]):
        return False, f"exit code {completed.returncode}"
    expected_json = oracle.get("json")
    if expected_json is None:
        return True, ""
    try:
        actual_json = json.loads(completed.stdout.strip())
    except json.JSONDecodeError as error:
        return False, f"invalid JSON output: {error.msg}"
    if actual_json != expected_json:
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
    manifest = options.source_root / "manifest" / "cases.jsonl"
    cases = [
        row
        for row in read_jsonl(manifest)
        if row.get("component") == "SelfHost.Diagnostics"
    ]
    validate_lineage(root, cases)

    work_root = options.work_parent.resolve() / (
        f"selfhost-diagnostics-{options.build_mode.lower()}-{options.optimization.lower()}"
    )
    work_root.mkdir(parents=True, exist_ok=True)
    executable = work_root / "selfhost-diagnostics-tests.exe"
    project = options.source_root / "selfhost" / "modules" / "SelfHostDiagnostics.nsproj"
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
        if case_id in KNOWN_PRODUCT_DEFECTS:
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
        "required": len(cases) - len(KNOWN_PRODUCT_DEFECTS),
        "known_product_defects": len(KNOWN_PRODUCT_DEFECTS),
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
