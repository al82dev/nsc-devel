import sys
from pathlib import Path

lines = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
for name in ("tls_volatile_array", "tls_vu_array", "tls_vl_array", "tls_vd_array", "tls_vb_array"):
    for operation in ("load", "store"):
        if not any(operation in line and "volatile" in line and "@" + name in line for line in lines):
            raise SystemExit(f"Falta {operation} volatile de {name} en LLVM IR")
print("LLVM IR conserva lecturas y escrituras volatile del arreglo TLS.")
