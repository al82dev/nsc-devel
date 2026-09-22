#!/usr/bin/env python3
"""SH48: rebuild and execute the complete NativeSharp 2.0 library/runtime surface."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], cwd: Path, timeout: int = 180) -> tuple[subprocess.CompletedProcess[str], float]:
    started = time.perf_counter()
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=timeout, check=False)
    return result, time.perf_counter() - started


def require(result: subprocess.CompletedProcess[str], label: str, failures: list[str]) -> bool:
    if result.returncode == 0:
        return True
    failures.append(f"{label}: exit={result.returncode}: {result.stdout}{result.stderr}")
    return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--seed-compiler", type=Path, required=True)
    parser.add_argument("--runtime-library", type=Path, required=True)
    parser.add_argument("--runtime-abi", type=Path, required=True)
    parser.add_argument("--lld-link", type=Path, required=True)
    parser.add_argument("--nslink", type=Path, required=True)
    parser.add_argument("--llvm-ar", type=Path, required=True)
    parser.add_argument("--llvm-nm", type=Path, required=True)
    parser.add_argument("--import-library", action="append", type=Path, default=[])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    root = args.source_root.resolve()
    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    if contract.get("schema") != "nativesharp-selfhost-runtime-library-contract" or contract.get("version") != 1:
        print("NSH4801: unsupported SH48 contract", file=sys.stderr)
        return 1
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    failures: list[str] = []
    timings: dict[str, float] = {}
    hashes: dict[str, str] = {}
    cases: list[dict[str, object]] = []

    sources = sorted(
        (path for relative in contract["library_roots"] for path in (root / relative).glob("*.ns")),
        key=lambda path: path.relative_to(root).as_posix(),
    )
    if len(sources) != int(contract["source_files"]):
        failures.append(f"standard-library source coverage: {len(sources)}/{contract['source_files']}")
    source_rows = [f"{digest(path)}  {path.relative_to(root).as_posix()}\n" for path in sources]
    source_manifest = output / "runtime-library-sources.manifest"
    source_manifest.write_text("".join(source_rows), encoding="utf-8", newline="\n")
    hashes["source_manifest"] = digest(source_manifest)

    verified, elapsed = run([str(args.seed_compiler), "--verify-windows-runtime-library",
                             str(args.runtime_library)], root)
    timings["runtime_verification"] = round(elapsed, 6)
    require(verified, "Windows runtime verification", failures)
    abi_text = args.runtime_abi.read_text(encoding="utf-8")
    for group, symbols in contract["runtime_symbol_groups"].items():
        for symbol in symbols:
            if symbol not in abi_text:
                failures.append(f"runtime ABI group {group} is missing {symbol}")
    hashes["runtime_library"] = digest(args.runtime_library)
    hashes["runtime_abi"] = digest(args.runtime_abi)

    library_project = root / contract["library_project"]
    smoke_project = root / contract["smoke_project"]
    for profile in contract["profiles"]:
        lower = profile.lower()
        library_object = output / f"NativeSharp.StandardLibrary.{lower}.obj"
        repeated_object = output / f"NativeSharp.StandardLibrary.{lower}.repeat.obj"
        library_archive = output / f"NativeSharp.StandardLibrary.{lower}.lib"
        command = [str(args.compiler), "--emit-project-library-object", str(library_project),
                   "-" + profile, "-o", str(library_object)]
        emitted, elapsed = run(command, root)
        timings[f"library_{lower}"] = round(elapsed, 6)
        if not require(emitted, f"library emission {profile}", failures):
            continue
        repeated, _ = run([*command[:-1], str(repeated_object)], root)
        if require(repeated, f"library repeat {profile}", failures):
            if library_object.read_bytes() != repeated_object.read_bytes():
                failures.append(f"library object is not deterministic for {profile}")
        symbols, _ = run([str(args.llvm_nm), "-g", str(library_object)], root)
        if require(symbols, f"library symbols {profile}", failures):
            for forbidden in contract["forbidden_library_symbols"]:
                if forbidden in symbols.stdout:
                    failures.append(f"{profile} library defines executable-only symbol {forbidden}")
        archived, _ = run([str(args.llvm_ar), "rcs", str(library_archive), str(library_object)], root)
        require(archived, f"library archive {profile}", failures)
        hashes[f"library_object_{lower}"] = digest(library_object)
        if library_archive.is_file():
            hashes[f"library_archive_{lower}"] = digest(library_archive)

        smoke_object = output / f"runtime-smoke-{lower}.obj"
        smoke, elapsed = run([str(args.compiler), "--emit-bootstrap-object", str(smoke_project),
                              "-" + profile, "-o", str(smoke_object)], root)
        timings[f"smoke_{lower}"] = round(elapsed, 6)
        if not require(smoke, f"runtime smoke emission {profile}", failures):
            continue
        undefined, _ = run([str(args.llvm_nm), "-u", str(smoke_object)], root)
        if require(undefined, f"runtime smoke hooks {profile}", failures):
            for hook in contract["required_smoke_hooks"]:
                if hook not in undefined.stdout:
                    failures.append(f"{profile} smoke object is missing runtime hook {hook}")

        libraries = [args.runtime_library, *args.import_library]
        for linker in contract["linkers"]:
            executable = output / f"runtime-smoke-{lower}-{linker}.exe"
            if linker == "lld":
                link = [str(args.lld_link), str(smoke_object), *map(str, libraries),
                        "/entry:mainCRTStartup", "/subsystem:console", "/machine:x64",
                        "/nodefaultlib", f"/out:{executable}"]
            else:
                link = [str(args.nslink), "--link", str(smoke_object)]
                for library in libraries:
                    link += ["--lib", str(library)]
                link += ["--entry", "mainCRTStartup", "-o", str(executable)]
            linked, elapsed = run(link, root)
            timings[f"link_{lower}_{linker}"] = round(elapsed, 6)
            if require(linked, f"{profile} {linker} link", failures):
                executed, elapsed = run([str(executable)], output, timeout=30)
                timings[f"run_{lower}_{linker}"] = round(elapsed, 6)
                if require(executed, f"{profile} {linker} execution", failures):
                    cases.append({"profile": profile, "linker": linker, "status": "passed",
                                  "sha256": digest(executable)})

    expected = int(contract["expected_executions"])
    if len(cases) != expected:
        failures.append(f"execution matrix incomplete: {len(cases)}/{expected}")
    report = {
        "schema": "nativesharp-selfhost-runtime-library-report", "version": 1,
        "gate": "SH48", "tanda": 50, "status": "failed" if failures else "passed",
        "source_files": len(sources), "profiles": contract["profiles"],
        "linkers": contract["linkers"], "passed_executions": len(cases),
        "expected_executions": expected, "deterministic_library_objects": not failures,
        "runtime_symbol_groups": sorted(contract["runtime_symbol_groups"]),
        "compiler_sha256": digest(args.compiler), "contract_sha256": digest(args.contract),
        "hashes": dict(sorted(hashes.items())), "timings_seconds": timings,
        "cases": cases, "failures": failures,
    }
    report_path = args.report or output / "sh48-report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8", newline="\n")
    if failures:
        for failure in failures:
            print("NSH4802: " + failure, file=sys.stderr)
        return 1
    print(f"SH48 runtime/library verified: {len(sources)} sources, {len(cases)} O0-O3 executions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
