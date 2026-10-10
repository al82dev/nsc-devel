from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))
from run_suite import run


SPECIAL_INFRASTRUCTURE_CASES = {
    "TS-SEMA-077",
    "TS-SEMA-094",
    "TS-SEMA-101",
    "TS-SEMA-265",
    "TS-SEMA-281",
    "TS-SEMA-306",
    "TS-SEMA-314",
    "TS-SEMA-321",
}

KNOWN_PRODUCT_DEFECTS = {
    "TS-SEMA-106",
    "TS-SEMA-110",
    "TS-SEMA-246",
    "TS-SEMA-317",
}


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
    manifests = [json.loads(line) for line in manifest_path.read_text(encoding="utf-8").splitlines()]
    sema_manifests = [case for case in manifests if case["component"] == "Host.Sema"]
    expected_ids = [
        f"TS-SEMA-{number:03d}"
        for number in range(1, 322)
        if f"TS-SEMA-{number:03d}" not in SPECIAL_INFRASTRUCTURE_CASES
    ]
    if [case["case_id"] for case in sema_manifests] != expected_ids:
        raise RuntimeError("manifest does not contain the complete executable Host.Sema case set")

    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    with tempfile.TemporaryDirectory(prefix="host-sema-", dir=options.work_parent) as temporary:
        root = Path(temporary)
        code = run([
            "--manifest", str(manifest_path),
            "--components", str(inventory / "components.csv"),
            "--contracts", str(inventory / "contracts.csv"),
            "--host-executable", str(options.host_executable),
            "--work-root", str(root),
            "--run-id", "host-sema",
            "--revision", "phase-2-host-sema",
            "--build-mode", options.build_mode,
            "--component", "Host.Sema",
            "--profile", "NIGHTLY",
        ])
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        payload = json.loads((root / "host-sema" / "results.json").read_text(encoding="utf-8"))
        if [result["case_id"] for result in payload["results"]] != expected_ids:
            raise RuntimeError("controller did not execute the complete executable Host.Sema case set")
        counts = payload["counts"]
        unexpected = any(
            count != 0
            for status, count in counts.items()
            if status not in {"PASS", "KNOWN_FAILURE"}
        )
        if (
            counts["PASS"] != len(expected_ids) - len(KNOWN_PRODUCT_DEFECTS)
            or counts["KNOWN_FAILURE"] != len(KNOWN_PRODUCT_DEFECTS)
            or unexpected
        ):
            raise RuntimeError(f"unexpected Host.Sema result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
