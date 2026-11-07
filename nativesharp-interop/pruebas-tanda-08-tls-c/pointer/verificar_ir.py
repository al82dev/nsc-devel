import sys
from pathlib import Path

lines = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
if not any("volatile" in line and "@tls_volatile_slot" in line for line in lines):
    raise SystemExit("Falta acceso volatile al puntero TLS en LLVM IR")
if not any("store" in line and "volatile" in line and "i32" in line for line in lines):
    raise SystemExit("Falta escritura volatile al destino del puntero TLS")
for name in ("tls_volatile_record", "tls_volatile_records", "tls_volatile_mode", "tls_const_volatile_record"):
    operations = ("load",) if name == "tls_const_volatile_record" else ("load", "store")
    for operation in operations:
        if not any(operation in line and "volatile" in line and "@" + name in line for line in lines):
            raise SystemExit(f"Falta {operation} volatile de {name} en LLVM IR")
print("LLVM IR conserva los accesos volatile del puntero TLS y su destino.")
