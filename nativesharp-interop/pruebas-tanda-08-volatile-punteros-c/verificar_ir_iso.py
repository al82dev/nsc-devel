import re
import sys
from pathlib import Path

for label, path in (("Clang", sys.argv[1]), ("NSC", sys.argv[2])):
    ir = Path(path).read_text(encoding="utf-8")
    for ty, align, minimum in (("i8", 1, 5), ("i16", 2, 2), ("i32", 4, 2), ("i64", 8, 2), ("float", 4, 2), ("double", 8, 2)):
        for op in ("load", "store"):
            pattern = rf"\b{op} volatile {ty}\b[^\n]*\balign {align}\b"
            if len(re.findall(pattern, ir)) < minimum:
                raise SystemExit(f"{label}: falta {op} volatile ISO {ty} alineación {align}")
    if re.search(r"\b(?:load|store) atomic volatile\b", ir):
        raise SystemExit(f"{label}: se encontró acceso atomic volatile en modo ISO")
print("IR volatile ISO de Clang y NSC verificado por tipo.")
