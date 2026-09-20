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
    parser = argparse.ArgumentParser(description="Verify NativeSharp source loop control.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--backend-source", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        contract = json.loads(arguments.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == "nativesharp-selfhost-source-loop-control", "bad contract")
        backend = arguments.backend_source.read_text(encoding="utf-8")
        for api in contract["required_llvm_api"]:
            require(api in backend, f"backend misses {api}")
        for marker in ("ForStatement", "DoWhileStatement", "BreakStatement", "ContinueStatement"):
            require(marker in backend, f"backend misses {marker}")
        artifacts: dict[str, str] = {}
        executions: list[dict[str, object]] = []
        for profile in contract["profiles"]:
            lower = profile.lower()
            obj = arguments.output_dir / f"loops-{lower}.obj"
            anatomy = arguments.output_dir / f"loops-{lower}.readobj.txt"
            require(obj.is_file() and anatomy.is_file(), f"missing {profile} artifacts")
            text = anatomy.read_text(encoding="utf-8", errors="replace")
            for symbol in contract["required_symbols"]:
                require(symbol in text, f"{profile} misses {symbol}")
            artifacts[obj.name] = digest(obj)
            artifacts[anatomy.name] = digest(anatomy)
            for linker in contract["linkers"]:
                executable = arguments.output_dir / f"{linker}-{lower}.exe"
                completed = subprocess.run([str(executable)], check=False)
                require(completed.returncode == contract["expected_exit_code"],
                        f"{executable.name} returned {completed.returncode}")
                artifacts[executable.name] = digest(executable)
                executions.append({"profile": profile, "linker": linker,
                                   "actual": completed.returncode})
        report = {
            "schema": "nativesharp-selfhost-sh19-report", "version": 1, "gate": "SH19",
            "status": "passed", "source_derived_codegen": True, "scope": contract["scope"],
            "profiles": contract["profiles"], "linkers": contract["linkers"],
            "executions": executions, "artifacts": dict(sorted(artifacts.items())),
            "backend_source_sha256": digest(arguments.backend_source), "problems": [],
        }
        arguments.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"verify_source_loop_control: error: {error}", file=sys.stderr)
        return 1
    print("SH19 source loop control verified: for, do, break and continue across O0-O3")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
