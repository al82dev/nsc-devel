import pathlib
import re
import sys

ir = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
symbols = {}
for operation in ("load", "store"):
    pattern = rf"\b{operation} atomic i32\b[^\n]*@(__nsc_c_inline_[0-9a-f]+)\b[^\n]*\bseq_cst\b[^\n]*\balign 4\b"
    symbols[operation] = set(re.findall(pattern, ir))
if len(symbols["load"] & symbols["store"]) < 2:
    raise SystemExit("Faltan lecturas y escrituras atomicas de ambas globales internas")
print("Lectura y escritura atomicas de ambas globales verificadas en IR")
