import re
import sys
from pathlib import Path

clang_ir = Path(sys.argv[1]).read_text(encoding="utf-8")
native_ir = Path(sys.argv[2]).read_text(encoding="utf-8")
for label, ir in (("Clang", clang_ir), ("NSC", native_ir)):
    for ty, align, minimum in (("i8", 1, 2), ("i16", 2, 1), ("i32", 4, 1), ("i64", 8, 1), ("float", 4, 1), ("double", 8, 1)):
        for op, ordering in (("load", "acquire"), ("store", "release")):
            pattern = rf"\b{op} atomic volatile {ty}\b[^\n]*\b{ordering}, align {align}\b"
            if len(re.findall(pattern, ir)) < minimum:
                raise SystemExit(f"{label}: falta {op} {ty} {ordering} alineación {align}")
print("IR volatile de Clang y NSC verificado por tipo y orden.")
