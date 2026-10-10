from __future__ import annotations

import argparse
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
    parser.add_argument("--repository-root", type=Path, required=True)
    parser.add_argument("--work-parent", type=Path, required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    options = parser.parse_args()
    options.work_parent.mkdir(parents=True, exist_ok=True)
    inventory = options.repository_root / "docs" / "compiler-testing-reengineering" / "inventory"
    with tempfile.TemporaryDirectory(prefix="host-diagnostics-engine-budget-", dir=options.work_parent) as temporary:
        root = Path(temporary)
        code = run([
            "--manifest", str(options.source_root / "manifest" / "cases.jsonl"),
            "--components", str(inventory / "components.csv"),
            "--contracts", str(inventory / "contracts.csv"),
            "--host-executable", str(options.host_executable),
            "--work-root", str(root),
            "--run-id", "host-diagnostics-engine-budget",
            "--revision", "phase-2-host-diagnostics-engine-budget",
            "--build-mode", options.build_mode,
            "--contract", "CD-DIA-005",
            "--profile", "PR",
        ])
        if code != 0:
            raise RuntimeError(f"controller returned {code}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
