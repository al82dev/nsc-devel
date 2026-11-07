import sys
from pathlib import Path

source = Path(sys.argv[1])
destination = Path(sys.argv[2])
for name in ("MorePointer.h", "MorePointer.c"):
    text = (source / name).read_text(encoding="utf-8")
    if text.count("_Atomic(") != 3:
        raise SystemExit(f"Cantidad inesperada de declaraciones atomicas en {source / name}")
    (destination / name).write_text(text.replace("_Atomic(", "_Thread_local _Atomic("), encoding="utf-8")
