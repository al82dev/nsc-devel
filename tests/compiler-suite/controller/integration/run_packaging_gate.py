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

from component_gate_support import (
    read_jsonl,
    required_manifest_integer,
    required_nonnegative_integer,
)


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8", newline="\n")


def unique_map(
    rows: list[dict[str, object]] | list[dict[str, str]],
    key: str,
    source: str,
) -> dict[str, dict[str, object] | dict[str, str]]:
    result: dict[str, dict[str, object] | dict[str, str]] = {}
    for row in rows:
        value = str(row.get(key, ""))
        if not value:
            raise RuntimeError(f"{source} contains a row without {key}")
        if value in result:
            raise RuntimeError(f"{source} contains duplicate {key} {value}")
        result[value] = row
    return result


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


def validate_lineage(
    repository_root: Path,
    cases: list[dict[str, object]],
) -> tuple[
    dict[str, dict[str, str]],
    dict[str, dict[str, str]],
    frozenset[str],
]:
    inventory = repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    design = [row for row in read_csv(inventory / "test-case-design.csv") if row["component"] == "Packaging"]
    allocation = [
        row for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "Packaging"
    ]
    contracts = [row for row in read_csv(inventory / "contracts.csv") if row["component"] == "Packaging"]
    if not design or not allocation or not contracts:
        raise RuntimeError("Packaging canonical inventory is empty")
    design_by_id = unique_map(design, "case_id", "Packaging design")
    allocation_by_id = unique_map(allocation, "case_id", "Packaging allocation")
    contracts_by_id = unique_map(contracts, "contract_id", "Packaging contracts")
    manifest_by_id = unique_map(cases, "case_id", "Packaging manifest")
    if design_by_id.keys() != allocation_by_id.keys():
        raise RuntimeError("Packaging design and allocation case IDs differ")
    profile_mismatches: set[str] = set()
    for case_id, design_row in design_by_id.items():
        allocation_row = allocation_by_id[case_id]
        contract_id = str(design_row["contract_id"])
        if contract_id not in contracts_by_id:
            raise RuntimeError(f"Packaging case {case_id} references unknown contract {contract_id}")
        if str(allocation_row["contract_id"]) != contract_id:
            raise RuntimeError(f"Packaging allocation changes the contract of {case_id}")
        if str(design_row.get("frequency", "")) != str(allocation_row.get("default_profile", "")):
            profile_mismatches.add(case_id)
    for case_id, manifest_row in manifest_by_id.items():
        if case_id not in design_by_id:
            raise RuntimeError(f"Packaging manifest contains foreign case {case_id}")
        if str(manifest_row.get("contract_id", "")) != str(design_by_id[case_id]["contract_id"]):
            raise RuntimeError(f"Packaging manifest changes the contract of {case_id}")
    return design_by_id, contracts_by_id, frozenset(profile_mismatches)


def validate_classifications(
    repository_root: Path,
    cases: list[dict[str, object]],
    design_by_id: dict[str, dict[str, str]],
    contracts_by_id: dict[str, dict[str, str]],
    profile_mismatches: frozenset[str],
) -> tuple[frozenset[str], frozenset[str]]:
    component_root = repository_root / "tests" / "compiler-suite" / "packaging"
    catalog = read_jsonl(component_root / "PackagingContractCatalog.jsonl")
    classified = read_jsonl(component_root / "PackagingCaseClassifications.jsonl")
    contradictions = read_jsonl(component_root / "PackagingPlanContradictions.jsonl")
    catalog_by_id = unique_map(catalog, "contract_id", "Packaging contract catalogue")
    classified_by_id = unique_map(classified, "case_id", "Packaging classifications")
    if catalog_by_id.keys() != contracts_by_id.keys():
        raise RuntimeError("Packaging contract catalogue differs from the canonical inventory")
    catalog_cases: set[str] = set()
    executable_catalog_cases: set[str] = set()
    infrastructure_catalog_cases: set[str] = set()
    for contract_id, row in catalog_by_id.items():
        raw_case_ids = row.get("case_ids")
        if not isinstance(raw_case_ids, list) or not raw_case_ids:
            raise RuntimeError(f"Packaging catalogue contract {contract_id} has no case IDs")
        contract_cases = {str(case_id) for case_id in raw_case_ids}
        if len(contract_cases) != len(raw_case_ids):
            raise RuntimeError(f"Packaging catalogue contract {contract_id} repeats case IDs")
        if catalog_cases & contract_cases:
            raise RuntimeError(f"Packaging catalogue overlaps at contract {contract_id}")
        for case_id in contract_cases:
            design_row = design_by_id.get(case_id)
            if design_row is None or str(design_row["contract_id"]) != contract_id:
                raise RuntimeError(f"Packaging catalogue misclassifies {case_id}")
        catalog_cases.update(contract_cases)
        disposition = str(row.get("disposition", ""))
        if disposition == "Executable":
            executable_catalog_cases.update(contract_cases)
        elif disposition == "InfrastructurePending":
            infrastructure_catalog_cases.update(contract_cases)
        else:
            raise RuntimeError(f"unsupported Packaging contract disposition {disposition!r}")
    if catalog_cases != set(design_by_id):
        raise RuntimeError("Packaging contract catalogue does not cover the canonical denominator")
    for case_id, row in classified_by_id.items():
        design_row = design_by_id.get(case_id)
        if design_row is None:
            raise RuntimeError(f"Packaging classifications contain foreign case {case_id}")
        if str(row.get("contract_id", "")) != str(design_row["contract_id"]):
            raise RuntimeError(f"Packaging classification changes the contract of {case_id}")
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
    if infrastructure != infrastructure_catalog_cases:
        raise RuntimeError("Packaging infrastructure classifications differ from the contract catalogue")
    contradiction_keys: set[tuple[str, str]] = set()
    for row in contradictions:
        key = (str(row.get("scope", "")), str(row.get("field", "")))
        if not all(key) or key in contradiction_keys:
            raise RuntimeError("Packaging contradictions contain an invalid or duplicate key")
        if key != ("Packaging", "default_profile"):
            raise RuntimeError(f"unsupported Packaging contradiction {key!r}")
        contradiction_keys.add(key)
    if bool(profile_mismatches) != bool(contradictions):
        raise RuntimeError("Packaging profile contradiction does not match the live inventories")
    executable = {str(row["case_id"]) for row in cases}
    if executable != executable_catalog_cases:
        raise RuntimeError("Packaging manifest differs from the executable contract catalogue")
    if executable | infrastructure != set(design_by_id) or executable & infrastructure:
        raise RuntimeError("Packaging denominator is incomplete or overlapping")
    if not defects <= executable:
        raise RuntimeError("Packaging product defects must belong to executable cases")
    if set(classified_by_id) != infrastructure | defects:
        raise RuntimeError("Packaging classifications contain an unsupported disposition")
    for contract_id, row in catalog_by_id.items():
        contract_cases = {str(case_id) for case_id in row["case_ids"]}
        contract_defects = len(contract_cases & defects)
        contract_infrastructure = len(contract_cases & infrastructure)
        contract_required = len(contract_cases & executable) - contract_defects
        declared_counts = (
            required_nonnegative_integer(row, "required", f"Packaging contract {contract_id}"),
            required_nonnegative_integer(
                row, "known_product_defect", f"Packaging contract {contract_id}"
            ),
            required_nonnegative_integer(
                row, "infrastructure_pending", f"Packaging contract {contract_id}"
            ),
        )
        derived_counts = (contract_required, contract_defects, contract_infrastructure)
        if declared_counts != derived_counts:
            raise RuntimeError(f"Packaging catalogue summary differs at {contract_id}")
    tagged = {
        str(row["case_id"])
        for row in cases
        if "known-product-defect" in row.get("tags", [])
    }
    if tagged != defects:
        raise RuntimeError("Packaging executable KPD tags changed")
    return frozenset(infrastructure), frozenset(defects)


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
    contract = required_manifest_integer(inputs, "contract_ordinal", "Packaging")
    variant = required_manifest_integer(inputs, "variant", "Packaging")
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
    design_by_id, contracts_by_id, profile_mismatches = validate_lineage(
        options.repository_root,
        cases,
    )
    infrastructure_pending, known_product_defects = validate_classifications(
        options.repository_root,
        cases,
        design_by_id,
        contracts_by_id,
        profile_mismatches,
    )
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
                "duration_seconds": duration,
            }
        )
    payload = {
        "component": "Packaging",
        "build_mode": options.build_mode,
        "canonical_denominator": len(design_by_id),
        "new_suite_executable": len(cases),
        "required": len(cases) - len(known_product_defects),
        "known_product_defects": len(known_product_defects),
        "infrastructure_pending": len(infrastructure_pending),
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
