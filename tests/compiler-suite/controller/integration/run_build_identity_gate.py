from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))

from run_suite import run


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    parser.add_argument("--nsc-executable", type=Path, required=True)
    parser.add_argument("--comparison-nsc-executable", type=Path, required=True)
    parser.add_argument("--build-id-header", type=Path, required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)
    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    with tempfile.TemporaryDirectory(
        prefix="host-basic-build-identity-", dir=options.work_parent
    ) as temporary:
        root = Path(temporary)
        code = run(
            [
                "--manifest", str(options.source_root / "manifest" / "cases.jsonl"),
                "--components", str(inventory / "components.csv"),
                "--contracts", str(inventory / "contracts.csv"),
                "--nsc-executable", str(options.nsc_executable),
                "--comparison-nsc-executable", str(options.comparison_nsc_executable),
                "--build-id-header", str(options.build_id_header),
                "--work-root", str(root),
                "--run-id", "host-basic-build-identity",
                "--revision", "phase-2-host-basic-build-identity",
                "--build-mode", options.build_mode,
                "--contract", "CB-BAS-006",
                "--profile", "NIGHTLY",
            ]
        )
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        payload = json.loads(
            (root / "host-basic-build-identity" / "results.json").read_text(
                encoding="utf-8"
            )
        )
        results = payload["results"]
        expected_ids = [f"TB-BAS-{index:03d}" for index in range(20, 24)]
        if [result["case_id"] for result in results] != expected_ids:
            raise RuntimeError("controller did not execute TB-BAS-020..023")
        counts = payload["counts"]
        if counts["PASS"] != 4 or any(
            count != 0 for status, count in counts.items() if status != "PASS"
        ):
            raise RuntimeError(f"unexpected build identity result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
