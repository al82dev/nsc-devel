import pathlib
import re
import sys

for filename in sys.argv[1:]:
    ir = pathlib.Path(filename).read_text(encoding="utf-8")
    for width, align in ((16, 2), (32, 4)):
        for operation, order in (("load", "acquire"), ("store", "release")):
            pattern = rf"\b{operation} atomic volatile i{width}\b[^\n]*\b{order}\b[^\n]*\balign {align}\b"
            if not re.search(pattern, ir):
                raise SystemExit(f"{filename}: falta {operation} volatile i{width} {order}")
    print(f"Orden volatile verificado: {filename}")
