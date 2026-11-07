import sys
from pathlib import Path

ir = Path(sys.argv[1]).read_text(encoding="utf-8")
lines = ir.splitlines()
for name in ("tls_volatile", "tls_volatile_unsigned", "tls_volatile_long", "tls_volatile_double", "tls_volatile_bool", "tls_const_volatile"):
    symbol = "@" + name
    if not any(symbol + " =" in line and "thread_local" in line for line in lines):
        raise SystemExit(f"Falta declaracion TLS {name} en LLVM IR")
    operations = ("load",) if name == "tls_const_volatile" else ("load", "store")
    for operation in operations:
        if not any(operation in line and "volatile" in line and symbol + "," in line for line in lines):
            raise SystemExit(f"Falta {operation} volatile de {name} en LLVM IR")
print("LLVM IR conserva lectura y escritura volatile del TLS.")
