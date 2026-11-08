import re
import sys
from pathlib import Path


clang_ir = Path(sys.argv[1]).read_text(encoding="utf-8")
nsc_ir = Path(sys.argv[2]).read_text(encoding="utf-8")
for width in (8, 32):
    if not re.search(rf"\bload volatile i{width}\b", clang_ir):
        raise SystemExit(f"Clang: falta lectura volatile i{width}")
    for operation in ("load", "store"):
        if not re.search(rf"\b{operation} volatile i{width}\b", nsc_ir):
            raise SystemExit(f"NSC: falta {operation} volatile i{width}")
print("Record C volatile: lecturas y escrituras anidadas verificadas.")
