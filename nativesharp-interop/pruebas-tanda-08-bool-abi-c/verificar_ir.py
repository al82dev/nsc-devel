import re
import sys
from pathlib import Path

clang_ir = Path(sys.argv[1]).read_text(encoding="utf-8")
native_ir = Path(sys.argv[2]).read_text(encoding="utf-8")
cases = {
    "c_bool_identity": (True, [0]),
    "c_bool_mixed": (True, [1]),
    "c_bool_save": (False, [0]),
}
for name, (bool_return, bool_args) in cases.items():
    for label, ir, op in (("Clang", clang_ir, "define"), ("NSC", native_ir, "declare")):
        match = re.search(r"^" + op + r"\b([^\n]*)@" + name + r"\(([^\n)]*)\)", ir, re.M)
        if not match:
            raise SystemExit(f"{label}: falta {op} de {name}")
        if bool_return and not re.search(r"\bzeroext\s+i1\b", match.group(1)):
            raise SystemExit(f"{label}: retorno sin zeroext en {name}: {match.group(0)}")
        args = match.group(2).split(",")
        for index in bool_args:
            if index >= len(args) or not re.search(r"\bi1\b[^,%]*\bzeroext\b", args[index]):
                raise SystemExit(f"{label}: argumento {index} sin zeroext en {name}: {match.group(0)}")
    calls = re.findall(r"\bcall\b([^\n]*)@" + name + r"\(([^\n)]*)\)", native_ir)
    if not calls:
        raise SystemExit(f"NSC: falta llamada a {name}")
    for result, arguments in calls:
        if bool_return and not re.search(r"\bzeroext\s+i1\b", result):
            raise SystemExit(f"NSC: llamada a {name} sin zeroext en el retorno")
        args = arguments.split(",")
        for index in bool_args:
            if index >= len(args) or not re.search(r"\bi1\b[^,%]*\bzeroext\b", args[index]):
                raise SystemExit(f"NSC: llamada a {name} sin zeroext en argumento {index}")
print("Atributos ABI bool de Clang y NSC coinciden.")
