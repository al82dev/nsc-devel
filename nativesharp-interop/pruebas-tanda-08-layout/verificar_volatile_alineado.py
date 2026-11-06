import re
import sys
from pathlib import Path

clang_ir = Path(sys.argv[1]).read_text(encoding="utf-8")
nsc_ir = Path(sys.argv[2]).read_text(encoding="utf-8")
clang_section = clang_ir.split("define dso_local i32 @layout_aligned_volatile_pointer(", 1)[-1].split("\n}", 1)[0]
nsc_section = nsc_ir.split("call ptr @layout_aligned_volatile_aggregates()", 1)[-1].split("ret i32 34", 1)[0]
for source, section in (("Clang", clang_section), ("NSC", nsc_section)):
    if not re.search(r"store atomic volatile i32 .* release, align 4", section):
        print(source + ": falta escritura volatile release del enum alineado")
        sys.exit(1)
    if not re.search(r"load atomic volatile i32, ptr .* acquire, align 4", section):
        print(source + ": falta lectura volatile acquire del enum alineado")
        sys.exit(1)
