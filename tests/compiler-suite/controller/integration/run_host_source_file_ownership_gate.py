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
    """Run the bounded CS-SRC-002 cases through the canonical controller."""

    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)
    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    with tempfile.TemporaryDirectory(
        prefix="host-source-file-ownership-", dir=options.work_parent
    ) as temporary:
        root = Path(temporary)
        code = run(
            [
                "--manifest", str(options.source_root / "manifest" / "cases.jsonl"),
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
