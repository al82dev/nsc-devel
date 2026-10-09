from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path


CONTROLLER_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(CONTROLLER_ROOT))

from nsc_suite import (
    CancellationToken,
    EventWriter,
    HostExecutor,
    InventoryIndex,
    ReportWriter,
    RunMetadata,
    Selection,
    SuiteRunner,
    ToolIdentity,
    WorkspaceManager,
    load_manifest,
    select_cases,
)


def parse_arguments(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="NativeSharp compiler suite controller")
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--components", type=Path, required=True)
    parser.add_argument("--contracts", type=Path, required=True)
    parser.add_argument("--host-executable", type=Path, required=True)
    parser.add_argument("--work-root", type=Path, required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--build-mode", choices=("Debug", "Release"), required=True)
    parser.add_argument("--optimization", choices=("NA", "O0", "O1", "O2", "O3"))
    parser.add_argument("--profile", choices=("PR", "NIGHTLY", "WEEKLY"))
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--contract", action="append", default=[])
    parser.add_argument("--component", action="append", default=[])
    parser.add_argument("--layer", action="append", default=[])
    parser.add_argument("--tag", action="append", default=[])
    parser.add_argument("--all", action="store_true")
    return parser.parse_args(arguments)


def run(arguments: list[str]) -> int:
    options = parse_arguments(arguments)
    inventory = InventoryIndex.from_csv(options.components, options.contracts)
    cases = load_manifest(options.manifest, inventory)
    selection = Selection(
        all_cases=options.all,
        case_ids=frozenset(options.case),
        contract_ids=frozenset(options.contract),
        components=frozenset(options.component),
        layers=frozenset(options.layer),
        profile=options.profile,
        build_mode=options.build_mode,
        optimization=options.optimization,
        tags=frozenset(options.tag),
    )
    selected = select_cases(cases, selection)
    unsupported = tuple(case.case_id for case in selected if case.executor != "nsc_component_tests")
    if unsupported:
        raise ValueError(
            "selected cases require executors not implemented by this controller increment: "
            + ", ".join(unsupported)
        )

    host = options.host_executable.resolve()
    if not host.is_file():
        raise ValueError(f"host executable does not exist: {host}")
    work_root = options.work_root.resolve()
    event_path = work_root / options.run_id / "events.jsonl"
    cancellation = CancellationToken()
    with EventWriter(event_path) as events:
        runner = SuiteRunner(WorkspaceManager(work_root), events, cancellation)
        results = runner.run(options.run_id, selected, HostExecutor((str(host),)))
    metadata = RunMetadata(
        run_id=options.run_id,
        revision=options.revision,
        build_mode=options.build_mode,
        optimization=options.optimization or "NA",
        profile=options.profile or "explicit",
        scope="all" if options.all else "focused",
        selected_case_ids=tuple(case.case_id for case in selected),
        tools=(ToolIdentity("nsc_component_tests", str(host), _sha256(host)),),
    )
    ReportWriter(work_root / options.run_id).write(metadata, results)
    return 0 if all(result.status.value == "PASS" for result in results) else 1


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    try:
        return run(sys.argv[1:])
    except Exception as error:
        print(f"nsc_suite_controller: error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
