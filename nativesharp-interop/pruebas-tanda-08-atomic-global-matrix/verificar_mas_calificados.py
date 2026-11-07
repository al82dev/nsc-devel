import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
if len(sys.argv) < 3 or sys.argv[2] != "clang":
    for name in ("more_float_const", "more_float_volatile", "more_long_const",
                 "more_long_volatile", "more_ulong_const", "more_ulong_volatile"):
        for operation in ("load atomic", "store atomic"):
            pattern = rf"\b{operation}\b[^\n]*@{name}\b[^\n]*\bseq_cst\b"
            if not re.search(pattern, text):
                raise SystemExit(f"Falta {operation} seq_cst de {name}")
for operation, ordering in (("load", "acquire"), ("store", "release")):
    for kind, count in (("float", 1), ("i32", 2)):
        if len(sys.argv) < 3 or sys.argv[2] != "clang":
            count *= 2
        pattern = rf"\b{operation} atomic volatile {kind}\b[^\n]*\b{ordering}, align 4"
        if len(re.findall(pattern, text)) < count:
            raise SystemExit(f"Faltan accesos {operation} volatile {kind} {ordering}")
print("IR de punteros atomicos y apuntados volatile comprobado")
