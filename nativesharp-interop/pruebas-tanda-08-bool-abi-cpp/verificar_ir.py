import re
import sys
from pathlib import Path

clang_ir = Path(sys.argv[1]).read_text(encoding="utf-8")
native_ir = Path(sys.argv[2]).read_text(encoding="utf-8")
for prefix, bool_index in (("?flip@Switch@Demo@@", 1), ("?choose@Switch@Demo@@", 1), ("?invert@Demo@@", 0)):
    symbol_match = re.search(r'@"(' + re.escape(prefix) + r'[^"\n]*)"', clang_ir)
    if not symbol_match:
        raise SystemExit(f"Clang: no se encontró símbolo {prefix}")
    symbol = re.escape(symbol_match.group(1))
    for label, ir, op in (("Clang", clang_ir, "define"), ("NSC", native_ir, "declare"), ("NSC llamada", native_ir, "call")):
        matches = re.findall(r"\b" + op + r'\b([^\n]*)@"' + symbol + r'"\(([^\n]*)\)', ir)
        if not matches:
            raise SystemExit(f"{label}: falta {op} de {prefix}")
        for result, arguments in matches:
            if not re.search(r"\bzeroext\s+i1\b", result):
                raise SystemExit(f"{label}: retorno sin zeroext en {prefix}")
            args = arguments.split(",")
            if bool_index >= len(args) or not re.search(r"\bi1\b[^,%]*\bzeroext\b", args[bool_index]):
                raise SystemExit(f"{label}: argumento {bool_index} sin zeroext en {prefix}")
print("Atributos ABI bool C++ de Clang y NSC coinciden.")
