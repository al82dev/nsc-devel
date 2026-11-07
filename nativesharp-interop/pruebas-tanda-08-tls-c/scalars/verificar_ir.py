import sys
from pathlib import Path

ir = Path(sys.argv[1]).read_text(encoding="utf-8")
lines = ir.splitlines()
if not any("@tls_volatile" in line and "thread_local" in line for line in lines):
    raise SystemExit("Falta declaracion TLS volatile en LLVM IR")
if not any("load" in line and "volatile" in line and "@tls_volatile" in line for line in lines):
    raise SystemExit("Falta lectura volatile del TLS en LLVM IR")
if not any("store" in line and "volatile" in line and "@tls_volatile" in line for line in lines):
    raise SystemExit("Falta escritura volatile del TLS en LLVM IR")
print("LLVM IR conserva lectura y escritura volatile del TLS.")
