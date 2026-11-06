import re
import sys
from pathlib import Path

ir = Path(sys.argv[1]).read_text(encoding="utf-8")
patterns = (
    r"%field\.load(?:\d+)? = load i32, ptr %field\.addr(?:\d+)?, align 4",
    r"%compound\.load(?:\d+)? = load i32, ptr %field\.addr(?:\d+)?, align 4",
    r"store i32 [^\n]*ptr %field\.addr(?:\d+)?, align 4",
    r"%field\.load(?:\d+)? = load i32, ptr %field\.addr(?:\d+)?, align 1",
    r"%compound\.load(?:\d+)? = load i32, ptr %field\.addr(?:\d+)?, align 1",
    r"store i32 [^\n]*ptr %field\.addr(?:\d+)?, align 1",
    r"%field\.load(?:\d+)? = load ptr, ptr %field\.addr(?:\d+)?, align 1",
    r"store ptr [^\n]*ptr %field\.addr(?:\d+)?, align 1",
    r"%field\.load(?:\d+)? = load i32, ptr %field\.addr(?:\d+)?, align 2",
    r"%compound\.load(?:\d+)? = load i32, ptr %field\.addr(?:\d+)?, align 2",
    r"store i32 [^\n]*ptr %field\.addr(?:\d+)?, align 2",
    r"%field\.load(?:\d+)? = load double, ptr %field\.addr(?:\d+)?, align 2",
    r"store double [^\n]*ptr %field\.addr(?:\d+)?, align 2",
    r"%pointer\.load(?:\d+)? = load i32, ptr %raw\.element\.addr(?:\d+)?, align 1",
    r"%compound\.load(?:\d+)? = load i32, ptr %raw\.element\.addr(?:\d+)?, align 1",
    r"store i32 [^\n]*ptr %raw\.element\.addr(?:\d+)?, align 1",
    r"%pointer\.load(?:\d+)? = load volatile i32, ptr %raw\.element\.addr(?:\d+)?, align 1",
    r"store volatile i32 [^\n]*ptr %raw\.element\.addr(?:\d+)?, align 1",
    r"%pointer\.load(?:\d+)? = load ptr, ptr %raw\.element\.addr(?:\d+)?, align 1",
    r"store ptr [^\n]*ptr %raw\.element\.addr(?:\d+)?, align 1",
)
if any(re.search(pattern, ir) is None for pattern in patterns):
    print("Falta alineacion de campo C en lectura o escritura simple/compuesta")
    sys.exit(1)

nested_addresses = re.findall(
    r"(%field\.addr\d*) = getelementptr i8, ptr %field\.addr\d*, i64 4", ir
)
if not any(re.search(r"load i32, ptr " + re.escape(address) + r", align 1", ir)
           for address in nested_addresses):
    print("Falta lectura alineada a 1 del campo anidado")
    sys.exit(1)
if not any(re.search(r"store i32 [^\n]*ptr " + re.escape(address) + r", align 1", ir)
           for address in nested_addresses):
    print("Falta escritura alineada a 1 del campo anidado")
    sys.exit(1)
