import sys
from pathlib import Path

source, destination, mode = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
if mode not in ("global", "tls"):
    raise SystemExit("Modo esperado: global o tls")
for name in ("Widths.h", "Widths.c"):
    text = (source / name).read_text(encoding="utf-8")
    if text.count("_Atomic(") != 4:
        raise SystemExit(f"Cantidad inesperada de atomicos en {source / name}")
    if mode == "tls":
        text = text.replace("_Atomic(", "_Thread_local _Atomic(")
    (destination / name).write_text(text, encoding="utf-8")
