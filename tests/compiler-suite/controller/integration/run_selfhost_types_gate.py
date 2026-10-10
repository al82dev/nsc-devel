from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
import sys
import time
from pathlib import Path


ALL_IDS = tuple(f"TSH-{ordinal:05d}" for ordinal in range(981, 1028))
PLAN_CONTRADICTIONS = frozenset(
    {
        "TSH-00983",
        "TSH-00984",
        "TSH-00991",
        "TSH-00996",
        "TSH-00997",
        "TSH-01003",
        "TSH-01004",
        "TSH-01017",
        "TSH-01018",
        "TSH-01024",
    }
)
INFRASTRUCTURE_PENDING = frozenset({"TSH-01014"})
KNOWN_PRODUCT_DEFECTS = frozenset({"TSH-01011", "TSH-01023"})
EXECUTABLE_IDS = tuple(
    case_id
    for case_id in ALL_IDS
    if case_id not in PLAN_CONTRADICTIONS | INFRASTRUCTURE_PENDING
)
PROFILE_RANK = {"PR": 0, "NIGHTLY": 1}


def read_jsonl(path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8-sig").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            payload = json.loads(line)
        except json.JSONDecodeError as error:
            raise RuntimeError(
                f"invalid JSON at {path}:{line_number}: {error.msg}"
            ) from error
        if not isinstance(payload, dict):
            raise RuntimeError(f"expected JSON object at {path}:{line_number}")
        rows.append(payload)
    return rows


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


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
        if row["component"] == "SelfHost.Types"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Types"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Types"
    ]
    if tuple(row["case_id"] for row in design) != ALL_IDS:
        raise RuntimeError("design does not contain canonical TSH-00981..TSH-01027")
    if tuple(row["case_id"] for row in allocation) != ALL_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00981..TSH-01027")
    if len(contracts) != 7:
        raise RuntimeError("contract inventory does not contain seven Types contracts")
    if sum(row["default_profile"] == "PR" for row in allocation) != 46:
        raise RuntimeError("Types allocation no longer contains exactly 46 PR cases")
    if sum(row["default_profile"] == "NIGHTLY" for row in allocation) != 1:
        raise RuntimeError("Types allocation no longer contains exactly one NIGHTLY case")
    if sum(row["layer"] == "unit" for row in allocation) != 41:
        raise RuntimeError("Types allocation no longer contains exactly 41 unit cases")
    if sum(row["layer"] == "component" for row in allocation) != 6:
        raise RuntimeError("Types allocation no longer contains exactly six component cases")
    if tuple(str(row["case_id"]) for row in cases) != EXECUTABLE_IDS:
        raise RuntimeError("central manifest does not contain the exact executable Types partition")
    if any(row.get("component") != "SelfHost.Types" for row in cases):
        raise RuntimeError("filtered Types manifest contains a foreign component")


def validate_classifications(root: Path, cases: list[dict[str, object]]) -> None:
    path = (
        root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostTypesCaseClassifications.jsonl"
    )
    rows = read_jsonl(path)
    by_disposition: dict[str, set[str]] = {}
    for row in rows:
        disposition = str(row["disposition"])
        by_disposition.setdefault(disposition, set()).add(str(row["case_id"]))
    if by_disposition.get("PlanContradiction", set()) != PLAN_CONTRADICTIONS:
        raise RuntimeError("Types plan-contradiction catalogue changed")
    if by_disposition.get("InfrastructurePending", set()) != INFRASTRUCTURE_PENDING:
        raise RuntimeError("Types InfrastructurePending catalogue changed")
    if by_disposition.get("KnownProductDefect", set()) != KNOWN_PRODUCT_DEFECTS:
        raise RuntimeError("Types known-product-defect catalogue changed")
    catalogued = set().union(*by_disposition.values())
    if catalogued - set(ALL_IDS):
        raise RuntimeError("Types classification catalogue contains a foreign case")
    executable = {str(row["case_id"]) for row in cases}
    if executable | PLAN_CONTRADICTIONS | INFRASTRUCTURE_PENDING != set(ALL_IDS):
        raise RuntimeError("Types denominator contains omissions")
    if executable & (PLAN_CONTRADICTIONS | INFRASTRUCTURE_PENDING):
        raise RuntimeError("non-executable Types cases entered the central manifest")
    tagged_defects = {
        str(row["case_id"])
        for row in cases
        if "known-product-defect" in row.get("tags", [])
    }
    if tagged_defects != KNOWN_PRODUCT_DEFECTS:
        raise RuntimeError("Types executable defect tags changed")


def evaluate_case(
    row: dict[str, object], completed: subprocess.CompletedProcess[str]
) -> tuple[bool, str]:
    oracle = row.get("oracle")
    if not isinstance(oracle, dict):
        return False, "manifest oracle is not an object"
    expected = int(oracle["exit_code"])
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
    validate_lineage(repository_root, cases)
    validate_classifications(repository_root, cases)
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
        "component": "SelfHost.Types",
        "build_mode": options.build_mode,
        "optimization": options.optimization,
        "profile": options.profile,
        "canonical_denominator": len(ALL_IDS),
        "selected_executable": len(selected),
        "required": len(EXECUTABLE_IDS) - len(KNOWN_PRODUCT_DEFECTS),
        "known_product_defects": len(KNOWN_PRODUCT_DEFECTS),
        "infrastructure_pending": len(INFRASTRUCTURE_PENDING),
        "plan_contradictions": len(PLAN_CONTRADICTIONS),
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
