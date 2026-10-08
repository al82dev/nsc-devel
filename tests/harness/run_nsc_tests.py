#!/usr/bin/env python3
"""Select cases from the consolidated NSC test catalog.

Tanda 2 intentionally stops before process execution. The selected NSC path is
validated and frozen, but the compiler is not started by this program yet.
"""

from __future__ import annotations

import argparse
import sys
from datetime import datetime, timezone
from pathlib import Path


HARNESS_ROOT = Path(__file__).resolve().parent
if str(HARNESS_ROOT) not in sys.path:
    sys.path.insert(0, str(HARNESS_ROOT))

from nsc_test.catalog import CatalogError, TestCatalog  # noqa: E402
from nsc_test.command import NscCommandBuilder  # noqa: E402
from nsc_test.process import ProcessExecutor  # noqa: E402
from nsc_test.report import ReportWriter  # noqa: E402
from nsc_test.verify import ResultVerifier  # noqa: E402
from nsc_test.workspace import CaseWorkspace  # noqa: E402


def existing_file(value: str) -> Path:
    path = Path(value).resolve()
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"file does not exist: {path}")
    return path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="NativeSharp compiler test harness")
    parser.add_argument("--nsc", required=True, type=existing_file)
    parser.add_argument("--nslink", type=existing_file)
    parser.add_argument(
        "--catalog",
        type=Path,
        default=HARNESS_ROOT.parent / "cases",
    )
    parser.add_argument("--optimization", choices=("O0",), default="O0")
    parser.add_argument("--work-root", type=Path, default=HARNESS_ROOT.parent.parent / "_test-harness" / "work")
    parser.add_argument("--results-root", type=Path, default=HARNESS_ROOT.parent.parent / "_test-harness" / "results")
    parser.add_argument("--run-id")
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--list", action="store_true")
    selection.add_argument("--case")
    selection.add_argument("--component")
    selection.add_argument("--all", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    arguments = parser.parse_args(argv)
    try:
        catalog = TestCatalog.load(arguments.catalog)
        if arguments.list:
            selected = catalog.cases
        elif arguments.case is not None:
            selected = (catalog.require(arguments.case),)
        elif arguments.component is not None:
            selected = catalog.by_component(arguments.component)
            if not selected:
                raise CatalogError(f"component has no cases: {arguments.component}")
        else:
            selected = catalog.cases
    except CatalogError as error:
        parser.error(str(error))

    if arguments.list:
        for case in selected:
            print(f"{case.identifier}\t{case.component}\t{case.kind.value}")
        print(f"cases={len(selected)}")
        return 0

    builder = NscCommandBuilder(arguments.nsc, arguments.nslink)
    executor = ProcessExecutor()
    verifier = ResultVerifier()
    results = []
    for case in selected:
        workspace = CaseWorkspace.create(case, arguments.work_root)
        try:
            request = builder.build(case, workspace)
            process = executor.execute(request)
            result = verifier.verify(case, workspace, process)
            workspace.keep = not result.passed
            results.append(result)
        finally:
            workspace.cleanup()
    run_id = arguments.run_id or datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    output = ReportWriter(arguments.results_root).write_verified(
        run_id,
        arguments.nsc,
        tuple(results),
    )
    passed = sum(1 for result in results if result.passed)
    failed = len(results) - passed
    print(f"nsc={arguments.nsc}")
    if arguments.nslink is not None:
        print(f"nslink={arguments.nslink}")
    print(f"optimization={arguments.optimization}")
    print(f"selected={len(results)}")
    print(f"passed={passed}")
    print(f"failed={failed}")
    print(f"results={output}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
