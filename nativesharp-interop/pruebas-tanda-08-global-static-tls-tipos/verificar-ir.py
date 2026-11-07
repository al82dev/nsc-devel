import pathlib
import re
import sys

ir = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
if not re.search(r"external thread_local global i32", ir):
    raise SystemExit("Falta declaracion TLS i32")
for operation in ("load", "store"):
    if not re.search(rf"\b{operation} atomic i32\b[^\n]*\bseq_cst\b[^\n]*\balign 4\b", ir):
        raise SystemExit(f"Falta {operation} atomico secuencial de 32 bits")
print("TLS y operaciones atomicas verificadas en IR")
