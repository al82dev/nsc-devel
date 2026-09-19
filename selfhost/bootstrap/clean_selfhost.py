#!/usr/bin/env python3
"""Limpia únicamente áreas generadas de self-hosting; modo seco por omisión."""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

MARKER = ".nativesharp-selfhost-state"
GENERATED_CHILDREN = ("build", "work", "temp", "runs")


def is_within(path: Path, root: Path) -> bool:
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


def initialize(root: Path) -> Path:
    resolved = root.resolve()
    resolved.mkdir(parents=True, exist_ok=True)
    marker = resolved / MARKER
    marker.write_text("NativeSharp self-host generated state v1\n", encoding="utf-8")
    return marker


def cleanup_targets(root: Path) -> list[Path]:
    resolved = root.resolve()
    marker = resolved / MARKER
    if not marker.is_file():
        raise ValueError(f"falta el marcador de seguridad: {marker}")
    targets: list[Path] = []
    for name in GENERATED_CHILDREN:
        target = (resolved / name).resolve()
        if target.parent != resolved or not is_within(target, resolved):
            raise ValueError(f"destino fuera del área administrada: {target}")
        if target.exists():
            targets.append(target)
    return targets


def main() -> int:
    default_root = Path(__file__).resolve().parents[1] / "out"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state-root", type=Path, default=default_root)
    parser.add_argument("--initialize", action="store_true")
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    try:
        if args.initialize:
            marker = initialize(args.state_root)
            print(f"marcador creado: {marker}")
        targets = cleanup_targets(args.state_root)
        if not targets:
            print("no hay áreas generadas que limpiar")
            return 0
        for target in targets:
            print(("eliminando" if args.apply else "eliminaría") + f": {target}")
            if args.apply:
                shutil.rmtree(target)
    except (OSError, ValueError) as exc:
        print(f"selfhost cleanup: error: {exc}", file=sys.stderr)
        return 2
    if not args.apply:
        print("modo seco; use --apply para ejecutar la limpieza")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
