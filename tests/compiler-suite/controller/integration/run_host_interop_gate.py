from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))
from run_suite import run


EXECUTABLE_CONTRACTS = (
    *range(11, 19),
    22,
    *range(24, 29),
    34,
    35,
    39,
    43,
    44,
    48,
)


def _expected_ids() -> list[str]:
    identifiers: list[str] = []
    for contract in EXECUTABLE_CONTRACTS:
        first = ((contract - 1) * 8) + 1
        for variant in range(8):
            identifiers.append(f"TF-INTEROP-{first + variant:03d}")
    return identifiers


EXPECTED_IDS = _expected_ids()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)

    manifest_path = options.source_root / "manifest" / "cases.jsonl"
    manifests = [
        json.loads(line)
        for line in manifest_path.read_text(encoding="utf-8").splitlines()
    ]
    interop_manifests = [
        case for case in manifests if case["component"] == "Host.Interop"
    ]
    if [case["case_id"] for case in interop_manifests] != EXPECTED_IDS:
        raise RuntimeError(
            "manifest does not contain exactly the 160 executable Host.Interop "
            "cases in canonical order"
        )

    inventory = (
        options.repository_root
        / "docs"
        / "compiler-testing-reengineering"
        / "inventory"
    )
    with tempfile.TemporaryDirectory(
        prefix="host-interop-", dir=options.work_parent
    ) as temporary:
        root = Path(temporary)
        code = run(
            [
                "--manifest",
                str(manifest_path),
                "--components",
                str(inventory / "components.csv"),
                "--contracts",
                str(inventory / "contracts.csv"),
                "--host-executable",
                str(options.host_executable),
                "--work-root",
                str(root),
                "--run-id",
                "host-interop",
                "--revision",
                "phase-2-host-interop",
                "--build-mode",
                options.build_mode,
                "--component",
                "Host.Interop",
                "--profile",
                "NIGHTLY",
            ]
        )
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        payload = json.loads(
            (root / "host-interop" / "results.json").read_text(encoding="utf-8")
        )
        if [result["case_id"] for result in payload["results"]] != EXPECTED_IDS:
            raise RuntimeError(
                "controller did not execute exactly the 160 executable "
                "Host.Interop cases in canonical order"
            )
        counts = payload["counts"]
        unexpected = any(
            count != 0 for status, count in counts.items() if status != "PASS"
        )
        if counts["PASS"] != len(EXPECTED_IDS) or unexpected:
            raise RuntimeError(f"unexpected Host.Interop result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
