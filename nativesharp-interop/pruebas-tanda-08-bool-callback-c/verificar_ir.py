import re
import sys
from pathlib import Path

clang_ir = Path(sys.argv[1]).read_text(encoding="utf-8")
native_ir = Path(sys.argv[2]).read_text(encoding="utf-8")
for label, ir, pattern in (
    ("Clang callback", clang_ir, r"^define[^\n]*@c_bool_callback\([^\n]*\)"),
    ("Clang llamada indirecta", clang_ir, r"\bcall[^\n]*i1\s+%[^\n]*\)"),
    ("NSC callback", native_ir, r'^define[^\n]*__c_boundary[^\n]*\)'),
    ("NSC llamada indirecta", native_ir, r"\bcall[^\n]*i1\s+%[^\n]*\)"),
):
    matches = re.findall(pattern, ir, re.M)
    if not matches:
        raise SystemExit(f"{label}: no se encontró firma o llamada esperada")
    for text in matches:
        if not re.search(r"\bzeroext\s+i1\b", text) or not re.search(r"\bi1\s+(?:noundef\s+)?zeroext\b", text):
            raise SystemExit(f"{label}: faltan atributos ABI: {text}")
print("Callbacks bool C: atributos ABI correctos en Clang y NSC.")
