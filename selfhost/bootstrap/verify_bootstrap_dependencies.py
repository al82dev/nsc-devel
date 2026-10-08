#!/usr/bin/env python3

import argparse
import json
import re
import sys
from pathlib import Path


PROFILES = ("o0", "o1", "o2", "o3")
CHECK_NAMES = ("core", "collections", "io", "llvm-c")
DEPENDENCY_OWNERS = (
    "NativeSharpCore ",
    "NativeSharpCoreCollectionsGeneric ",
    "NativeSharpCoreIO ",
    "NativeSharpLLVM_C ",
    "NativeSharpCompilerNext ",
)
REQUIRED_METRICS = (
    "projects",
    "graph-units",
    "compiler-units",
    "external-projects",
    "external-resolutions",
)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    return parser.parse_args()


def parse_metrics(report: str, profile: str) -> dict[str, int]:
    values = dict(re.findall(r"^([a-z][a-z-]*)=(\d+)$", report, re.MULTILINE))
    missing = [name for name in REQUIRED_METRICS if name not in values]
    if missing:
        raise ValueError(f"{profile} dependency report misses {', '.join(missing)}")
    if "status=source-verified" not in report:
        raise ValueError(f"{profile} dependency report is not source-verified")
    metrics = {name: int(values[name]) for name in REQUIRED_METRICS}
    if any(metrics[name] <= 0 for name in REQUIRED_METRICS):
        raise ValueError(f"{profile} dependency report contains an empty metric")
    return metrics


def validate_graph(graph: str, profile: str) -> None:
    missing = [owner.strip() for owner in DEPENDENCY_OWNERS if owner not in graph]
    if missing:
        raise ValueError(f"{profile} misses dependency owners: {', '.join(missing)}")
    if ".obj" in graph or ".exe" in graph:
        raise ValueError(f"{profile} graph contains hidden binary input")


def parse_source_check(check: str, profile: str, name: str) -> int:
    match = re.search(r"^graph-units=(\d+) checked=(\d+) failed=(\d+)$", check, re.MULTILINE)
    if match is None:
        raise ValueError(f"{profile} {name} source check has no summary")
    graph_units, checked, failed = (int(value) for value in match.groups())
    if graph_units <= 0 or checked != graph_units or failed != 0:
        raise ValueError(
            f"{profile} {name} source check is incomplete: "
            f"graph-units={graph_units} checked={checked} failed={failed}"
        )
    return checked


def read_profile(output_dir: Path, profile: str) -> tuple[tuple[str, ...], dict[str, int], int]:
    report = (output_dir / f"dependencies-{profile}.txt").read_text(encoding="utf-8")
    graph = (output_dir / f"graph-{profile}.txt").read_text(encoding="utf-8")
    modules = (output_dir / f"modules-{profile}.txt").read_text(encoding="utf-8")
    metrics = parse_metrics(report, profile)
    validate_graph(graph, profile)

    check_texts: list[str] = []
    parsed_units = 0
    for name in CHECK_NAMES:
        check = (output_dir / f"check-{name}-{profile}.txt").read_text(encoding="utf-8")
        parsed_units += parse_source_check(check, profile, name)
        check_texts.append(check)
    return (report, graph, modules, *check_texts), metrics, parsed_units


def write_report(path: Path, metrics: dict[str, int], parsed_units: int) -> None:
    payload = {
        "schema": "nativesharp-selfhost-sh40-report",
        "version": 3,
        "gate": "SH40",
        "status": "passed",
        "projects": metrics["projects"],
        "units": metrics["graph-units"],
        "compiler_units": metrics["compiler-units"],
        "external_projects": metrics["external-projects"],
        "parsed_dependency_units": parsed_units,
        "external_resolutions": metrics["external-resolutions"],
        "problems": [],
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    arguments = parse_arguments()
    try:
        canonical_content: tuple[str, ...] | None = None
        canonical_metrics: dict[str, int] | None = None
        canonical_parsed_units = 0
        for profile in PROFILES:
            content, metrics, parsed_units = read_profile(arguments.output_dir, profile)
            if canonical_content is None:
                canonical_content = content
                canonical_metrics = metrics
                canonical_parsed_units = parsed_units
            elif content != canonical_content:
                raise ValueError(f"{profile} dependency graph differs")
        if canonical_metrics is None:
            raise ValueError("no dependency profiles were inspected")
        write_report(arguments.report, canonical_metrics, canonical_parsed_units)
    except (OSError, ValueError) as error:
        print(f"verify_bootstrap_dependencies: error: {error}", file=sys.stderr)
        return 1
    print("SH40 source-only bootstrap dependency graph verified across O0-O3")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
