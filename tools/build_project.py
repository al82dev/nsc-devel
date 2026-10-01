#!/usr/bin/env python3
"""Build one NativeSharp project without cmd.exe or batch helpers."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys


DEFAULT_ROOT = Path(r"C:\native-sharp")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("project", type=Path)
    parser.add_argument("output", nargs="?", type=Path)
    args = parser.parse_args()

    project = args.project.resolve()
    output = (args.output or project.with_suffix(".exe")).resolve()
    compiler = Path(
        os.environ.get(
            "NSC_EXE",
            Path(os.environ.get("NS_ROOT", str(DEFAULT_ROOT))) / "bin" / "nsc.exe",
        )
    ).resolve()
    optimization = os.environ.get("NSC_OPT", "-O0")
    obj = output.with_suffix(".obj")

    if not project.is_file():
        print(f"error: project not found: {project}", file=sys.stderr)
        return 2
    if not compiler.is_file():
        print(f"error: compiler not found: {compiler}", file=sys.stderr)
        return 2
    output.parent.mkdir(parents=True, exist_ok=True)

    commands = (
        [str(compiler), "--emit-project-oop-obj", str(project), optimization, "-o", str(obj)],
        [str(compiler), "--emit-project-oop-exe", str(project), optimization, "-o", str(output)],
    )
    for command in commands:
        completed = subprocess.run(command, cwd=project.parent, check=False)
        if completed.returncode != 0:
            return completed.returncode
    print(f"built: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
