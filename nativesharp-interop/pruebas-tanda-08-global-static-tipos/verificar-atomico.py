import pathlib
import re
import sys

ir = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
for operation in ("load", "store"):
    pattern = rf"\b{operation} atomic i32\b[^\n]*\bseq_cst\b[^\n]*\balign 4\b"
    if not re.search(pattern, ir):
        raise SystemExit(f"Falta {operation} atomico secuencial de 32 bits alineado")
print("Lectura y escritura atomicas verificadas en IR")
