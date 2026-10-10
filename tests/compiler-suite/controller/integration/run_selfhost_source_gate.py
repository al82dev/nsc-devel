from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
import sys
import time
from pathlib import Path


ALL_IDS = tuple(f"TSH-{ordinal:05d}" for ordinal in range(849, 910))
PLAN_CONTRADICTIONS = frozenset(
    {
        "TSH-00851",
        "TSH-00863",
        "TSH-00870",
        "TSH-00874",
        "TSH-00875",
        "TSH-00881",
        "TSH-00889",
        "TSH-00892",
        "TSH-00896",
        "TSH-00897",
        "TSH-00904",
    }
)
INFRASTRUCTURE_PENDING = frozenset({"TSH-00885", "TSH-00908"})
KNOWN_PRODUCT_DEFECTS = frozenset({"TSH-00903"})
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
        if row["component"] == "SelfHost.Source"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "SelfHost.Source"
    ]
    contracts = [
        row
        for row in read_csv(inventory / "contracts.csv")
        if row["component"] == "SelfHost.Source"
    ]
    if tuple(row["case_id"] for row in design) != ALL_IDS:
        raise RuntimeError("design does not contain canonical TSH-00849..TSH-00909")
    if tuple(row["case_id"] for row in allocation) != ALL_IDS:
        raise RuntimeError("allocation does not contain canonical TSH-00849..TSH-00909")
    if len(contracts) != 8:
        raise RuntimeError("contract inventory does not contain eight Source contracts")
    if sum(row["default_profile"] == "PR" for row in allocation) != 51:
        raise RuntimeError("Source allocation no longer contains exactly 51 PR cases")
    if sum(row["default_profile"] == "NIGHTLY" for row in allocation) != 10:
        raise RuntimeError("Source allocation no longer contains exactly ten NIGHTLY cases")
    if sum(row["layer"] == "unit" for row in allocation) != 36:
        raise RuntimeError("Source allocation no longer contains exactly 36 unit cases")
    if sum(row["layer"] == "component" for row in allocation) != 25:
        raise RuntimeError("Source allocation no longer contains exactly 25 component cases")
    if tuple(str(row["case_id"]) for row in cases) != EXECUTABLE_IDS:
        raise RuntimeError("central manifest does not contain the exact executable Source partition")
    if any(row.get("component") != "SelfHost.Source" for row in cases):
        raise RuntimeError("filtered Source manifest contains a foreign component")


def validate_classifications(root: Path, cases: list[dict[str, object]]) -> None:
    path = (
        root
        / "tests"
        / "compiler-suite"
        / "selfhost"
        / "modules"
        / "SelfHostSourceCaseClassifications.jsonl"
    )
    rows = read_jsonl(path)
    by_disposition: dict[str, set[str]] = {}
    for row in rows:
        disposition = str(row["disposition"])
        by_disposition.setdefault(disposition, set()).add(str(row["case_id"]))
    if by_disposition.get("PlanContradiction", set()) != PLAN_CONTRADICTIONS:
        raise RuntimeError("Source plan-contradiction catalogue changed")
    if by_disposition.get("InfrastructurePending", set()) != INFRASTRUCTURE_PENDING:
        raise RuntimeError("Source InfrastructurePending catalogue changed")
    if by_disposition.get("KnownProductDefect", set()) != KNOWN_PRODUCT_DEFECTS:
        raise RuntimeError("Source known-product-defect catalogue changed")
    catalogued = set().union(*by_disposition.values())
    if catalogued - set(ALL_IDS):
        raise RuntimeError("Source classification catalogue contains a foreign case")
    executable = {str(row["case_id"]) for row in cases}
    if executable | PLAN_CONTRADICTIONS | INFRASTRUCTURE_PENDING != set(ALL_IDS):
        raise RuntimeError("Source denominator contains omissions")
    if executable & (PLAN_CONTRADICTIONS | INFRASTRUCTURE_PENDING):
        raise RuntimeError("non-executable Source cases entered the central manifest")
    tagged_defects = {
        str(row["case_id"])
        for row in cases
        if "known-product-defect" in row.get("tags", [])
    }
    if tagged_defects != KNOWN_PRODUCT_DEFECTS:
        raise RuntimeError("Source executable defect tags changed")


def prepare_case_files(case_root: Path) -> None:
    # Bytes are explicit so CR/LF normalization is tested rather than Python newline policy.
    (case_root / "source-empty.ns").write_bytes(b"")
    (case_root / "source-input.ns").write_bytes(b"alpha\r\nbeta\rgamma\n")
    (case_root / "source-unicode.ns").write_bytes("á\r\nβ".encode("utf-8"))
    (case_root / "source-frontend.ns").write_bytes(
        b"comptime int value = #run(2 + 3);\n"
    )
    (case_root / "source-large.ns").write_bytes(b"x\r\n" * 32768)


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
    """Build the real Source fixture once and run every selected case in isolation."""
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
        raise RuntimeError("SelfHost.Source requires explicit NSC and NSLink executables")

    repository_root = options.repository_root.resolve()
    source_root = options.source_root.resolve()
    retired_fragment = source_root / "manifest" / "fragments" / "selfhost-source.jsonl"
    if retired_fragment.exists():
        raise RuntimeError("retired SelfHost.Source manifest fragment still exists")
    manifest = source_root / "manifest" / "cases.jsonl"
    cases = [
        row
        for row in read_jsonl(manifest)
        if row.get("component") == "SelfHost.Source"
    ]
    validate_lineage(repository_root, cases)
    validate_classifications(repository_root, cases)
    selected = [
        row
        for row in cases
        if PROFILE_RANK[str(row["profile"])] <= PROFILE_RANK[options.profile]
    ]

    work_root = options.work_parent.resolve() / (
        f"selfhost-source-{options.build_mode.lower()}-{options.optimization.lower()}"
    )
    work_root.mkdir(parents=True, exist_ok=True)
    executable = work_root / "selfhost-source-tests.exe"
    project = source_root / "selfhost" / "modules" / "SelfHostSource.nsproj"
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
        raise RuntimeError(f"SelfHost.Source fixture compilation failed with {compiled.returncode}")

    results: list[dict[str, object]] = []
    failures = 0
    for row in selected:
        case_id = str(row["case_id"])
        case_root = work_root / "cases" / case_id
        case_root.mkdir(parents=True, exist_ok=True)
        prepare_case_files(case_root)
        started = time.monotonic()
        completed = run_process(
            [str(executable), case_id, str(case_root)], case_root, environment, 180.0
        )
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
        "component": "SelfHost.Source",
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
        print(f"selfhost_source_gate: error: {error}", file=sys.stderr)
        raise SystemExit(2)
