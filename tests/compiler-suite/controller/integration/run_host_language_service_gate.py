from __future__ import annotations

import argparse
import json
import sys
import tempfile
from collections import Counter
from pathlib import Path

CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))
from run_suite import run


FULL_EXECUTABLE_CONTRACTS = (
    *range(1, 6),
    *range(7, 21),
    *range(30, 38),
    *range(39, 43),
)
EXPECTED_PROFILES = {"PR": 224, "NIGHTLY": 31}
EXPECTED_LAYERS = {"unit": 231, "component": 24}


def executable_ids() -> list[str]:
    identifiers: list[str] = []
    for contract in FULL_EXECUTABLE_CONTRACTS:
        first = (contract - 1) * 8 + 1
        identifiers.extend(
            f"TF-LANGUAGE-SERVICE-{ordinal:03d}"
            for ordinal in range(first, first + 8)
        )
    identifiers.extend(
        f"TF-LANGUAGE-SERVICE-{ordinal:03d}" for ordinal in range(297, 304)
    )
    return sorted(identifiers)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)

    manifest_path = (
        options.source_root
        / "manifest"
        / "cases.jsonl"
    )
    all_manifests = [
        json.loads(line)
        for line in manifest_path.read_text(encoding="utf-8").splitlines()
    ]
    manifests = [
        case
        for case in all_manifests
        if case["component"] == "Host.LanguageService"
    ]
    expected_ids = executable_ids()
    if [case["case_id"] for case in manifests] != expected_ids:
        raise RuntimeError(
            "central manifest does not contain the canonical 255 executable "
            "Host.LanguageService cases"
        )
    if Counter(case["profile"] for case in manifests) != EXPECTED_PROFILES:
        raise RuntimeError("Host.LanguageService profile distribution is not canonical")
    if Counter(case["layer"] for case in manifests) != EXPECTED_LAYERS:
        raise RuntimeError("Host.LanguageService layer distribution is not canonical")

    inventory = (
        options.repository_root
        / "docs"
        / "compiler-testing-reengineering"
        / "inventory"
    )
    with tempfile.TemporaryDirectory(
        prefix="host-language-service-", dir=options.work_parent
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
                "host-language-service",
                "--revision",
                "phase-2-host-language-service",
                "--build-mode",
                options.build_mode,
                "--component",
                "Host.LanguageService",
                "--profile",
                "NIGHTLY",
            ]
        )
        if code != 0:
            raise RuntimeError(f"Host.LanguageService controller returned {code}")
        payload = json.loads(
            (root / "host-language-service" / "results.json").read_text(
                encoding="utf-8"
            )
        )
        if [result["case_id"] for result in payload["results"]] != expected_ids:
            raise RuntimeError(
                "controller did not execute the canonical executable "
                "Host.LanguageService case set"
            )
        counts = payload["counts"]
        unexpected = any(
            count != 0 for status, count in counts.items() if status != "PASS"
        )
        if counts["PASS"] != len(expected_ids) or unexpected:
            raise RuntimeError(
                f"unexpected Host.LanguageService result counts: {counts}"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
