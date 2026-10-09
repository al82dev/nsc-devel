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
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)
    fixture = options.source_root / "fixtures" / "infrastructure"
    with tempfile.TemporaryDirectory(
        prefix="controller-gate-", dir=options.work_parent
    ) as temporary:
        root = Path(temporary)
        code = run(
            [
                "--manifest", str(fixture / "cases.jsonl"),
                "--components", str(fixture / "components.csv"),
                "--contracts", str(fixture / "contracts.csv"),
                "--host-executable", str(options.host_executable),
                "--work-root", str(root),
                "--run-id", "infrastructure",
                "--revision", "synthetic-infrastructure-gate",
                "--build-mode", "Debug",
                "--all",
            ]
        )
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
        run_root = root / "infrastructure"
        payload = json.loads((run_root / "results.json").read_text(encoding="utf-8"))
        if payload["counts"]["PASS"] != 3 or len(payload["results"]) != 3:
            raise RuntimeError("controller did not preserve three passing results")
        events = (run_root / "events.jsonl").read_text(encoding="utf-8").splitlines()
        if len(events) != 8:
            raise RuntimeError(f"controller emitted {len(events)} events instead of 8")
        for required in ("summary.md", "failures.log"):
            if not run_root.joinpath(required).is_file():
                raise RuntimeError(f"controller omitted {required}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
