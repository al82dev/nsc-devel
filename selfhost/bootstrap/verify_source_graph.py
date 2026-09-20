#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def parse_dump(text: str) -> dict[str, object]:
    lines = text.splitlines()
    require(bool(lines) and lines[0] == "source-graph schema=1", "bad dump header")
    header: dict[str, str] = {}
    units: list[tuple[int, str, str]] = []
    for line in lines[1:]:
        if line.startswith("unit "):
            parts = line.split(" ", 3)
            require(len(parts) == 4, f"bad unit line: {line}")
            units.append((int(parts[1]), parts[2], parts[3]))
        elif "=" in line:
            key, _, value = line.partition("=")
            header[key] = value
    return {"header": header, "units": units}


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify the NativeSharp deterministic source graph.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--graph-source", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        contract = json.loads(arguments.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == "nativesharp-selfhost-source-graph-contract", "bad contract")

        source = arguments.graph_source.read_text(encoding="utf-8")
        for symbol in contract["required_source_symbols"]:
            require(symbol in source, f"source graph misses {symbol}")

        artifacts: dict[str, str] = {}
        canonical: str | None = None
        checks: list[dict[str, object]] = []

        for profile in contract["profiles"]:
            lower = profile.lower()
            dump_path = arguments.output_dir / f"source-graph-{lower}.txt"
            check_path = arguments.output_dir / f"check-project-{lower}.txt"
            require(dump_path.is_file() and check_path.is_file(), f"missing {profile} artifacts")

            dump_text = dump_path.read_text(encoding="utf-8")
            # The whole point of the gate: the dump may not move with the optimization level.
            if canonical is None:
                canonical = dump_text
            else:
                require(dump_text == canonical, f"{profile} dump differs from O0")

            parsed = parse_dump(dump_text)
            header = parsed["header"]
            units = parsed["units"]
            require(header.get("status") == contract["expected_status"], f"{profile} status")
            require(int(header["units"]) == len(units), f"{profile} unit count disagrees with the lines")
            require(int(header["projects"]) == len(contract["expected_projects"]),
                    f"{profile} project count")
            require([ordinal for ordinal, _, _ in units] == list(range(len(units))),
                    f"{profile} ordinals are not dense and ascending")

            owners: list[str] = []
            for _, owner, _ in units:
                if owner not in owners:
                    owners.append(owner)
            require(owners == contract["expected_projects"],
                    f"{profile} project order is {owners}")

            root = [relative for _, owner, relative in units if owner == contract["root_owner"]]
            require(root == contract["root_units"], f"{profile} root unit list is {root}")

            check_text = check_path.read_text(encoding="utf-8")
            summary = [line for line in check_text.splitlines() if line.startswith("graph-units=")]
            require(len(summary) == 1, f"{profile} check summary missing")
            require(summary[0] == f"graph-units={len(units)} checked={len(root)} failed=0",
                    f"{profile} check: {summary[0]}")

            artifacts[dump_path.name] = digest(dump_path)
            artifacts[check_path.name] = digest(check_path)
            checks.append({"profile": profile, "units": len(units), "root_units": len(root)})

        report = {
            "schema": "nativesharp-selfhost-sh25-report", "version": 1, "gate": "SH25",
            "status": "passed", "source_derived_codegen": False, "scope": contract["scope"],
            "profiles": contract["profiles"], "projects": contract["expected_projects"],
            "root_owner": contract["root_owner"], "root_unit_count": len(contract["root_units"]),
            "checks": checks, "artifacts": dict(sorted(artifacts.items())),
            "graph_source_sha256": digest(arguments.graph_source), "problems": [],
        }
        arguments.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"verify_source_graph: error: {error}", file=sys.stderr)
        return 1
    print("SH25 source graph verified: identical across O0-O3, canonical order, per-unit lexer/parser diagnostics")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
