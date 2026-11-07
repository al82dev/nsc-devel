import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
for kind, width in (("i16", 2), ("i64", 8), ("double", 8)):
    for operation, ordering in (("load", "acquire"), ("store", "release")):
        pattern = rf"\b{operation} atomic volatile {kind}\b[^\n]*\b{ordering}, align {width}"
        if len(re.findall(pattern, text)) < 2:
            raise SystemExit(f"Faltan accesos {operation} de {kind} con {ordering}")
print("IR volatile short, long y double directo e indexado verificado")
