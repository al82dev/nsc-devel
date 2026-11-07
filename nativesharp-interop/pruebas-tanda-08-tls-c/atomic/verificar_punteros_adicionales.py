import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
for symbol, step in (("atomic_short_pointer", 2), ("atomic_long_pointer", 8), ("atomic_double_pointer", 8)):
    address = "@" + symbol
    checks = (
        rf"store atomic i64 .*ptr {address}\b seq_cst, align 8",
        rf"load atomic i64, ptr {address}\b seq_cst, align 8",
        rf"atomicrmw add ptr {address}\b, i64 {step} seq_cst, align 8",
        rf"atomicrmw add ptr {address}\b, i64 -{step * 2} seq_cst, align 8",
    )
    for pattern in checks:
        if not re.search(pattern, text):
            raise SystemExit(f"Falta operacion atomica de {symbol}: {pattern}")
print("IR de punteros TLS atomicos short, long y double verificado")
