import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
for operation, ordering in (("load", "acquire"), ("store", "release")):
    pattern = rf"\b{operation} atomic volatile i32\b[^\n]*\b{ordering}, align 4"
    if len(re.findall(pattern, text)) < 2:
        raise SystemExit(f"Faltan accesos {operation} volatile directo e indexado con {ordering}")
print("IR volatile unsigned directo e indexado verificado")
