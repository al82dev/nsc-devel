from __future__ import annotations

import hashlib
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
MANIFEST = ROOT / "MANIFEST_SHA256.txt"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    if not MANIFEST.is_file():
        print("ERROR: MANIFEST_SHA256.txt not found", file=sys.stderr)
        return 2

    checked = 0
    failed = 0
    for line_number, raw in enumerate(MANIFEST.read_text(encoding="utf-8").splitlines(), start=1):
        if not raw.strip():
            continue
        try:
            expected, relative = raw.split("  ", 1)
        except ValueError:
            print(f"FAIL line {line_number}: malformed manifest entry", file=sys.stderr)
            failed += 1
            continue
        relative = relative.removeprefix("./")
        path = ROOT / relative
        if not path.is_file():
            print(f"FAIL missing: {relative}", file=sys.stderr)
            failed += 1
            continue
        actual = sha256(path)
        checked += 1
        if actual != expected:
            print(f"FAIL hash: {relative}", file=sys.stderr)
            print(f"  expected {expected}", file=sys.stderr)
            print(f"  actual   {actual}", file=sys.stderr)
            failed += 1

    if failed:
        print(f"MANIFEST: {checked} checked, {failed} failure(s)")
        return 1

    print(f"MANIFEST: {checked}/{checked} PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
