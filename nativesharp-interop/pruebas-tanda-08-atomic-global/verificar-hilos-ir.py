import re
import sys
from pathlib import Path

for label, path in (("Clang", sys.argv[1]), ("NSC", sys.argv[2])):
    ir = Path(path).read_text(encoding="utf-8")
    for op in ("load", "store"):
        if not re.search(rf"\b{op} atomic i32\b[^\n]*@atomic_turn\b[^\n]*\bseq_cst\b", ir):
            raise SystemExit(f"{label}: falta {op} seq_cst de la señal")
    if not re.search(r"\batomicrmw (?:volatile )?add ptr @atomic_global\b[^\n]*\bseq_cst\b", ir):
        raise SystemExit(f"{label}: falta incremento atómico seq_cst del contador")
print("Sincronización y contador atómicos comprobados en IR C y NSC.")
