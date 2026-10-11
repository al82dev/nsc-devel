from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from run_suite import run
from component_gate_support import component_manifest_rows
from selfhost_inventory import (
    load_component_inventory,
    validate_classified_manifest,
)


def read_registered_source_ids(
    registry_surfaces: tuple[tuple[Path, str], ...]
) -> list[str]:
    case_ids: list[str] = []
    for host_executable, selector in registry_surfaces:
        completed = subprocess.run(
            [str(host_executable), selector],
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        case_ids.extend(
            line.split("\t", 1)[0]
            for line in completed.stdout.splitlines()
            if "\tHost.Source" in line
        )
    return sorted(case_ids)


def main() -> int:
    """Check the complete Source denominator and run bounded CS-SRC-002 cases."""

    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-extended-host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)
    component_inventory = load_component_inventory(
        options.repository_root, "Host.Source"
    )
    inventory_root = (
        options.repository_root
        / "docs"
        / "compiler-testing-reengineering"
        / "inventory"
    )
    manifest_path = options.source_root / "manifest" / "cases.jsonl"
    manifests = component_manifest_rows(
        options.source_root, options.repository_root, "Host.Source"
    )
    classification_path = (
        options.source_root
        / "host"
        / "modules"
        / "HostSourceCaseClassifications.jsonl"
    )
    validate_classified_manifest(
        component_inventory,
        manifests,
        classification_path,
        frozenset({"InfrastructurePending"}),
    )
    if read_registered_source_ids(
        (
            (options.host_executable, "--list"),
            (options.source_extended_host_executable, "--list"),
            (options.host_executable, "--list-pending"),
        )
    ) != sorted(
        component_inventory.case_ids
    ):
        raise RuntimeError(
            "host registry does not contain the complete canonical Host.Source inventory"
        )
    with tempfile.TemporaryDirectory(
        prefix="host-source-file-ownership-", dir=options.work_parent
    ) as temporary:
        root = Path(temporary)
        code = run(
            [
                "--manifest", str(manifest_path),
                "--components", str(inventory_root / "components.csv"),
                "--contracts", str(inventory_root / "contracts.csv"),
                "--host-executable", str(options.host_executable),
                "--source-extended-host-executable",
                str(options.source_extended_host_executable),
                "--work-root", str(root),
                "--run-id", "host-source-file-ownership",
                "--revision", "phase-2-host-source-file-ownership",
                "--build-mode", options.build_mode,
                "--contract", "CS-SRC-002",
                "--profile", "NIGHTLY",
            ]
        )
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
