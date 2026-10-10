from __future__ import annotations

import argparse
import csv
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


ALL_IDS = (
    *(f"T12B-{ordinal:05d}" for ordinal in range(457, 469)),
    *(f"T12B-{ordinal:05d}" for ordinal in range(1069, 1075)),
)
DEPLOY_BATCH_IDS = tuple(f"T12B-{ordinal:05d}" for ordinal in range(457, 463))
DEPLOY_GUI_IDS = tuple(f"T12B-{ordinal:05d}" for ordinal in range(463, 469))
PACKAGE_IDS = tuple(f"T12B-{ordinal:05d}" for ordinal in range(1069, 1075))
EXECUTABLE_IDS = DEPLOY_BATCH_IDS + PACKAGE_IDS
INFRASTRUCTURE_PENDING = frozenset(DEPLOY_GUI_IDS)
KNOWN_PRODUCT_DEFECTS = frozenset(
    {"T12B-00457", "T12B-00458", "T12B-00460", "T12B-00461"}
)


def read_jsonl(path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            payload = json.loads(line)
        except json.JSONDecodeError as error:
            raise RuntimeError(f"invalid JSON at {path}:{line_number}: {error.msg}") from error
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
    command: list[str], cwd: Path, environment: dict[str, str], timeout: float = 900.0
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


def log_process(case_root: Path, stem: str, completed: subprocess.CompletedProcess[str]) -> None:
    write_text(case_root / f"{stem}.stdout.log", completed.stdout)
    write_text(case_root / f"{stem}.stderr.log", completed.stderr)


def validate_lineage(repository_root: Path, cases: list[dict[str, object]]) -> None:
    inventory = repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    design = [row for row in read_csv(inventory / "test-case-design.csv") if row["component"] == "Packaging"]
    allocation = [
        row for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "Packaging"
    ]
    contracts = [row for row in read_csv(inventory / "contracts.csv") if row["component"] == "Packaging"]
    if tuple(row["case_id"] for row in design) != ALL_IDS:
        raise RuntimeError("Packaging design no longer contains the exact canonical 18 cases")
    if tuple(row["case_id"] for row in allocation) != ALL_IDS:
        raise RuntimeError("Packaging allocation no longer contains the exact canonical 18 cases")
    if len(contracts) != 3:
        raise RuntimeError("Packaging contract inventory no longer contains three contracts")
    if {row["default_profile"] for row in allocation} != {"NIGHTLY"}:
        raise RuntimeError("Packaging allocation no longer assigns every case to NIGHTLY")
    if tuple(str(row["case_id"]) for row in cases) != EXECUTABLE_IDS:
        raise RuntimeError("manifest does not contain the exact 12-case executable partition")


def validate_classifications(repository_root: Path, cases: list[dict[str, object]]) -> None:
    component_root = repository_root / "tests" / "compiler-suite" / "packaging"
    catalog = read_jsonl(component_root / "PackagingContractCatalog.jsonl")
    classified = read_jsonl(component_root / "PackagingCaseClassifications.jsonl")
    contradictions = read_jsonl(component_root / "PackagingPlanContradictions.jsonl")
    if {str(row["contract_id"]) for row in catalog} != {
        "C12B-PKG-001", "C12B-PKG-002", "C12B-PKG-003"
    }:
        raise RuntimeError("Packaging contract catalogue is incomplete")
    infrastructure = {
        str(row["case_id"])
        for row in classified
        if row["disposition"] == "InfrastructurePending"
    }
    defects = {
        str(row["case_id"])
        for row in classified
        if row["disposition"] == "KnownProductDefect"
    }
    if infrastructure != INFRASTRUCTURE_PENDING:
        raise RuntimeError("Packaging infrastructure-pending partition changed")
    if defects != KNOWN_PRODUCT_DEFECTS:
        raise RuntimeError("Packaging known-product-defect partition changed")
    if len(contradictions) != 1:
        raise RuntimeError("Packaging profile contradiction is not preserved")
    executable = {str(row["case_id"]) for row in cases}
    if executable | infrastructure != set(ALL_IDS) or executable & infrastructure:
        raise RuntimeError("Packaging denominator is incomplete or overlapping")
    tagged = {
        str(row["case_id"])
        for row in cases
        if "known-product-defect" in row.get("tags", [])
    }
    if tagged != KNOWN_PRODUCT_DEFECTS:
        raise RuntimeError("Packaging executable KPD tags changed")


def cmake_shim(case_root: Path) -> tuple[Path, Path]:
    shim_root = case_root / "shim"
    shim_root.mkdir(parents=True, exist_ok=True)
    log = case_root / "cmake-arguments.txt"
    shim = shim_root / "cmake.cmd"
    write_text(shim, "@echo off\r\n>\"%NSC_PACKAGING_CMAKE_LOG%\" echo %*\r\nexit /b 0\r\n")
    return shim_root, log


def run_deploy_batch(
    repository_root: Path,
    case_root: Path,
    variant: int,
    environment: dict[str, str],
) -> tuple[bool, str]:
    shim_root, log = cmake_shim(case_root)
    selected_build = case_root / ("build unicode" if variant == 2 else "build")
    selected_prefix = case_root / ("prefix unicode" if variant == 2 else "prefix")
    if variant == 4:
        selected_prefix = Path(selected_prefix.anchor)
    if variant == 5:
        selected_prefix = selected_build
    command = [
        os.environ.get("COMSPEC", r"C:\Windows\System32\cmd.exe"),
        "/d", "/c", str(repository_root / "deploy.bat"),
        "--build", str(selected_build), "--prefix", str(selected_prefix),
    ]

    def invoke(stem: str, dependency_present: bool) -> subprocess.CompletedProcess[str]:
        if log.exists():
            log.unlink()
        local_environment = dict(environment)
        local_environment["NSC_PACKAGING_CMAKE_LOG"] = str(log)
        system_root = Path(os.environ.get("SystemRoot", r"C:\Windows"))
        path_entries = [str(system_root / "System32")]
        if dependency_present:
            path_entries.insert(0, str(shim_root))
        local_environment["PATH"] = os.pathsep.join(path_entries)
        completed = run_process(command, case_root, local_environment)
        log_process(case_root, stem, completed)
        if log.exists():
            write_text(case_root / f"{stem}.arguments.log", log.read_text(encoding="utf-8"))
        return completed

    if variant == 3:
        completed = invoke("missing-cmake", False)
        return completed.returncode != 0 and not log.exists(), "missing CMake must fail before deployment"
    if variant == 6:
        first = invoke("first", True)
        first_arguments = log.read_text(encoding="utf-8") if log.exists() else ""
        second = invoke("second", True)
        second_arguments = log.read_text(encoding="utf-8") if log.exists() else ""
        return (
            first.returncode == 0
            and second.returncode == 0
            and first_arguments == second_arguments
        ), "deploy wrapper command differs across repetitions"

    completed = invoke("deploy", True)
    arguments = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
    selected = str(selected_build) in arguments and str(selected_prefix) in arguments
    if variant in {4, 5}:
        return completed.returncode != 0 and not log.exists(), "invalid path structure was not rejected"
    return completed.returncode == 0 and selected, "deploy wrapper ignored caller-selected paths"


def stage_installation(installation_root: Path, case_root: Path) -> Path:
    prefix = case_root / "installation"
    shutil.copytree(installation_root, prefix)
    return prefix


def package_command(
    python_executable: Path,
    script: Path,
    prefix: Path,
    archive_executable: Path,
    output: Path,
    variant: int,
    dry_run: bool,
) -> list[str]:
    timestamp = "20261009-000000"
    compression = "9"
    if variant == 4:
        compression = "10"
    if variant == 5:
        timestamp = "..."
    command = [
        str(python_executable), "-B", str(script),
        "--prefix", str(prefix),
        "--seven-zip", str(archive_executable),
        "--output-dir", str(output),
        "--compression", compression,
        "--timestamp", timestamp,
    ]
    if dry_run:
        command.append("--dry-run")
    return command


def run_package_native_sharp(
    options: argparse.Namespace,
    case_root: Path,
    variant: int,
    environment: dict[str, str],
) -> tuple[bool, str]:
    prefix = stage_installation(options.installation_root, case_root)
    output = case_root / "output"
    script = options.repository_root / "package_native_sharp.py"
    if variant == 3:
        compiler = prefix / "bin" / "nsc.exe"
        if compiler.exists():
            compiler.unlink()
    dry_run = variant != 2
    command = package_command(
        options.python_executable,
        script,
        prefix,
        options.archive_executable,
        output,
        variant,
        dry_run,
    )

    def invoke(stem: str) -> subprocess.CompletedProcess[str]:
        completed = run_process(command, case_root, environment)
        log_process(case_root, stem, completed)
        return completed

    first = invoke("first")
    archives = tuple(output.glob("*.7z")) if output.exists() else ()
    partials = tuple(output.glob("*.partial")) if output.exists() else ()
    if variant == 3:
        return (
            first.returncode != 0
            and "NativeSharp compiler not found" in first.stderr
            and not archives
            and not partials
        ), "missing compiler must fail without an archive"
    if variant == 4:
        return (
            first.returncode != 0
            and "compression must be between 0 and 9" in first.stderr
            and not archives
            and not partials
        ), "invalid compression must fail without an archive"
    if variant == 5:
        return (
            first.returncode != 0
            and "timestamp is empty" in first.stderr
            and not archives
            and not partials
        ), "empty filename timestamp must fail without an archive"
    if variant == 6:
        second = invoke("second")
        return (
            first.returncode == 0
            and second.returncode == 0
            and first.stdout == second.stdout
            and first.stderr == second.stderr
            and not archives
            and not partials
        ), "fixed-input dry runs differ"
    if variant == 1:
        return (
            first.returncode == 0
            and "Dry run: no archive was created." in first.stdout
            and "Version" in first.stdout
            and "Build hash" in first.stdout
            and not archives
            and not partials
        ), "minimal dry run did not report the exact identity contract"
    return (
        first.returncode == 0
        and len(archives) == 1
        and not partials
        and f"Package created: {archives[0]}" in first.stdout
    ), "representative packaging did not publish one verified archive"


def run_case(
    row: dict[str, object],
    options: argparse.Namespace,
    case_root: Path,
    environment: dict[str, str],
) -> tuple[bool, str]:
    inputs = row["inputs"]
    if not isinstance(inputs, dict):
        return False, "manifest inputs are not an object"
    contract = int(inputs["contract_ordinal"])
    variant = int(inputs["variant"])
    if contract == 1:
        return run_deploy_batch(options.repository_root, case_root, variant, environment)
    if contract == 3:
        return run_package_native_sharp(options, case_root, variant, environment)
    return False, f"unsupported executable Packaging contract {contract}"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--python-executable", type=Path, required=True)
    parser.add_argument("--archive-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--installation-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    options = parser.parse_args()
    options.python_executable = options.python_executable.resolve()
    options.archive_executable = options.archive_executable.resolve()
    options.source_root = options.source_root.resolve()
    options.repository_root = options.repository_root.resolve()
    options.installation_root = options.installation_root.resolve()
    if not options.python_executable.is_file():
        raise RuntimeError("Packaging requires an explicit Python executable")
    if not options.archive_executable.is_file():
        raise RuntimeError("Packaging requires an explicit archive executable")
    if not (options.installation_root / "bin" / "nsc.exe").is_file():
        raise RuntimeError("Packaging requires an explicit sandbox-copyable installation root")

    manifest = read_jsonl(options.source_root / "manifest" / "cases.jsonl")
    cases = [row for row in manifest if row.get("component") == "Packaging"]
    validate_lineage(options.repository_root, cases)
    validate_classifications(options.repository_root, cases)
    work_root = options.work_parent.resolve() / f"packaging-{options.build_mode.lower()}"
    work_root.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    results: list[dict[str, object]] = []
    failures = 0
    for row in cases:
        case_id = str(row["case_id"])
        case_root = work_root / "cases" / case_id
        if case_root.exists():
            shutil.rmtree(case_root)
        case_root.mkdir(parents=True)
        started = time.monotonic()
        satisfied, detail = run_case(row, options, case_root, environment)
        duration = time.monotonic() - started
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
                "duration_seconds": duration,
            }
        )
    payload = {
        "component": "Packaging",
        "build_mode": options.build_mode,
        "canonical_denominator": len(ALL_IDS),
        "new_suite_executable": len(cases),
        "required": len(cases) - len(KNOWN_PRODUCT_DEFECTS),
        "known_product_defects": len(KNOWN_PRODUCT_DEFECTS),
        "infrastructure_pending": len(INFRASTRUCTURE_PENDING),
        "failures": failures,
        "results": results,
    }
    write_text(work_root / "results.json", json.dumps(payload, indent=2) + "\n")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"packaging_gate: error: {error}", file=sys.stderr)
        raise SystemExit(2)
