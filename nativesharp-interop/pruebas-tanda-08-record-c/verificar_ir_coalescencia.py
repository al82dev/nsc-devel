import re
import sys
from pathlib import Path

ir = Path(sys.argv[1]).read_text(encoding="utf-8")
loads = re.findall(
    r"%coalesce\.current(?:\d+)? = load atomic volatile ptr, ptr @volatile_record_pointer acquire, align 8",
    ir,
)
fallbacks = re.findall(
    r"^coalesce\.store\.fallback(?:\d+)?:[^\n]*\n(.*?)(?=^[\w.]+:|^\})",
    ir,
    re.M | re.S,
)
stores = [block for block in fallbacks if re.search(
    r"store atomic volatile ptr [^\n]*@volatile_record_pointer release, align 8", block
)]
if len(loads) < 2 or len(stores) < 2:
    print("Falta orden acquire/release de coalescencia para el puntero global volatile")
    sys.exit(1)
