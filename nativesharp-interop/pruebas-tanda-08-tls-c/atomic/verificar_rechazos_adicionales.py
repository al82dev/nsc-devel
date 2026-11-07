import sys
from pathlib import Path

for path, message in (
    (sys.argv[1], "error: left side of assignment is not assignable"),
    (sys.argv[2], "error: initialization would discard pointer pointee volatile qualification"),
):
    text = Path(path).read_text(encoding="utf-8")
    if text.count(message) != 3:
        raise SystemExit(f"Se requieren tres rechazos en {path}: {message}")
print("Rechazos const y volatile verificados en los tres tipos")
