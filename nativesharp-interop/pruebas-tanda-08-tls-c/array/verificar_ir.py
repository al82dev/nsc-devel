import sys
from pathlib import Path

lines = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
for operation in ("load", "store"):
    if not any(operation in line and "volatile" in line and "@tls_volatile_array" in line for line in lines):
        raise SystemExit(f"Falta {operation} volatile del arreglo TLS en LLVM IR")
print("LLVM IR conserva lecturas y escrituras volatile del arreglo TLS.")
