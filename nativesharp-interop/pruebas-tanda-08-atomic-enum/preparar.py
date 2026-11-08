from pathlib import Path
import sys

source, destination = map(Path, sys.argv[1:3])
mode = sys.argv[3]
for name in ("Api.h", "Api.c"):
    text = (source / name).read_text(encoding="utf-8")
    if text.count("_Atomic(") != 1:
        raise SystemExit(f"Cantidad inesperada de declaraciones atomicas: {name}")
    if mode == "tls":
        text = text.replace("_Atomic(", "_Thread_local _Atomic(")
    (destination / name).write_text(text, encoding="utf-8")
