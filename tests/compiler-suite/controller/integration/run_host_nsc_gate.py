from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from component_gate_support import component_manifest_rows
from selfhost_inventory import validate_empty_component_universe


def listed_nsc_cases(host_executable: Path) -> list[tuple[str, str]]:
    """Read registered Host.nsc identities without selecting or executing any test body."""
    completed = subprocess.run(
        [str(host_executable), "--list"],
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
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
    """Prove the retired component was not reintroduced into ordinary execution."""
    completed = subprocess.run(
        [str(host_executable), "--component", "Host.nsc"],
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        check=False,
    )
    expected = "component has no test cases: Host.nsc"
    if completed.returncode == 0 or expected not in completed.stderr:
        raise RuntimeError(
            "Host.nsc contains selectable cases outside the active executable universe"
        )


def main() -> int:
    """Check that Host.nsc remains absent from inventory, manifest, and registry."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    options = parser.parse_args()

    validate_empty_component_universe(
        options.repository_root, "Host.nsc", "host-nsc.jsonl"
    )
    manifests = component_manifest_rows(
        options.source_root, options.repository_root, "Host.nsc"
    )
    if manifests:
        raise RuntimeError(
            "Host.nsc has no executable cases yet; its manifest fragment must remain empty"
        )

    listed = listed_nsc_cases(options.host_executable)
    if listed:
        raise RuntimeError("host registry exposes Host.nsc outside the active inventory")
    assert_no_required_nsc_cases(options.host_executable)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
