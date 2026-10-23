from __future__ import annotations

import argparse
from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

from component_gate_support import (
    component_manifest_rows,
    read_csv,
    read_jsonl,
    required_manifest_integer,
    required_nonnegative_integer,
    required_string_field,
    required_string_array,
    unique_string_index,
)


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


def log_process(case_root: Path, stem: str, completed: subprocess.CompletedProcess[str]) -> None:
    write_text(case_root / f"{stem}.stdout.log", completed.stdout)
    write_text(case_root / f"{stem}.stderr.log", completed.stderr)


def validate_lineage(
    repository_root: Path,
    cases: list[dict[str, object]],
) -> tuple[
    dict[str, dict[str, str]],
    dict[str, dict[str, str]],
]:
    inventory = repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    design = [
        row
        for row in read_csv(
            inventory / "test-case-design.csv",
            ("case_id", "contract_id", "component", "layer", "frequency"),
        )
        if row["component"] == "Packaging"
    ]
    allocation = [
        row
        for row in read_csv(
            inventory / "suite-case-allocation.csv",
            ("case_id", "contract_id", "component", "layer", "default_profile"),
        )
        if row["component"] == "Packaging"
    ]
    contracts = [
        row
        for row in read_csv(
            inventory / "contracts.csv",
            ("contract_id", "component"),
        )
        if row["component"] == "Packaging"
    ]
    if not design or not allocation or not contracts:
        raise RuntimeError("Packaging canonical inventory is empty")
    design_by_id = unique_string_index(design, "case_id", "Packaging design")
    allocation_by_id = unique_string_index(
        allocation, "case_id", "Packaging allocation"
    )
    contracts_by_id = unique_string_index(
        contracts, "contract_id", "Packaging contracts"
    )
    manifest_by_id = unique_string_index(cases, "case_id", "Packaging manifest")
    if design_by_id.keys() != allocation_by_id.keys():
        raise RuntimeError("Packaging design and allocation case IDs differ")
    for case_id, design_row in design_by_id.items():
        allocation_row = allocation_by_id[case_id]
        contract_id = design_row["contract_id"]
        if contract_id not in contracts_by_id:
            raise RuntimeError(f"Packaging case {case_id} references unknown contract {contract_id}")
        if allocation_row["contract_id"] != contract_id:
            raise RuntimeError(f"Packaging allocation changes the contract of {case_id}")
    for case_id, manifest_row in manifest_by_id.items():
        required_string_array(
            manifest_row, "tags", f"Packaging manifest case {case_id}"
        )
        if case_id not in design_by_id:
            raise RuntimeError(f"Packaging manifest contains foreign case {case_id}")
        manifest_contract = required_string_field(
            manifest_row, "contract_id", f"Packaging manifest case {case_id}"
        )
        if manifest_contract != design_by_id[case_id]["contract_id"]:
            raise RuntimeError(f"Packaging manifest changes the contract of {case_id}")
        if manifest_row.get("layer") != design_by_id[case_id]["layer"]:
            raise RuntimeError(f"Packaging manifest changes the layer of {case_id}")
        if manifest_row.get("profile") != allocation_by_id[case_id]["default_profile"]:
            raise RuntimeError(f"Packaging manifest changes the profile of {case_id}")
    return design_by_id, contracts_by_id


def validate_classifications(
    repository_root: Path,
    cases: list[dict[str, object]],
    design_by_id: dict[str, dict[str, str]],
    contracts_by_id: dict[str, dict[str, str]],
) -> frozenset[str]:
    component_root = repository_root / "tests" / "compiler-suite" / "packaging"
    live_contract_ids = {row["contract_id"] for row in design_by_id.values()}
    contracts_by_id = {
        contract_id: row
        for contract_id, row in contracts_by_id.items()
        if contract_id in live_contract_ids
    }
    catalog = [
        row
        for row in read_jsonl(component_root / "PackagingContractCatalog.jsonl")
        if row.get("contract_id") in live_contract_ids
    ]
    all_classified = read_jsonl(component_root / "PackagingCaseClassifications.jsonl")
    classified = [
        row for row in all_classified if row.get("case_id") in design_by_id
    ]
    catalog_by_id = unique_string_index(
        catalog, "contract_id", "Packaging contract catalogue"
    )
    classified_by_id = unique_string_index(
        classified, "case_id", "Packaging classifications"
    )
    if catalog_by_id.keys() != contracts_by_id.keys():
        raise RuntimeError("Packaging contract catalogue differs from the canonical inventory")
    catalog_cases: set[str] = set()
    executable_catalog_cases: set[str] = set()
    catalog_case_ids: dict[str, tuple[str, ...]] = {}
    for contract_id, row in catalog_by_id.items():
        contract_cases_ordered = required_string_array(
            row, "case_ids", f"Packaging contract {contract_id}"
        )
        if not contract_cases_ordered:
            raise RuntimeError(f"Packaging catalogue contract {contract_id} has no case IDs")
        catalog_case_ids[contract_id] = contract_cases_ordered
        contract_cases = set(contract_cases_ordered)
        if catalog_cases & contract_cases:
            raise RuntimeError(f"Packaging catalogue overlaps at contract {contract_id}")
        for case_id in contract_cases:
            design_row = design_by_id.get(case_id)
            if design_row is None or design_row["contract_id"] != contract_id:
                raise RuntimeError(f"Packaging catalogue misclassifies {case_id}")
        catalog_cases.update(contract_cases)
        disposition = required_string_field(
            row, "disposition", f"Packaging contract {contract_id}"
        )
        if disposition == "Executable":
            executable_catalog_cases.update(contract_cases)
        else:
            raise RuntimeError(f"unsupported Packaging contract disposition {disposition!r}")
    if catalog_cases != set(design_by_id):
        raise RuntimeError("Packaging contract catalogue does not cover the canonical denominator")
    for case_id, row in classified_by_id.items():
        design_row = design_by_id.get(case_id)
        if design_row is None:
            raise RuntimeError(f"Packaging classifications contain foreign case {case_id}")
        contract_id = required_string_field(
            row, "contract_id", f"Packaging classification {case_id}"
        )
        required_string_field(
            row, "disposition", f"Packaging classification {case_id}"
        )
        if contract_id != design_row["contract_id"]:
            raise RuntimeError(f"Packaging classification changes the contract of {case_id}")
    defects = {
        row["case_id"]
        for row in classified
        if row["disposition"] == "KnownProductDefect"
    }
    executable = {row["case_id"] for row in cases}
    if executable != executable_catalog_cases:
        raise RuntimeError("Packaging manifest differs from the executable contract catalogue")
    if executable != set(design_by_id):
        raise RuntimeError("Packaging executable denominator is incomplete")
    if not defects <= executable:
        raise RuntimeError("Packaging product defects must belong to executable cases")
    if set(classified_by_id) != defects:
        raise RuntimeError("Packaging classifications contain an unsupported disposition")
    for contract_id, row in catalog_by_id.items():
        contract_cases = set(catalog_case_ids[contract_id])
        contract_defects = len(contract_cases & defects)
        contract_required = len(contract_cases & executable) - contract_defects
        declared_counts = (
            required_nonnegative_integer(row, "required", f"Packaging contract {contract_id}"),
            required_nonnegative_integer(
                row, "known_product_defect", f"Packaging contract {contract_id}"
            ),
        )
        derived_counts = (contract_required, contract_defects)
        if declared_counts != derived_counts:
            raise RuntimeError(f"Packaging catalogue summary differs at {contract_id}")
    tagged = {
        row["case_id"]
        for row in cases
        if "known-product-defect" in row.get("tags", [])
    }
    if tagged != defects:
        raise RuntimeError("Packaging executable KPD tags changed")
    return frozenset(defects)


def load_builder_deploy(repository_root: Path) -> object:
    """Import nsc_builder.deploy from the builder kept in the repository (nsc-builder/)."""
    builder_root = str(repository_root / "nsc-builder")
    if builder_root not in sys.path:
        sys.path.insert(0, builder_root)
    return importlib.import_module("nsc_builder.deploy")


def tool_shims(case_root: Path, names: tuple[str, ...]) -> Path:
    """Create empty command shims so tool discovery resolves without running real tools."""
    shim_root = case_root / "shim"
    shim_root.mkdir(parents=True, exist_ok=True)
    for name in names:
        write_text(shim_root / f"{name}.cmd", "@echo off\r\nexit /b 0\r\n")
    return shim_root


def create_plan(deploy: object, config: object, build_mode: str) -> tuple[object | None, str]:
    """Return (plan, "") or (None, error) from nsc_builder.deploy.create_deploy_plan."""
    try:
        return deploy.create_deploy_plan(config, build_mode), ""
    except ValueError as error:
        return None, str(error)


def check_deploy_variant(
    deploy: object,
    config: object,
    build_mode: str,
    build_dir: Path,
    prefix: Path,
    variant: int,
    case_root: Path,
) -> tuple[bool, str]:
    """Evaluate one deploy-plan variant; nothing is installed or executed."""
    plan, error = create_plan(deploy, config, build_mode)
    write_text(case_root / "plan.log", repr(plan.commands) if plan is not None else error)
    if variant in {1, 2}:
        expected = [
            "--install", build_dir.resolve().as_posix(), "--config", build_mode,
            "--prefix", prefix.resolve().as_posix(),
        ]
        return (
            plan is not None
            and plan.commands[0][1][1:] == expected
            and plan.installed_compiler == prefix.resolve() / "bin" / "nsc.exe"
        ), "deploy plan ignored the selected build tree or prefix"
    if variant == 3:
        return plan is None and "cmake" in error, "missing CMake must fail before any command"
    if variant == 4:
        return plan is None and "unsafe deploy prefix" in error, "drive-root prefix was not rejected"
    if variant == 5:
        return (
            plan is None and "overlaps compiler build tree" in error
        ), "prefix inside the build tree was not rejected"
    return False, f"deploy plan variant {variant} is not checked yet"


def run_deploy_plan(
    repository_root: Path,
    case_root: Path,
    variant: int,
    build_mode: str,
) -> tuple[bool, str]:
    """Check the builder deploy plan for one variant with a case-local build tree and prefix."""
    deploy = load_builder_deploy(repository_root)
    configuration = importlib.import_module("nsc_builder.configuration")
    build_root = case_root / ("build unicode" if variant == 2 else "build")
    config = configuration.BuilderConfig(compiler_build_root=str(build_root))
    build_dir = deploy.compiler_build_dir(config, build_mode)
    build_dir.mkdir(parents=True, exist_ok=True)
    write_text(build_dir / "cmake_install.cmake", "")
    prefix = case_root / ("prefix unicode" if variant == 2 else "prefix")
    if variant == 4:
        prefix = Path(case_root.anchor)
    if variant == 5:
        prefix = build_dir
    shim_root = tool_shims(case_root, ("ninja",) if variant == 3 else ("cmake", "ninja"))
    original_prefix = deploy.DEPLOY_PREFIX
    original_path = os.environ.get("PATH", "")
    deploy.DEPLOY_PREFIX = prefix
    system32 = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32"
    os.environ["PATH"] = os.pathsep.join((str(shim_root), str(system32)))
    try:
        return check_deploy_variant(deploy, config, build_mode, build_dir, prefix, variant, case_root)
    finally:
        deploy.DEPLOY_PREFIX = original_prefix
        os.environ["PATH"] = original_path


def stage_installation(installation_root: Path, case_root: Path) -> Path:
    prefix = case_root / "installation"
    shutil.copytree(installation_root, prefix)
    return prefix


def load_packaging_api(script: Path) -> object:
    module_name = "nsc_packaging_api"
    spec = importlib.util.spec_from_file_location(module_name, script)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load Packaging API: {script}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    try:
        spec.loader.exec_module(module)
    finally:
        sys.modules.pop(module_name, None)
    return module


def package_arguments(
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
        str(script),
        "--prefix", str(prefix),
        "--seven-zip", str(archive_executable),
        "--output-dir", str(output),
        "--compression", compression,
        "--timestamp", timestamp,
    ]
    if dry_run:
        command.append("--dry-run")
    return command


def invoke_packaging_api(
    api: object,
    arguments: list[str],
    prefix: Path,
    archive_executable: Path,
    output: Path,
    variant: int,
    dry_run: bool,
    environment: dict[str, str],
) -> subprocess.CompletedProcess[str]:
    stdout = io.StringIO()
    stderr = io.StringIO()
    compression = 10 if variant == 4 else 9
    timestamp = "..." if variant == 5 else "20261009-000000"
    return_code = 0
    with redirect_stdout(stdout), redirect_stderr(stderr):
        try:
            api.create_archive(
                prefix,
                archive_executable,
                output,
                compression,
                timestamp,
                dry_run,
                environment,
            )
        except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
            print(f"ERROR: {error}", file=sys.stderr)
            return_code = 1
    return subprocess.CompletedProcess(
        arguments,
        return_code,
        stdout.getvalue(),
        stderr.getvalue(),
    )


def run_package_native_sharp(
    options: argparse.Namespace,
    case_root: Path,
    variant: int,
    environment: dict[str, str],
) -> tuple[bool, str]:
    prefix = stage_installation(options.installation_root, case_root)
    output = case_root / "output"
    script = options.repository_root / "package_native_sharp.py"
    api = load_packaging_api(script)
    if variant == 3:
        compiler = prefix / "bin" / "nsc.exe"
        if compiler.exists():
            compiler.unlink()
    dry_run = variant != 2
    command = package_arguments(
        script,
        prefix,
        options.archive_executable,
        output,
        variant,
        dry_run,
    )

    def invoke(stem: str) -> subprocess.CompletedProcess[str]:
        completed = invoke_packaging_api(
            api,
            command,
            prefix,
            options.archive_executable,
            output,
            variant,
            dry_run,
            environment,
        )
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
        return run_deploy_plan(options.repository_root, case_root, variant, options.build_mode)
    if contract == 3:
        return run_package_native_sharp(options, case_root, variant, environment)
    return False, f"unsupported executable Packaging contract {contract}"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--installation-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    options = parser.parse_args()
    options.archive_executable = options.archive_executable.resolve()
    options.source_root = options.source_root.resolve()
    options.repository_root = options.repository_root.resolve()
    options.installation_root = options.installation_root.resolve()
    if not options.archive_executable.is_file():
        raise RuntimeError("Packaging requires an explicit archive executable")
    if not (options.installation_root / "bin" / "nsc.exe").is_file():
        raise RuntimeError("Packaging requires an explicit sandbox-copyable installation root")

    cases = component_manifest_rows(
        options.source_root, options.repository_root, "Packaging"
    )
    design_by_id, contracts_by_id = validate_lineage(
        options.repository_root,
        cases,
    )
    known_product_defects = validate_classifications(
        options.repository_root,
        cases,
        design_by_id,
        contracts_by_id,
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
