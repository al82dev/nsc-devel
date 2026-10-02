#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify source-derived NativeSharp LLVM objects.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--backend-source", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        contract = json.loads(arguments.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == "nativesharp-selfhost-source-derived-backend", "bad contract")
        require(contract.get("source_derived_codegen") is True, "source-derived flag must be true")
        backend = arguments.backend_source.read_text(encoding="utf-8")
        for fragment in ("EmitScalarExpression", "SyntaxTree tree", "LLVMBuildAdd", "LLVMBuildSub",
                         "LLVMBuildMul", "LLVMBuildSDiv"):
            require(fragment in backend, f"backend misses {fragment}")
        require("void* six =" not in backend, "fixed canonical computation remains")

        artifacts: dict[str, str] = {}
        executions: list[dict[str, object]] = []
        for profile in contract["profiles"]:
            lower = profile.lower()
            hashes = []
            for expected in contract["expected_exit_codes"]:
                obj = arguments.output_dir / f"source-{expected}-{lower}.obj"
                require(obj.is_file() and obj.stat().st_size > 0, f"missing {obj.name}")
                object_hash = digest(obj)
                hashes.append(object_hash)
                artifacts[obj.name] = object_hash
                for linker in contract["linkers"]:
                    executable = arguments.output_dir / f"{linker}-{expected}-{lower}.exe"
                    require(executable.is_file(), f"missing {executable.name}")
                    completed = subprocess.run([str(executable)], check=False)
                    require(completed.returncode == expected,
                            f"{executable.name} returned {completed.returncode}, expected {expected}")
                    artifacts[executable.name] = digest(executable)
                    executions.append({"profile": profile, "linker": linker,
                                       "expected": expected, "actual": completed.returncode})
            require(hashes[0] != hashes[1], f"{profile} objects do not depend on source")

        report = {
            "schema": "nativesharp-selfhost-sh16-report",
            "version": 1,
            "gate": "SH16",
            "status": "passed",
            "source_derived_codegen": True,
            "scope": "integer-entry-expression",
            "profiles": contract["profiles"],
            "linkers": contract["linkers"],
            "executions": executions,
            "artifacts": dict(sorted(artifacts.items())),
            "backend_source_sha256": digest(arguments.backend_source),
            "problems": [],
        }
        arguments.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"verify_source_derived_backend: error: {error}", file=sys.stderr)
        return 1
    print("SH16 source-derived backend verified: 2 sources, O0/O3, NSLink")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
