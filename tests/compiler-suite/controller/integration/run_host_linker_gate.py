from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

CONTROLLER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CONTROLLER_ROOT))
from run_suite import run


EXPECTED_IDS = [
    "TF-LINKER-265",
    "TF-LINKER-267",
    "TF-LINKER-268",
    "TF-LINKER-269",
    "TF-LINKER-271",
    "TF-LINKER-272",
    "TF-LINKER-289",
    "TF-LINKER-290",
    "TF-LINKER-291",
    "TF-LINKER-292",
    "TF-LINKER-293",
    "TF-LINKER-295",
    "TF-LINKER-296",
]


def _load_linker_cases(source_root: Path) -> list[dict[str, object]]:
    """Loads Host.Linker executable rows from the central manifest in linear time."""
    manifest = source_root / "manifest" / "cases.jsonl"
    cases: list[dict[str, object]] = []
    for line in manifest.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        case = json.loads(line)
        if case["component"] == "Host.Linker":
            cases.append(case)
    return cases


def main() -> int:
    """Runs the executable subset after central integration is authorized."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)

    manifests = _load_linker_cases(options.source_root)
    actual_ids = [str(case["case_id"]) for case in manifests]
    if actual_ids != EXPECTED_IDS:
        raise RuntimeError(
            "Host.Linker central manifest does not contain exactly the 13 executable "
            "cases in canonical order"
        )

    inventory = (
        options.repository_root
        / "docs"
        / "compiler-testing-reengineering"
        / "inventory"
    )
    manifest_path = options.source_root / "manifest" / "cases.jsonl"
    with tempfile.TemporaryDirectory(
        prefix="host-linker-", dir=options.work_parent
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
                "host-linker",
                "--revision",
                "phase-2-host-linker",
                "--build-mode",
                options.build_mode,
                "--component",
                "Host.Linker",
                "--profile",
                "NIGHTLY",
            ]
        )
        if code != 0:
            raise RuntimeError(f"Host.Linker controller returned {code}")
        payload = json.loads(
            (root / "host-linker" / "results.json").read_text(encoding="utf-8")
        )
        result_ids = [result["case_id"] for result in payload["results"]]
        if result_ids != EXPECTED_IDS:
            raise RuntimeError(
                "controller did not execute exactly the 13 executable "
                "Host.Linker cases in canonical order"
            )
        counts = payload["counts"]
        unexpected = any(
            count != 0 for status, count in counts.items() if status != "PASS"
        )
        if counts["PASS"] != len(EXPECTED_IDS) or unexpected:
            raise RuntimeError(f"unexpected Host.Linker result counts: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
