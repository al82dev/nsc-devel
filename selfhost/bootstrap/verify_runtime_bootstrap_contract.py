#!/usr/bin/env python3
"""Verifica los dos artefactos externos del runtime requeridos por el bootstrap."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def run(command: list[str]) -> tuple[int, str]:
    result = subprocess.run(
        command,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    return result.returncode, result.stdout + result.stderr


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", required=True, type=Path)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--nsc", required=True, type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    failures: list[str] = []
    if contract.get("schema") != "nativesharp-runtime-bootstrap-contract" or contract.get("version") != 1:
        failures.append("unsupported runtime bootstrap contract schema")

    library = args.prefix / contract["runtime_library"]
    abi = args.prefix / contract["runtime_abi_contract"]
    if not library.is_file():
        failures.append(f"runtime library is absent: {library}")
    if not abi.is_file():
        failures.append(f"runtime ABI contract is absent: {abi}")

    abi_text = abi.read_text(encoding="utf-8") if abi.is_file() else ""
    if not abi_text.startswith("NSRUNTIMEABI " + contract["runtime_abi"] + "\n"):
        failures.append("runtime ABI version differs from bootstrap contract")
    if f"symbol_set={contract['runtime_symbol_set']}\n" not in abi_text:
        failures.append("runtime symbol set differs from bootstrap contract")
    if f"target={contract['target']}\n" not in abi_text:
        failures.append("runtime target differs from bootstrap contract")
    for symbol in contract["required_symbols"]:
        if f" {symbol} " not in abi_text:
            failures.append(f"runtime ABI is missing required bootstrap symbol {symbol}")

    for option, artifact in (
        ("--verify-runtime-abi-contract", abi),
        ("--verify-windows-runtime-library", library),
    ):
        if artifact.is_file():
            code, output = run([str(args.nsc), option, str(artifact)])
            if code != 0:
                failures.append(f"{option} failed: {output.strip()}")

    report = {
        "schema": "nativesharp-selfhost-runtime-bootstrap-report",
        "version": 1,
        "status": "failed" if failures else "passed",
        "runtime_abi": contract["runtime_abi"],
        "runtime_symbol_set": contract["runtime_symbol_set"],
        "required_symbol_count": len(contract["required_symbols"]),
        "runtime_library_sha256": sha256(library) if library.is_file() else None,
        "runtime_abi_sha256": sha256(abi) if abi.is_file() else None,
        "failures": failures,
    }
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if failures:
        for failure in failures:
            print("NSH0303: " + failure, file=sys.stderr)
        return 1
    print("SH03 runtime bootstrap contract verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
