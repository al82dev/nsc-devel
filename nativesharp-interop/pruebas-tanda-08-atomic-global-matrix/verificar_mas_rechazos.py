import sys
from pathlib import Path

for path, expected in zip(sys.argv[1:], ("left side of assignment is not assignable",
                                         "would discard pointee constness",
                                         "would discard pointer pointee volatile qualification")):
    text = Path(path).read_text(encoding="utf-8")
    if text.count(expected) < 3:
        raise SystemExit(f"Faltan rechazos {expected} en {path}")
print("Rechazos de const y volatile comprobados")
