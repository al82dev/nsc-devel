#!/usr/bin/env python3
"""Remove generated files from one example directory without a batch helper."""

from __future__ import annotations

import argparse
from pathlib import Path


GENERATED_SUFFIXES = (".exe", ".obj", ".pdb", ".ilk", ".lib", ".exp", ".nsmeta", ".h")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", nargs="?", type=Path, default=Path.cwd())
    args = parser.parse_args()
    directory = args.directory.resolve()
    if not directory.is_dir():
        parser.error(f"directory does not exist: {directory}")
    for path in directory.iterdir():
        if path.is_file() and path.suffix.casefold() in GENERATED_SUFFIXES:
            path.unlink()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
