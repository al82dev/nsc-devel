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


ACTIVE_CONTRACTS = (1, 2, 3, 4, 5, 76)
ALL_IDS = tuple(f"T12B-{ordinal:05d}" for ordinal in range(1, 457))
EXECUTABLE_IDS = tuple(
    f"T12B-{ordinal:05d}"
    for contract in ACTIVE_CONTRACTS
    for ordinal in range((contract - 1) * 6 + 1, contract * 6 + 1)
)
HISTORICAL_DELEGATED = frozenset(f"T12B-{ordinal:05d}" for ordinal in range(31, 451))
KNOWN_PRODUCT_DEFECTS = frozenset({"T12B-00021", "T12B-00022"})
ZLIB_SOURCES = (
    "adler32.c", "compress.c", "crc32.c", "deflate.c", "infback.c", "inffast.c",
    "inflate.c", "inftrees.c", "trees.c", "uncompr.c", "zutil.c",
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
        command, cwd=cwd, env=environment, text=True, encoding="utf-8",
        errors="replace", capture_output=True, timeout=timeout, check=False,
    )


def validate_lineage(repository_root: Path, cases: list[dict[str, object]]) -> None:
    inventory = repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    design = [r for r in read_csv(inventory / "test-case-design.csv") if r["component"] == "BuildSystem"]
    allocation = [r for r in read_csv(inventory / "suite-case-allocation.csv") if r["component"] == "BuildSystem"]
    contracts = [r for r in read_csv(inventory / "contracts.csv") if r["component"] == "BuildSystem"]
    if tuple(r["case_id"] for r in design) != ALL_IDS:
        raise RuntimeError("BuildSystem design is not the canonical T12B-00001..T12B-00456 range")
    if tuple(r["case_id"] for r in allocation) != ALL_IDS:
        raise RuntimeError("BuildSystem allocation is not the canonical T12B-00001..T12B-00456 range")
    if len(contracts) != 76:
        raise RuntimeError("BuildSystem contract inventory no longer contains 76 contracts")
    if sum(r["default_profile"] == "NIGHTLY" for r in allocation) != 36:
        raise RuntimeError("BuildSystem allocation no longer contains 36 NIGHTLY cases")
    if sum(r["default_profile"] == "WEEKLY" for r in allocation) != 420:
        raise RuntimeError("BuildSystem allocation no longer contains 420 WEEKLY cases")
    if tuple(str(r["case_id"]) for r in cases) != EXECUTABLE_IDS:
        raise RuntimeError("fragment does not contain the exact 36-case executable partition")


def validate_classifications(repository_root: Path, cases: list[dict[str, object]]) -> None:
    component_root = repository_root / "tests" / "compiler-suite" / "buildsystem"
    catalog = read_jsonl(component_root / "BuildSystemContractCatalog.jsonl")
    classified = read_jsonl(component_root / "BuildSystemCaseClassifications.jsonl")
    if len(catalog) != 76 or {str(r["contract_id"]) for r in catalog} != {
        f"C12B-BLD-{index:03d}" for index in range(1, 77)
    }:
        raise RuntimeError("BuildSystem contract catalogue is incomplete")
    delegated = {
        str(r["case_id"]) for r in classified if r["disposition"] == "HistoricalDelegated"
    }
    defects = {
        str(r["case_id"]) for r in classified if r["disposition"] == "KnownProductDefect"
    }
    if delegated != HISTORICAL_DELEGATED:
        raise RuntimeError("BuildSystem historical delegation is not the exact 420-case allocation")
    if defects != KNOWN_PRODUCT_DEFECTS:
        raise RuntimeError("BuildSystem KPD catalogue changed")
    executable = {str(r["case_id"]) for r in cases}
    if executable | delegated != set(ALL_IDS) or executable & delegated:
        raise RuntimeError("BuildSystem denominator is incomplete or overlapping")
    tagged = {
        str(r["case_id"]) for r in cases if "known-product-defect" in r.get("tags", [])
    }
    if tagged != KNOWN_PRODUCT_DEFECTS:
        raise RuntimeError("BuildSystem executable KPD tags changed")


def log_process(case_root: Path, stem: str, completed: subprocess.CompletedProcess[str]) -> None:
    write_text(case_root / f"{stem}.stdout.log", completed.stdout)
    write_text(case_root / f"{stem}.stderr.log", completed.stderr)


def run_identity(
    cmake: Path, repository_root: Path, fixture_root: Path, case_root: Path, variant: int,
    environment: dict[str, str],
) -> tuple[bool, str]:
    standard_template = fixture_root / "BuildIdentity.in.h"

    def invoke(
        stem: str, override: str | None, omit_output: bool = False, empty_template: bool = False
    ) -> subprocess.CompletedProcess[str]:
        output = case_root / f"{stem}.h"
        template = standard_template
        if empty_template:
            template = case_root / "empty.in.h"
            write_text(template, "")
        command = [str(cmake), f"-DNSC_TEMPLATE={template}", "-DNSC_VERSION_BASE=3.0.0"]
        if not omit_output:
            command.append(f"-DNSC_OUTPUT={output}")
        if override is not None:
            command.append(f"-DNSC_BUILD_ID_OVERRIDE={override}")
        command.extend(["-P", str(repository_root / "cmake" / "GenerateBuildIdentity.cmake")])
        completed = run_process(command, case_root, environment)
        log_process(case_root, stem, completed)
        return completed

    if variant == 3:
        completed = invoke("missing-output", "0000000000000001", omit_output=True)
        return completed.returncode != 0 and "NSC_OUTPUT is required" in completed.stderr, "missing output must fail"
    if variant == 4:
        completed = invoke("invalid-override", "ABC")
        return completed.returncode != 0 and "exactly 16 lowercase hexadecimal" in completed.stderr, "invalid override must fail"
    if variant == 6:
        first = invoke("first", "abcdef0123456789")
        second = invoke("second", "abcdef0123456789")
        same = (case_root / "first.h").read_bytes() == (case_root / "second.h").read_bytes()
        return first.returncode == 0 and second.returncode == 0 and same, "canonical override outputs must match"
    override = "0000000000000001" if variant in {1, 5} else "abcdef0123456789"
    completed = invoke("identity", override, empty_template=variant == 5)
    output = case_root / "identity.h"
    if completed.returncode != 0 or not output.is_file():
        return False, "identity producer failed or omitted output"
    if variant == 5:
        return output.read_bytes() == b"", "empty template must produce an empty file"
    text = output.read_text(encoding="utf-8")
    expected_id = override
    expected_version = f"3.0.0+build.19700101T000000Z.{expected_id}"
    return expected_id in text and expected_version in text, "generated identity fields differ"


def stage_llvm_fixture(fixture_root: Path, case_root: Path, variant: int) -> Path:
    source = case_root / "llvm-dependencies"
    source.mkdir(parents=True)
    shutil.copy2(fixture_root / "LlvmStaticDependencies" / "CMakeLists.txt", source / "CMakeLists.txt")
    write_text(source / "duplicate.c", "int duplicate_symbol(void) { return 0; }\n")
    zlib = source / "third_party" / "zlib"
    zstd = source / "third_party" / "zstd" / "lib"
    for name in ZLIB_SOURCES:
        if variant != 3 or name != "adler32.c":
            write_text(zlib / name, f"int nsc_probe_{name.replace('.', '_')}(void) {{ return 0; }}\n")
    if variant != 5:
        for directory, name in (("common", "common.c"), ("compress", "compress.c"), ("decompress", "decompress.c")):
            write_text(zstd / directory / name, f"int nsc_probe_{directory}(void) {{ return 0; }}\n")
    return source


def run_llvm_dependencies(
    cmake: Path, repository_root: Path, fixture_root: Path, case_root: Path, variant: int,
    environment: dict[str, str],
) -> tuple[bool, str]:
    def configure(stem: str) -> subprocess.CompletedProcess[str]:
        source = stage_llvm_fixture(fixture_root, case_root / stem, variant)
        build = case_root / stem / "build"
        command = [str(cmake), "-S", str(source), "-B", str(build), f"-DNSC_REPOSITORY_ROOT={repository_root}"]
        if variant == 4:
            command.append("-DNSC_DECLARE_DUPLICATE_ZLIB=ON")
        completed = run_process(command, case_root / stem, environment)
        log_process(case_root, stem, completed)
        return completed

    if variant == 6:
        first = configure("first")
        second = configure("second")
        left = case_root / "first" / "build" / "probe-result.txt"
        right = case_root / "second" / "build" / "probe-result.txt"
        return first.returncode == 0 and second.returncode == 0 and left.read_bytes() == right.read_bytes(), "dependency target metadata differs"
    completed = configure("configure")
    if variant in {3, 4, 5}:
        return completed.returncode != 0, "invalid dependency fixture must fail configuration"
    result = case_root / "configure" / "build" / "probe-result.txt"
    if completed.returncode != 0 or not result.is_file():
        return False, "dependency configuration failed or omitted probe result"
    text = result.read_text(encoding="utf-8")
    return "zlib=nsc-zlib" in text and "zstd=nsc-zstd" in text, "target output names differ"


def run_profiles(
    cmake: Path, repository_root: Path, fixture_root: Path, case_root: Path, variant: int,
    environment: dict[str, str],
) -> tuple[bool, str]:
    values = {1: "O0", 2: "O3, O1, O3", 3: "BAD", 4: "O0;BAD", 5: "   "}
    local_environment = dict(environment)
    if variant in values:
        local_environment["NSC_SELFHOST_PROFILES"] = values[variant]
    else:
        local_environment.pop("NSC_SELFHOST_PROFILES", None)
    output = case_root / "profiles.txt"
    command = [str(cmake), f"-DNSC_REPOSITORY_ROOT={repository_root}", f"-DNSC_PROBE_OUTPUT={output}", "-P", str(fixture_root / "BuildSystemProfilesProbe.cmake")]
    completed = run_process(command, case_root, local_environment)
    log_process(case_root, "profiles", completed)
    if variant in {3, 4, 5}:
        return completed.returncode != 0, "invalid or empty profile selection must fail"
    expected = {1: "O0\n", 2: "O3;O1\n", 6: "O0;O1;O2;O3\n"}[variant]
    return completed.returncode == 0 and output.read_text(encoding="utf-8") == expected, "profile selection differs"


def run_runtime(
    cmake: Path, repository_root: Path, fixture_root: Path, case_root: Path, variant: int,
    environment: dict[str, str],
) -> tuple[bool, str]:
    output = case_root / "runtime.txt"
    nslink = case_root / "prefix" / "bin" / "nslink.exe"
    if variant == 3:
        nslink_value = ""
    elif variant == 4:
        nslink_value = "not-a-path"
    else:
        nslink_value = str(nslink)
    command = [str(cmake), f"-DNSC_REPOSITORY_ROOT={repository_root}", f"-DNSC_PROBE_OUTPUT={output}", f"-DNSC_PROBE_NSLINK={nslink_value}"]
    if variant == 2:
        command.extend([f"-DRUNTIME_LIBRARY={case_root / 'explicit.lib'}", f"-DRUNTIME_ABI={case_root / 'explicit.abi'}"])
    if variant == 5:
        command.extend(["-DRUNTIME_LIBRARY=", "-DRUNTIME_ABI="])
    command.extend(["-P", str(fixture_root / "BuildSystemRuntimeProbe.cmake")])
    completed = run_process(command, case_root, environment)
    log_process(case_root, "runtime", completed)
    if variant in {3, 4}:
        return completed.returncode != 0 and not output.exists(), "invalid NSLink producer must fail before publication"
    if completed.returncode != 0 or not output.is_file():
        return False, "runtime resolution failed or omitted output"
    text = output.read_text(encoding="utf-8").replace("\\", "/")
    if variant == 6:
        first = output.read_bytes()
        second = run_process(command, case_root, environment)
        log_process(case_root, "runtime-second", second)
        return second.returncode == 0 and output.read_bytes() == first, "repeated runtime resolution differs"
    if variant == 2:
        return "explicit.lib" in text and "explicit.abi" in text, "explicit override lost precedence"
    return "/lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib" in text and "/share/nativesharp/runtime/NativeSharpRuntime.abi" in text, "derived runtime paths differ"


def run_installation(
    cmake: Path, repository_root: Path, installation_root: Path, case_root: Path, variant: int,
    environment: dict[str, str],
) -> tuple[bool, str]:
    prefix = case_root / "installation"
    shutil.copytree(installation_root, prefix)
    compiler = prefix / "bin" / "nsc.exe"
    if variant == 3 and compiler.exists():
        compiler.unlink()
    manifest = prefix / "share" / "nativesharp" / "toolchain" / "NativeSharpInstallation.manifest"
    if variant == 4:
        write_text(manifest, "malformed previous manifest\n")

    def invoke(stem: str) -> subprocess.CompletedProcess[str]:
        completed = run_process([str(cmake), f"-DCMAKE_INSTALL_PREFIX={prefix}", "-P", str(repository_root / "cmake" / "VerifyInstalledNativeSharp.cmake")], case_root, environment)
        log_process(case_root, stem, completed)
        return completed

    first = invoke("first")
    if variant in {3, 4}:
        return first.returncode != 0 and not (prefix / ".nativesharp-update.plan").exists(), "invalid installation must fail without stale plan"
    if first.returncode != 0 or not manifest.is_file():
        return False, "installation verification failed or omitted manifest"
    if variant in {2, 6}:
        first_bytes = manifest.read_bytes()
        second = invoke("second")
        same = manifest.is_file() and manifest.read_bytes() == first_bytes
        return second.returncode == 0 and same, "repeated installation verification differs"
    return True, ""


def normalized_target_help(output: str) -> tuple[str, ...]:
    return tuple(sorted(line.strip() for line in output.splitlines() if line.strip().startswith("... ")))


def run_root_graph(
    cmake: Path, repository_root: Path, case_root: Path, variant: int,
    base_arguments: list[str], environment: dict[str, str],
) -> tuple[bool, str]:
    def configure(stem: str) -> tuple[subprocess.CompletedProcess[str], Path]:
        build = case_root / stem
        arguments = list(base_arguments)
        if variant == 3:
            arguments = [arg for arg in arguments if "LLVM_DIR=" not in arg]
        if variant == 4:
            arguments.append("-DNSC_HOST_LINKAGE=INVALID")
        arguments.append(f"-DNSC_BUILD_TESTS={'ON' if variant == 2 else 'OFF'}")
        completed = run_process([str(cmake), "-S", str(repository_root), "-B", str(build), *arguments], case_root, environment)
        log_process(case_root, stem, completed)
        return completed, build

    if variant == 6:
        first, first_build = configure("first")
        second, second_build = configure("second")
        if first.returncode != 0 or second.returncode != 0:
            return False, "root graph configuration failed"
        first_help = run_process([str(cmake), "--build", str(first_build), "--target", "help"], case_root, environment)
        second_help = run_process([str(cmake), "--build", str(second_build), "--target", "help"], case_root, environment)
        log_process(case_root, "first-help", first_help)
        log_process(case_root, "second-help", second_help)
        return first_help.returncode == 0 and second_help.returncode == 0 and normalized_target_help(first_help.stdout) == normalized_target_help(second_help.stdout), "root target graphs differ"
    completed, _ = configure("configure")
    if variant in {3, 4}:
        return completed.returncode != 0, "invalid root configuration must fail"
    return completed.returncode == 0, "root configuration failed"


def run_case(
    row: dict[str, object], options: argparse.Namespace, case_root: Path,
    environment: dict[str, str],
) -> tuple[bool, str]:
    inputs = row["inputs"]
    if not isinstance(inputs, dict):
        return False, "manifest inputs are not an object"
    contract = int(inputs["contract_ordinal"])
    variant = int(inputs["variant"])
    fixture_root = options.source_root / "buildsystem" / "fixtures"
    if contract == 1:
        return run_identity(options.cmake_executable, options.repository_root, fixture_root, case_root, variant, environment)
    if contract == 2:
        return run_llvm_dependencies(options.cmake_executable, options.repository_root, fixture_root, case_root, variant, environment)
    if contract == 3:
        return run_profiles(options.cmake_executable, options.repository_root, fixture_root, case_root, variant, environment)
    if contract == 4:
        return run_runtime(options.cmake_executable, options.repository_root, fixture_root, case_root, variant, environment)
    if contract == 5:
        return run_installation(options.cmake_executable, options.repository_root, options.installation_root, case_root, variant, environment)
    if contract == 76:
        return run_root_graph(options.cmake_executable, options.repository_root, case_root, variant, options.root_config_argument, environment)
    return False, f"unsupported active BuildSystem contract {contract}"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmake-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--installation-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    parser.add_argument("--root-config-argument", action="append", default=[])
    options = parser.parse_args()
    options.cmake_executable = options.cmake_executable.resolve()
    options.source_root = options.source_root.resolve()
    options.repository_root = options.repository_root.resolve()
    options.installation_root = options.installation_root.resolve()
    if not options.cmake_executable.is_file():
        raise RuntimeError("BuildSystem requires an explicit CMake executable")
    if not (options.installation_root / "bin" / "nsc.exe").is_file():
        raise RuntimeError("BuildSystem requires an explicit sandbox-copyable installation root")
    if not any("LLVM_DIR=" in argument for argument in options.root_config_argument):
        raise RuntimeError("BuildSystem root graph requires an explicit LLVM_DIR config argument")

    manifest = read_jsonl(options.source_root / "manifest" / "cases.jsonl")
    cases = [row for row in manifest if row.get("component") == "BuildSystem"]
    validate_lineage(options.repository_root, cases)
    validate_classifications(options.repository_root, cases)
    work_root = options.work_parent.resolve() / f"buildsystem-{options.build_mode.lower()}"
    work_root.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    results: list[dict[str, object]] = []
    failures = 0
    for row in cases:
        case_id = str(row["case_id"])
        case_root = work_root / "cases" / case_id
        case_root.mkdir(parents=True, exist_ok=True)
        started = time.monotonic()
        satisfied, detail = run_case(row, options, case_root, environment)
        duration = time.monotonic() - started
        if case_id in KNOWN_PRODUCT_DEFECTS:
            status = "UNEXPECTED_PASS" if satisfied else "KNOWN_PRODUCT_DEFECT"
            failures += 1
        else:
            status = "PASS" if satisfied else "FAIL"
            failures += int(not satisfied)
        results.append({"case_id": case_id, "contract_id": row["contract_id"], "status": status, "detail": detail, "duration_seconds": duration})
    payload = {
        "component": "BuildSystem", "build_mode": options.build_mode,
        "canonical_denominator": len(ALL_IDS), "new_suite_executable": len(cases),
        "required": len(cases) - len(KNOWN_PRODUCT_DEFECTS),
        "known_product_defects": len(KNOWN_PRODUCT_DEFECTS),
        "historical_delegated": len(HISTORICAL_DELEGATED), "failures": failures,
        "results": results,
    }
    write_text(work_root / "results.json", json.dumps(payload, indent=2) + "\n")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"buildsystem_gate: error: {error}", file=sys.stderr)
        raise SystemExit(2)
