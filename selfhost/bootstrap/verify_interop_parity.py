#!/usr/bin/env python3
"""SH46: execute the NativeSharp 2.0 C/C++/COM/callback/DLL ABI matrix."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

try:
    from .profile_selection import selected_profiles
except ImportError:
    from profile_selection import selected_profiles

def run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                          encoding="utf-8", errors="replace", check=False)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


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
    parser.add_argument("--nslink", type=Path, required=True)
    parser.add_argument("--clang-cl", type=Path, required=True)
    parser.add_argument("--llvm-nm", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    root = args.source_root.resolve()
    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    if contract.get("schema") != "nativesharp-selfhost-interop-parity-contract" or contract.get("version") != 1:
        print("NSH4601: unsupported SH46 contract", file=sys.stderr)
        return 1
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    failures: list[str] = []
    cases: list[dict[str, object]] = []

    host_c = output / "host-c.obj"
    host_cpp = output / "host-cpp.obj"
    consumer = output / "dll-consumer.obj"
    compile_jobs = [
        ([str(args.clang_cl), "/nologo", "/c", "/GS-", "/Zl", "/clang:-O2",
          "/Fo:" + str(host_c), str(root / contract["host_c"])], "C host"),
        ([str(args.clang_cl), "/nologo", "/c", "/GS-", "/Zl", "/EHs-c-", "/GR-", "/clang:-O2",
          "/Fo:" + str(host_cpp), str(root / contract["host_cpp"])], "C++ host"),
        ([str(args.clang_cl), "/nologo", "/c", "/GS-", "/Zl", "/clang:-O2",
          "/Fo:" + str(consumer), str(root / contract["dll_consumer"])], "DLL consumer"),
    ]
    for command, label in compile_jobs:
        require(run(command, root), label, failures)

    project = root / contract["project"]
    profiles = selected_profiles(contract["profiles"])
    for profile in profiles:
        lower = profile.lower()
        native_object = output / f"interop-{lower}.obj"
        emitted = run([str(args.compiler), "--emit-project-object", str(project),
                       "-" + profile, "-o", str(native_object)], root)
        if not require(emitted, f"NativeSharp {profile}", failures):
            continue
        symbols = run([str(args.llvm_nm), str(native_object)], root)
        symbol_text = symbols.stdout
        for name in contract["undefined_host_symbols"]:
            if name not in symbol_text:
                failures.append(f"{profile}: missing host ABI symbol {name}")
        for name in contract["exports"]:
            if name not in symbol_text:
                failures.append(f"{profile}: missing NativeSharp export {name}")

        for linker in contract["linkers"]:
            stem = f"interop-{lower}-{linker}"
            executable = output / (stem + ".exe")
            link_command = [str(args.nslink), "--link", str(native_object), str(host_c), str(host_cpp)]
            for dll, names in contract["imports"].items():
                link_command += ["--import", dll + ":" + ",".join(names)]
            link_command += ["--entry", contract["entry"], "-o", str(executable)]
            linked = run(link_command, root)
            if require(linked, f"{profile} {linker} EXE link", failures):
                executed = run([str(executable)], output)
                if require(executed, f"{profile} {linker} EXE run", failures):
                    cases.append({"profile": profile, "linker": linker, "product": "EXE",
                                  "sha256": digest(executable), "status": "passed"})

            dll = output / (stem + ".dll")
            implib = output / (stem + ".lib")
            dll_command = [str(args.nslink), "--link", str(native_object), str(host_c), str(host_cpp), "--dll"]
            for name in contract["exports"]:
                dll_command += ["--export", name]
            for imported_dll, names in contract["imports"].items():
                dll_command += ["--import", imported_dll + ":" + ",".join(names)]
            dll_command += ["--implib", str(implib), "-o", str(dll)]
            built_dll = run(dll_command, root)
            if not require(built_dll, f"{profile} {linker} DLL link", failures):
                continue
            consumer_exe = output / (stem + "-consumer.exe")
            consumer_command = [str(args.nslink), "--link", str(consumer), "--lib", str(implib),
                                "--import", "KERNEL32.dll:ExitProcess", "--entry", "mainCRTStartup",
                                "-o", str(consumer_exe)]
            linked_consumer = run(consumer_command, root)
            if require(linked_consumer, f"{profile} {linker} DLL consumer link", failures):
                consumed = run([str(consumer_exe)], output)
                if require(consumed, f"{profile} {linker} DLL consumer run", failures):
                    cases.append({"profile": profile, "linker": linker, "product": "DLL",
                                  "sha256": digest(dll), "status": "passed"})

    expected = len(profiles) * int(contract["expected_executions_per_profile"])
    if len(cases) != expected:
        failures.append(f"execution matrix incomplete: {len(cases)}/{expected}")
    report = {
        "schema": "nativesharp-selfhost-interop-parity-report", "version": 1,
        "status": "failed" if failures else "passed", "profiles": profiles,
        "linkers": contract["linkers"], "boundaries": contract["boundaries"],
        "passed_executions": len(cases), "expected_executions": expected,
        "compiler_sha256": digest(args.compiler), "contract_sha256": digest(args.contract),
        "cases": cases, "failures": failures,
    }
    report_path = args.report or output / "report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if failures:
        for failure in failures:
            print("NSH4602: " + failure, file=sys.stderr)
        return 1
    print(f"SH46 interop parity verified: {len(cases)} {'/'.join(profiles)} EXE/DLL executions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
