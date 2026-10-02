#!/usr/bin/env python3
"""SH47: certify specialized NativeSharp code and the external NSAsm boundary."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def run(command: list[str], cwd: Path, timeout: int = 120) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout, check=False)


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
    parser.add_argument("--nsasm", type=Path, required=True)
    parser.add_argument("--nslink", type=Path, required=True)
    parser.add_argument("--llvm-objdump", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    root = args.source_root.resolve()
    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    if contract.get("schema") != "nativesharp-selfhost-specialized-code-contract" or contract.get("version") != 1:
        print("NSH4701: unsupported SH47 contract", file=sys.stderr)
        return 1
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    failures: list[str] = []
    cases: list[dict[str, object]] = []

    version = run([str(args.nsasm), "/version"], root)
    require(version, "NSAsm identity", failures)
    assembly_object = output / "specialized-nsasm.obj"
    assembled = run([str(args.nsasm), "/nologo", "/c", "/Fo" + str(assembly_object),
                     str(root / contract["assembly"])], root)
    if require(assembled, "NSAsm object", failures):
        dump = run([str(args.llvm_objdump), "-d", str(assembly_object)], root)
        if require(dump, "NSAsm disassembly", failures):
            text = dump.stdout.lower()
            for fragment in contract["required_nsasm_disassembly"]:
                if fragment.lower() not in text:
                    failures.append(f"NSAsm object is missing instruction {fragment}")

    project = root / contract["project"]
    for profile in contract["profiles"]:
        native_object = output / f"specialized-{profile.lower()}.obj"
        emitted = run([str(args.compiler), "--emit-project-object", str(project),
                       "-" + profile, "-o", str(native_object)], root)
        if not require(emitted, f"NativeSharp {profile}", failures):
            continue
        native_dump = run([str(args.llvm_objdump), "-d", str(native_object)], root)
        if require(native_dump, f"NativeSharp {profile} disassembly", failures):
            native_text = native_dump.stdout
            for fragment in contract["required_native_disassembly"]:
                if fragment not in native_text:
                    failures.append(f"{profile}: NativeSharp object is missing {fragment}")

        for linker in contract["linkers"]:
            executable = output / f"specialized-{profile.lower()}-{linker}.exe"
            command = [str(args.nslink), "--link", str(native_object), str(assembly_object)]
            for dll, symbols in contract["imports"].items():
                command += ["--import", dll + ":" + ",".join(symbols)]
            command += ["--entry", contract["entry"], "-o", str(executable)]
            linked = run(command, root)
            if require(linked, f"{profile} {linker} link", failures):
                executed = run([str(executable)], output, timeout=10)
                if require(executed, f"{profile} {linker} run", failures):
                    cases.append({"profile": profile, "linker": linker,
                                  "status": "passed", "sha256": digest(executable)})

    expected = int(contract["expected_executions"])
    if len(cases) != expected:
        failures.append(f"execution matrix incomplete: {len(cases)}/{expected}")
    report = {
        "schema": "nativesharp-selfhost-specialized-code-report", "version": 1,
        "status": "failed" if failures else "passed", "profiles": contract["profiles"],
        "linkers": contract["linkers"], "surfaces": contract["surfaces"],
        "passed_executions": len(cases), "expected_executions": expected,
        "compiler_sha256": digest(args.compiler), "nsasm_sha256": digest(args.nsasm),
        "contract_sha256": digest(args.contract), "cases": cases, "failures": failures,
    }
    report_path = args.report or output / "report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if failures:
        for failure in failures:
            print("NSH4702: " + failure, file=sys.stderr)
        return 1
    print(f"SH47 specialized code verified: {len(cases)} O0-O3 executions with NSAsm")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
