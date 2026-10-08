#!/usr/bin/env python3
"""Select cases from the consolidated NSC test catalog.

Tanda 2 intentionally stops before process execution. The selected NSC path is
validated and frozen, but the compiler is not started by this program yet.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


HARNESS_ROOT = Path(__file__).resolve().parent
if str(HARNESS_ROOT) not in sys.path:
    sys.path.insert(0, str(HARNESS_ROOT))

from nsc_test.catalog import CatalogError, TestCatalog  # noqa: E402


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

    print(f"nsc={arguments.nsc}")
    if arguments.nslink is not None:
        print(f"nslink={arguments.nslink}")
    print(f"optimization={arguments.optimization}")
    print(f"selected={len(selected)}")
    print("execution=not-enabled-in-tanda-2")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
