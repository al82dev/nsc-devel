#!/usr/bin/env python3
"""Verifica el inventario LLVM-C de nsc0 y la frontera NativeSharp congelada por SH03."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path


def digest(path: Path) -> str:
    canonical = path.read_text(encoding="utf-8").replace("\r\n", "\n").replace("\r", "\n")
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def imported_llvm_symbols(readobj: Path, executable: Path, library: str) -> list[str]:
    run = subprocess.run(
        [str(readobj), "--coff-imports", str(executable)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if run.returncode != 0:
        raise RuntimeError(run.stdout + run.stderr)
    current = ""
    result: set[str] = set()
    for line in run.stdout.splitlines():
        match = re.match(r"^\s*Name:\s*(\S+)\s*$", line)
        if match:
            current = match.group(1)
            continue
        match = re.match(r"^\s*Symbol:\s*(\S+)", line)
        if match and current.casefold() == library.casefold():
            result.add(match.group(1))
    return sorted(result, key=str.casefold)


def declared_binding_symbols(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    return sorted(
        set(re.findall(r"public\s+[A-Za-z0-9_*]+\s+(LLVM[A-Za-z0-9_]+)\s*\(", text)),
        key=str.casefold,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", required=True, type=Path)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--nsc", required=True, type=Path)
    parser.add_argument("--llvm-readobj", required=True, type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    failures: list[str] = []
    if contract.get("schema") != "nativesharp-llvm-c-api-contract" or contract.get("version") != 2:
        failures.append("unsupported LLVM C contract schema")
    if contract.get("source_hash_canonicalization") != "utf-8-lf":
        failures.append("unsupported LLVM C source hash canonicalization")

    dynamic_imports = imported_llvm_symbols(
        args.llvm_readobj, args.nsc, contract["forbidden_dynamic_library"])
    if contract.get("linkage") != "static-llvm-components":
        failures.append("SH03 requires static LLVM component linkage")
    if dynamic_imports:
        failures.append("nsc imports forbidden LLVM-C.dll symbols: " + repr(dynamic_imports))

    signature_source = args.source_root / contract["signature_source"]
    enum_source = args.source_root / contract["enum_source"]
    declared = declared_binding_symbols(signature_source)
    if not set(contract["bootstrap_symbols"]).issubset(declared):
        failures.append("NativeSharp LLVM binding lost an SH03 bootstrap symbol")
    audited_bindings = set(contract["audited_binding_symbols"]) | set(
        contract.get("bootstrap_extension_symbols", []))
    if not set(declared).issubset(audited_bindings):
        failures.append("NativeSharp LLVM binding declares symbols outside the audited nsc0 inventory")
    if digest(signature_source) != contract["signature_source_sha256"]:
        failures.append("NativeSharp LLVM signatures changed without contract update")
    if digest(enum_source) != contract["enum_source_sha256"]:
        failures.append("NativeSharp LLVM enums changed without contract update")

    report = {
        "schema": "nativesharp-selfhost-llvm-c-contract-report",
        "version": 1,
        "status": "failed" if failures else "passed",
        "llvm_major": contract["supported_llvm_major"],
        "linkage": contract["linkage"],
        "forbidden_dynamic_import_count": len(dynamic_imports),
        "audited_binding_symbol_count": len(contract["audited_binding_symbols"]),
        "bootstrap_symbol_count": len(contract["bootstrap_symbols"]),
        "bootstrap_extension_symbol_count": len(contract.get("bootstrap_extension_symbols", [])),
        "binding_symbol_count": len(declared),
        "failures": failures,
    }
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if failures:
        for failure in failures:
            print("NSH0302: " + failure, file=sys.stderr)
        return 1
    print(
        "SH03 LLVM contract verified: static component linkage, no LLVM-C.dll imports, "
        f"{len(contract['bootstrap_symbols'])} bootstrap and {len(declared)} bound symbols"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
