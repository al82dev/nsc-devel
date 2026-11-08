import pathlib
import re
import sys

mode = sys.argv[1]
for path in sys.argv[2:]:
    text = pathlib.Path(path).read_text(encoding="utf-8")
    if mode == "tls" and not re.search(r"@state\s*=\s*[^\n]*\bthread_local\b", text):
        raise SystemExit(f"{path}: state no aparece como variable TLS")
    if not re.search(r"\b(?:cmpxchg|atomicrmw)\b[^\n]*\bseq_cst\b", text):
        raise SystemExit(f"{path}: falta lectura-modificacion-escritura atomica seq_cst")
    for operation in ("load", "store"):
        symbol = r"@state\b" if mode == "global" else r""
        pattern = rf"\b{operation}\s+atomic\s+i32\b[^\n]*{symbol}[^\n]*\bseq_cst\b[^\n]*\balign 4\b"
        if not re.search(pattern, text):
            raise SystemExit(f"{path}: falta {operation} atomico i32 seq_cst alineado a 4 para state")
    print(f"IR atomico comprobado: {path}")
