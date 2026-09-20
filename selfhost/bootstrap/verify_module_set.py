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
    require(bool(lines) and lines[0] == "module-set schema=1", "bad dump header")
    header: dict[str, str] = {}
    units: list[tuple[int, str, int, int, int]] = []
    external: list[str] = []
    problems: list[str] = []
    for line in lines[1:]:
        if line.startswith("unit "):
            parts = line.split(" ")
            require(len(parts) == 6, f"bad unit line: {line}")
            units.append((int(parts[1]), parts[2],
                          int(parts[3].removeprefix("lex=")),
                          int(parts[4].removeprefix("parse=")),
                          int(parts[5].removeprefix("external="))))
        elif line.startswith("external "):
            external.append(line[len("external "):])
        elif line.startswith("problem "):
            problems.append(line[len("problem "):])
        elif "=" in line:
            key, _, value = line.partition("=")
            header[key] = value
    return {"header": header, "units": units, "external": external, "problems": problems}


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify the NativeSharp cross-unit module set.")
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--module-source", type=Path, required=True)
    parser.add_argument("--analyzer-source", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        contract = json.loads(arguments.contract.read_text(encoding="utf-8"))
        require(contract.get("schema") == "nativesharp-selfhost-module-set-contract", "bad contract")

        module_source = arguments.module_source.read_text(encoding="utf-8")
        for symbol in contract["required_source_symbols"]:
            require(symbol in module_source, f"module set misses {symbol}")
        analyzer_source = arguments.analyzer_source.read_text(encoding="utf-8")
        for symbol in contract["required_analyzer_symbols"]:
            require(symbol in analyzer_source, f"analyzer misses {symbol}")

        artifacts: dict[str, str] = {}
        canonical_set: str | None = None
        canonical_exports: str | None = None
        checks: list[dict[str, object]] = []

        for profile in contract["profiles"]:
            lower = profile.lower()
            dump_path = arguments.output_dir / f"module-set-{lower}.txt"
            exports_path = arguments.output_dir / f"module-exports-{lower}.txt"
            positive_path = arguments.output_dir / f"positive-set-{lower}.txt"
            positive_exports_path = arguments.output_dir / f"positive-exports-{lower}.txt"
            negative_path = arguments.output_dir / f"negative-set-{lower}.txt"
            require(dump_path.is_file() and exports_path.is_file() and positive_path.is_file() and
                    positive_exports_path.is_file() and negative_path.is_file(),
                    f"missing {profile} artifacts")

            dump_text = dump_path.read_text(encoding="utf-8")
            exports_text = exports_path.read_text(encoding="utf-8")
            # Neither surface may move with the optimization level.
            if canonical_set is None:
                canonical_set = dump_text
                canonical_exports = exports_text
            else:
                require(dump_text == canonical_set, f"{profile} module-set dump differs from O0")
                require(exports_text == canonical_exports, f"{profile} exports dump differs from O0")

            parsed = parse_dump(dump_text)
            header = parsed["header"]
            units = parsed["units"]
            require(header.get("status") == contract["expected_status"], f"{profile} status")
            require(int(header["units"]) == len(units), f"{profile} unit count disagrees with the lines")
            require(len(units) == contract["expected_units"],
                    f"{profile} compiled {len(units)} units, contract says {contract['expected_units']}")
            require(parsed["external"] == contract["expected_external_projects"],
                    f"{profile} external projects are {parsed['external']}")
            require(int(header["external-metadata"]) == contract["expected_external_metadata"],
                    f"{profile} external metadata count is {header['external-metadata']}")
            require(len(parsed["problems"]) == contract["expected_problems"],
                    f"{profile} identity problems: {parsed['problems']}")

            front_end = sum(lex + parse for _, _, lex, parse, _ in units)
            require(front_end == contract["expected_front_end_errors"],
                    f"{profile} front-end errors: {front_end}")
            external_resolutions = sum(external for _, _, _, _, external in units)
            require(int(header["cross-unit-resolutions"]) == external_resolutions,
                    f"{profile} cross-unit resolution count disagrees with units")
            require(external_resolutions >= contract["expected_min_cross_unit_resolutions"],
                    f"{profile} resolved only {external_resolutions} cross-unit references")

            require(exports_text.splitlines()[0] == "module-exports schema=1", f"{profile} exports header")
            exported = [line for line in exports_text.splitlines() if line.startswith("export ")]
            require(int(header["exports"]) == len(exported),
                    f"{profile} export count disagrees between the two surfaces")

            positive = parse_dump(positive_path.read_text(encoding="utf-8"))
            require(len(positive["units"]) == contract["expected_positive_units"],
                    f"{profile} positive fixture units")
            require(not positive["problems"], f"{profile} positive identity problems")
            require(int(positive["header"]["external-metadata"]) ==
                    contract["expected_positive_external_metadata"],
                    f"{profile} positive fixture did not consume its external metadata")
            require(int(positive["header"]["cross-unit-resolutions"]) >= 1,
                    f"{profile} positive fixture did not resolve across units")
            positive_exports = positive_exports_path.read_text(encoding="utf-8").splitlines()
            overload_prefix = f"export Function {contract['expected_positive_overload']} "
            overloads = [line for line in positive_exports if line.startswith(overload_prefix)]
            require(len(overloads) == 2 and len(set(overloads)) == 2,
                    f"{profile} overload set is {overloads}")
            external_symbol = contract["expected_positive_external_symbol"]
            require(any(external_symbol in line for line in positive_exports),
                    f"{profile} metadata symbol was not indexed: {external_symbol}")

            negative = parse_dump(negative_path.read_text(encoding="utf-8"))
            require(negative["problems"] == [contract["expected_negative_problem"]],
                    f"{profile} negative identity problems are {negative['problems']}")

            artifacts[dump_path.name] = digest(dump_path)
            artifacts[exports_path.name] = digest(exports_path)
            artifacts[positive_path.name] = digest(positive_path)
            artifacts[positive_exports_path.name] = digest(positive_exports_path)
            artifacts[negative_path.name] = digest(negative_path)
            checks.append({"profile": profile, "units": len(units), "exports": len(exported),
                           "cross_unit_resolutions": external_resolutions})

        report = {
            "schema": "nativesharp-selfhost-sh26-report", "version": 1, "gate": "SH26",
            "status": "passed", "source_derived_codegen": False, "scope": contract["scope"],
            "profiles": contract["profiles"], "root_owner": contract["root_owner"],
            "external_projects": contract["expected_external_projects"],
            "checks": checks, "artifacts": dict(sorted(artifacts.items())),
            "module_source_sha256": digest(arguments.module_source),
            "analyzer_source_sha256": digest(arguments.analyzer_source), "problems": [],
        }
        arguments.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, ValueError, KeyError, IndexError, json.JSONDecodeError) as error:
        print(f"verify_module_set: error: {error}", file=sys.stderr)
        return 1
    print("SH26 module set verified: cross-unit identity, stable across O0-O3")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
