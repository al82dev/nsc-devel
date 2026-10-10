from __future__ import annotations

import argparse
import csv
import json
import subprocess
from pathlib import Path


EXPECTED_IDS = [f"TF-NSC-{ordinal:03d}" for ordinal in range(1, 161)]
EXPECTED_CONTRACTS = [
    f"CF-NSC-{contract:03d}"
    for contract in range(1, 21)
    for _variant in range(1, 9)
]


def read_csv(path: Path) -> list[dict[str, str]]:
    """Load one canonical UTF-8 inventory while preserving its published row order."""
    with path.open("r", encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def listed_nsc_cases(host_executable: Path) -> list[tuple[str, str]]:
    """Read registered Host.nsc identities without selecting or executing any test body."""
    completed = subprocess.run(
        [str(host_executable), "--list"],
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=30,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"host registry listing returned {completed.returncode}: {completed.stderr.strip()}"
        )
    listed: list[tuple[str, str]] = []
    for line in completed.stdout.splitlines():
        fields = line.split("\t")
        if len(fields) == 3 and fields[2] == "Host.nsc":
            listed.append((fields[0], fields[1]))
    return listed


def assert_no_required_nsc_cases(host_executable: Path) -> None:
    """Prove the pending inventory was not accidentally promoted to ordinary execution."""
    completed = subprocess.run(
        [str(host_executable), "--component", "Host.nsc"],
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=30,
        check=False,
    )
    expected = "component has no test cases: Host.nsc"
    if completed.returncode == 0 or expected not in completed.stderr:
        raise RuntimeError(
            "Host.nsc contains selectable cases even though every current case is "
            "InfrastructurePending"
        )


def main() -> int:
    """Check the canonical denominator, pending registration, and empty executable fragment."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()

    inventory = (
        options.repository_root
        / "docs"
        / "compiler-testing-reengineering"
        / "inventory"
    )
    design = [
        row
        for row in read_csv(inventory / "test-case-design.csv")
        if row["component"] == "Host.nsc"
    ]
    allocation = [
        row
        for row in read_csv(inventory / "suite-case-allocation.csv")
        if row["component"] == "Host.nsc"
    ]
    if [row["case_id"] for row in design] != EXPECTED_IDS:
        raise RuntimeError("test-case design does not contain canonical TF-NSC-001..160")
    if [row["contract_id"] for row in design] != EXPECTED_CONTRACTS:
        raise RuntimeError("test-case design does not preserve eight cases per Host.nsc contract")
    if [row["case_id"] for row in allocation] != EXPECTED_IDS:
        raise RuntimeError("suite allocation does not contain canonical TF-NSC-001..160")
    layers = [row["layer"] for row in allocation]
    if layers.count("unit") != 80 or layers.count("process") != 80:
        raise RuntimeError("Host.nsc layer distribution is not the canonical 80 unit and 80 process")

    manifest_path = options.source_root / "manifest" / "cases.jsonl"
    manifests = [
        json.loads(line)
        for line in manifest_path.read_text(encoding="utf-8").splitlines()
    ]
    if any(case["component"] == "Host.nsc" for case in manifests):
        raise RuntimeError(
            "Host.nsc has no executable cases yet; its manifest fragment must remain empty"
        )

    listed = listed_nsc_cases(options.host_executable)
    if listed != list(zip(EXPECTED_IDS, EXPECTED_CONTRACTS, strict=True)):
        raise RuntimeError("host registry does not expose the complete ordered Host.nsc inventory")
    assert_no_required_nsc_cases(options.host_executable)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
