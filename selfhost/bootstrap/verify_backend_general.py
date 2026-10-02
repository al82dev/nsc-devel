#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


def require(value: bool, message: str) -> None:
    if not value:
        raise ValueError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify the SH27 general project backend.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--backend-source", type=Path, required=True)
    parser.add_argument("--driver-source", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        contract = json.loads(args.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == "nativesharp-selfhost-backend-general-contract", "bad contract")
        sources = args.backend_source.read_text(encoding="utf-8") + args.driver_source.read_text(encoding="utf-8")
        for symbol in contract["required_source_symbols"]:
            require(symbol in sources, f"implementation misses {symbol}")

        canonical: list[str] | None = None
        artifacts: dict[str, str] = {}
        checks: list[dict[str, object]] = []
        for profile in contract["profiles"]:
            lower = profile.lower()
            inspection = args.output_dir / f"backend-{lower}.readobj.txt"
            text = inspection.read_text(encoding="utf-8")
            require(contract["forbidden_symbol_prefix"] not in text,
                    f"{profile} retains historical source-only mangling")
            names: list[str] = []
            for function in contract["expected_functions"]:
                matches = re.findall(rf"__nativesharp_[0-9]+_{re.escape(function)}", text)
                require(len(set(matches)) == 1, f"{profile} {function} symbols are {matches}")
                names.append(matches[0])
            if canonical is None:
                canonical = names
            else:
                require(names == canonical, f"{profile} mangling differs from O0")
            for route in contract["routes"]:
                result_path = args.output_dir / f"{route}-{lower}.result.txt"
                result = int(result_path.read_text(encoding="ascii").strip())
                require(result == contract["expected_exit_code"],
                        f"{profile}/{route} returned {result}")
                artifacts[result_path.name] = digest(result_path)
            artifacts[inspection.name] = digest(inspection)
            checks.append({"profile": profile, "symbols": names, "routes": contract["routes"]})

        report = {
            "schema": "nativesharp-selfhost-sh27-report", "version": 1, "gate": "SH27",
            "status": "passed", "profiles": contract["profiles"], "checks": checks,
            "artifacts": dict(sorted(artifacts.items())), "problems": []
        }
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"verify_backend_general: error: {error}", file=sys.stderr)
        return 1
    print("SH27 general project backend verified: O0-O3, NSLink, stable ABI names")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
