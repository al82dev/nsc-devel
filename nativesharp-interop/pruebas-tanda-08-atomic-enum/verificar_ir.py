import pathlib
import re
import sys

for path in sys.argv[1:]:
    text = pathlib.Path(path).read_text(encoding="utf-8")
    for operation in ("load", "store"):
        pattern = rf"\b{operation}\s+atomic\s+i32\b[^\n]*@state\b[^\n]*\bseq_cst\b[^\n]*\balign 4\b"
        if not re.search(pattern, text):
            raise SystemExit(f"{path}: falta {operation} atomico i32 seq_cst alineado a 4 para state")
    print(f"IR atomico comprobado: {path}")
