import sys
from pathlib import Path

source = Path(sys.argv[1])
destination = Path(sys.argv[2])
for name in ("Api.h", "Api.c"):
    text = (source / name).read_text(encoding="utf-8")
    if "_Thread_local " not in text:
        raise SystemExit(f"Falta _Thread_local en {source / name}")
    (destination / name).write_text(text.replace("_Thread_local ", ""), encoding="utf-8")
