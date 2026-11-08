import pathlib
import re
import sys

clang, native = (pathlib.Path(path).read_text(encoding="utf-8") for path in sys.argv[1:])
for name, ir in (("Clang", clang), ("NSC", native)):
    for pattern in (r"@c_flag = [^\n]*\bglobal i8\b",
                    r"@c_values = [^\n]*\bglobal \[2 x i8\]"):
        if not re.search(pattern, ir):
            raise SystemExit(f"{name}: declaracion bool C no usa almacenamiento de byte: {pattern}")
    for operation in ("load", "store"):
        if len(re.findall(rf"\b{operation} i8\b[^\n]*@c_(?:flag|values)", ir)) < 2:
            raise SystemExit(f"{name}: faltan accesos {operation} i8 a las globales C")
    if re.search(r"\b(?:load|store) i1\b[^\n]*@c_(?:flag|values)", ir):
        raise SystemExit(f"{name}: una global C bool sigue usando accesos i1")
print("Declaraciones y accesos bool C de byte verificados en Clang y NSC.")
