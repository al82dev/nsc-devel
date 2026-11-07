import re
import sys
from pathlib import Path

source = Path(sys.argv[1]).read_text(encoding="utf-8")
names = ("extra_u8", "extra_i16", "extra_u32_ptr", "extra_i16_ptr",
         "extra_i64_ptr", "extra_f64_ptr", "extra_const_ptr", "extra_volatile_ptr")
for name in names:
    for operation in ("load", "store"):
        pattern = rf"\b{operation} atomic\b[^\n]*@{name}\b[^\n]*\bseq_cst\b"
        if not re.search(pattern, source):
            print(f"Falta {operation} atomico seq_cst de {name} en {sys.argv[1]}")
            sys.exit(1)
print(f"Accesos atomicos seq_cst comprobados: {sys.argv[1]}")
