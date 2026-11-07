import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
for name in ("more_float_ptr", "more_long_ptr", "more_ulong_ptr"):
    for operation in ("load atomic", "store atomic", "atomicrmw"):
        pattern = rf"\b{operation}\b[^\n]*@{name}\b[^\n]*\bseq_cst\b"
        if not re.search(pattern, text):
            raise SystemExit(f"Falta {operation} seq_cst de {name}")
print("IR de punteros atomicos globales comprobado")
