import pathlib
import re
import sys

for filename in sys.argv[1:]:
    ir = pathlib.Path(filename).read_text(encoding="utf-8")
    for operation, order in (("load", "acquire"), ("store", "release")):
        pattern = rf"\b{operation} atomic volatile i32\b[^\n]*\b{order}\b[^\n]*\balign 4\b"
        if not re.search(pattern, ir):
            raise SystemExit(f"{filename}: falta {operation} volatile {order} de 32 bits")
    print(f"Orden y alineacion volatile verificados: {filename}")
