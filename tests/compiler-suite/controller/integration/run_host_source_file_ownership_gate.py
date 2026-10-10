from __future__ import annotations

import argparse
import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from run_suite import run


INFRASTRUCTURE_PENDING = {
    "TS-SRC-008",
    "TS-SRC-040",
    "TS-SRC-041",
    "TS-SRC-049",
}


def read_source_inventory(inventory: Path) -> list[str]:
    with (inventory / "test-case-design.csv").open(
        newline="", encoding="utf-8-sig"
    ) as stream:
        return [
            row["case_id"]
            for row in csv.DictReader(stream)
            if row["component"] == "Host.Source"
        ]


def read_registered_source_ids(host_executable: Path) -> list[str]:
    completed = subprocess.run(
        [str(host_executable), "--list"],
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return sorted(
        line.split("\t", 1)[0]
        for line in completed.stdout.splitlines()
        if "\tHost.Source" in line
    )


def main() -> int:
    """Check the complete Source denominator and run bounded CS-SRC-002 cases."""

    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)
    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    source_ids = read_source_inventory(inventory)
    if source_ids != [f"TS-SRC-{ordinal:03d}" for ordinal in range(1, 59)]:
        raise RuntimeError("Host.Source canonical inventory is no longer TS-SRC-001..058")
    manifest_path = options.source_root / "manifest" / "cases.jsonl"
    manifests = [
        json.loads(line)
        for line in manifest_path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    manifest_ids = sorted(
        case["case_id"] for case in manifests if case["component"] == "Host.Source"
    )
    expected_executable = sorted(set(source_ids) - INFRASTRUCTURE_PENDING)
    if manifest_ids != expected_executable:
        raise RuntimeError("Host.Source manifest does not contain its exact 54 executable cases")
    if read_registered_source_ids(options.host_executable) != sorted(source_ids):
        raise RuntimeError("host registry does not contain the complete 58-case Host.Source denominator")
    with tempfile.TemporaryDirectory(
        prefix="host-source-file-ownership-", dir=options.work_parent
    ) as temporary:
        root = Path(temporary)
        code = run(
            [
                "--manifest", str(manifest_path),
                "--components", str(inventory / "components.csv"),
                "--contracts", str(inventory / "contracts.csv"),
                "--host-executable", str(options.host_executable),
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
        payload = json.loads(
            (root / "host-source-file-ownership" / "results.json").read_text(encoding="utf-8")
        )
        results = payload["results"]
        expected_ids = [
            "TS-SRC-003",
            "TS-SRC-004",
            "TS-SRC-005",
            "TS-SRC-006",
            "TS-SRC-007",
        ]
        if [result["case_id"] for result in results] != expected_ids:
            raise RuntimeError("controller did not execute the bounded TS-SRC-003..007 set")
        counts = payload["counts"]
        if counts["PASS"] != len(expected_ids) or any(
            count != 0 for status, count in counts.items() if status != "PASS"
        ):
            raise RuntimeError(f"unexpected SourceFile ownership result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
