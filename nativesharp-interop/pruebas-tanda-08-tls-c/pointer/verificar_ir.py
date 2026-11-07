import sys
from pathlib import Path

lines = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
if not any("volatile" in line and "@tls_volatile_slot" in line for line in lines):
    raise SystemExit("Falta acceso volatile al puntero TLS en LLVM IR")
if not any("store" in line and "volatile" in line and "i32" in line for line in lines):
    raise SystemExit("Falta escritura volatile al destino del puntero TLS")
print("LLVM IR conserva los accesos volatile del puntero TLS y su destino.")
