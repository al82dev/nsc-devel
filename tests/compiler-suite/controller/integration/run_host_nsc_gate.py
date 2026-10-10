from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from selfhost_inventory import load_component_inventory, read_jsonl


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

    inventory = load_component_inventory(options.repository_root, "Host.nsc")
    expected = sorted(
        (case_id, row["contract_id"])
        for case_id, row in inventory.design_by_id.items()
    )

    manifest_path = options.source_root / "manifest" / "cases.jsonl"
    manifests = read_jsonl(manifest_path)
    if any(case["component"] == "Host.nsc" for case in manifests):
        raise RuntimeError(
            "Host.nsc has no executable cases yet; its manifest fragment must remain empty"
        )

    listed = listed_nsc_cases(options.host_executable)
    if listed != expected:
        raise RuntimeError("host registry does not expose the complete ordered Host.nsc inventory")
    assert_no_required_nsc_cases(options.host_executable)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
