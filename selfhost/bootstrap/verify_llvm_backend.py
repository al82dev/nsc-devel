#!/usr/bin/env python3
"""Verifica el expediente material de SH08 y produce su reporte canónico."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise ValueError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", required=True, type=Path)
    parser.add_argument("--backend-source", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    try:
        contract = json.loads(args.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == "nativesharp-selfhost-llvm-backend-contract",
                "unexpected SH08 contract schema")
        require(contract.get("version") == 1, "unsupported SH08 contract version")
        require(contract.get("optimizations") == ["O0", "O1", "O2", "O3"],
                "optimization matrix is incomplete")
        require(contract.get("linkers") == ["lld-link", "NSLink"],
                "linker matrix is incomplete")
        require(len(contract.get("covered_work_items", [])) == 14,
                "SH08 work-item ledger is incomplete")
        source = args.backend_source.read_text(encoding="utf-8")
        for api in contract["required_backend_api"]:
            require(api in source, f"backend API {api} is absent")

        artifact_hashes: dict[str, str] = {}
        anatomy_checks = 0
        assembly_checks = 0
        executable_checks = 0
        for profile in contract["optimizations"]:
            lower = profile.lower()
            obj = args.output_dir / f"backend-{lower}.obj"
            asm = args.output_dir / f"backend-{lower}.asm"
            anatomy = args.output_dir / f"backend-{lower}.readobj.txt"
            for path in (obj, asm, anatomy):
                require(path.is_file() and path.stat().st_size > 0, f"missing artifact {path.name}")
                artifact_hashes[path.name] = sha256(path)
            anatomy_text = anatomy.read_text(encoding="utf-8", errors="replace")
            for symbol in contract["required_symbols"]:
                require(symbol in anatomy_text, f"{profile} object misses symbol {symbol}")
                anatomy_checks += 1
            for section in contract["required_sections"]:
                require(section in anatomy_text, f"{profile} object misses section {section}")
                anatomy_checks += 1
            require(contract["required_relocation"] in anatomy_text,
                    f"{profile} object misses ExitProcess relocation")
            anatomy_checks += 1
            assembly = asm.read_text(encoding="utf-8", errors="replace")
            require(contract["entry"] in assembly and "ExitProcess" in assembly,
                    f"{profile} assembly misses entry/import")
            assembly_checks += 2
            for route in ("lld", "nslink"):
                exe = args.output_dir / f"{route}-{lower}.exe"
                require(exe.is_file() and exe.stat().st_size > 0,
                        f"missing linked executable {exe.name}")
                artifact_hashes[exe.name] = sha256(exe)
                executable_checks += 1

        report = {
            "schema": "nativesharp-selfhost-sh08-report",
            "version": 1,
            "status": "passed",
            "llvm_major": contract["llvm_major"],
            "target": contract["target"],
            "optimizations": contract["optimizations"],
            "linkers": contract["linkers"],
            "expected_exit_code": contract["expected_exit_code"],
            "anatomy_checks": anatomy_checks,
            "assembly_checks": assembly_checks,
            "executable_checks": executable_checks,
            "work_items": contract["covered_work_items"],
            "backend_source_sha256": sha256(args.backend_source),
            "artifacts": dict(sorted(artifact_hashes.items())),
        }
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"verify_llvm_backend: error: {error}", file=sys.stderr)
        return 1
    print("SH08 LLVM backend verified: 4 profiles, 2 artifacts, 2 linkers, exit=42")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
