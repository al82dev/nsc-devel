import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
mode = sys.argv[2]
for name in ("width_i8", "width_u8", "width_u16", "width_u64"):
    declaration = rf"@{name} = external {'thread_local ' if mode == 'tls' else ''}global ptr"
    if not re.search(declaration, text):
        raise SystemExit(f"Falta declaracion {mode} de {name}")
    for operation in ("load atomic", "store atomic", "atomicrmw"):
        if not re.search(rf"\b{operation}\b[^\n]*@{name}\b[^\n]*\bseq_cst\b", text):
            raise SystemExit(f"Falta {operation} seq_cst de {name}")
print(f"IR de punteros atomicos {mode} comprobado")
