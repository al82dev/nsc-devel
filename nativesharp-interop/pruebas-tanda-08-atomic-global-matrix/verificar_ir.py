import re
import sys
from pathlib import Path

source = Path(sys.argv[1]).read_text(encoding="utf-8")
names = ("value_u32", "value_i64", "value_u64", "value_i8", "value_u16",
         "value_f32", "value_f64", "value_bool", "value_pointer")
for name in names:
    for operation in ("load", "store"):
        pattern = rf"\b{operation} atomic\b[^\n]*@{name}\b[^\n]*\bseq_cst\b"
        if not re.search(pattern, source):
            print(f"Falta {operation} atomico seq_cst de {name} en {sys.argv[1]}")
            sys.exit(1)
print(f"Accesos atomicos seq_cst comprobados: {sys.argv[1]}")
