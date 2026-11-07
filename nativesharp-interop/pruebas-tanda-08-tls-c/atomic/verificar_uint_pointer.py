import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
symbol = r"@atomic_unsigned_pointer\b"
checks = (
    rf"store atomic i64 .*ptr {symbol} seq_cst, align 8",
    rf"load atomic i64, ptr {symbol} seq_cst, align 8",
    rf"atomicrmw add ptr {symbol}, i64 4 seq_cst, align 8",
    rf"atomicrmw add ptr {symbol}, i64 -8 seq_cst, align 8",
)
for check in checks:
    if not re.search(check, text):
        raise SystemExit("Falta operacion atomica del puntero unsigned: " + check)
print("IR del puntero TLS atomico unsigned verificado")
