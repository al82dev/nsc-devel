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
    "TP-PAR-514",
    "TP-PAR-516",
    "TP-PAR-524",
}

KNOWN_PRODUCT_DEFECTS = {
    "TP-PAR-063",
    "TP-PAR-069",
    "TP-PAR-098",
    "TP-PAR-103",
    "TP-PAR-306",
    "TP-PAR-309",
    "TP-PAR-316",
    "TP-PAR-327",
    "TP-PAR-334",
    "TP-PAR-355",
    "TP-PAR-356",
    "TP-PAR-358",
    "TP-PAR-400",
    "TP-PAR-405",
    "TP-PAR-406",
    "TP-PAR-422",
    "TP-PAR-431",
    "TP-PAR-450",
    "TP-PAR-476",
    "TP-PAR-511",
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
    parser_manifests = [case for case in manifests if case["component"] == "Host.Parser"]
    expected_ids = [
        f"TP-PAR-{number:03d}"
        for number in range(1, 537)
        if f"TP-PAR-{number:03d}" not in SPECIAL_INFRASTRUCTURE_CASES
    ]
    if [case["case_id"] for case in parser_manifests] != expected_ids:
        raise RuntimeError("manifest does not contain the complete executable Host.Parser case set")

    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    with tempfile.TemporaryDirectory(prefix="host-parser-", dir=options.work_parent) as temporary:
        root = Path(temporary)
        code = run([
            "--manifest", str(manifest_path),
            "--components", str(inventory / "components.csv"),
            "--contracts", str(inventory / "contracts.csv"),
            "--host-executable", str(options.host_executable),
            "--work-root", str(root),
            "--run-id", "host-parser",
            "--revision", "phase-2-host-parser",
            "--build-mode", options.build_mode,
            "--component", "Host.Parser",
            "--profile", "NIGHTLY",
        ])
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        payload = json.loads((root / "host-parser" / "results.json").read_text(encoding="utf-8"))
        if [result["case_id"] for result in payload["results"]] != expected_ids:
            raise RuntimeError("controller did not execute the complete executable Host.Parser case set")
        counts = payload["counts"]
        expected_passes = len(expected_ids) - len(KNOWN_PRODUCT_DEFECTS)
        unexpected = any(
            count != 0
            for status, count in counts.items()
            if status not in {"PASS", "KNOWN_FAILURE"}
        )
        if (
            counts["PASS"] != expected_passes
            or counts["KNOWN_FAILURE"] != len(KNOWN_PRODUCT_DEFECTS)
            or unexpected
        ):
            raise RuntimeError(f"unexpected Host.Parser result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
