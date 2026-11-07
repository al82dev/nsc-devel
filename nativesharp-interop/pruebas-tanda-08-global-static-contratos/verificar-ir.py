import pathlib
import re
import sys

for filename in sys.argv[1:]:
    ir = pathlib.Path(filename).read_text(encoding="utf-8")
    for operation, order in (("load", "acquire"), ("store", "release")):
        pattern = rf"\b{operation} atomic volatile i32\b[^\n]*\b{order}\b[^\n]*\balign 4\b"
        if not re.search(pattern, ir):
            raise SystemExit(f"{filename}: falta {operation} volatile {order}")
    for operation in ("load", "store"):
        pattern = rf"\b{operation} atomic i32\b[^\n]*\bseq_cst\b[^\n]*\balign 4\b"
        if not re.search(pattern, ir):
            raise SystemExit(f"{filename}: falta {operation} atomico seq_cst")
    print(f"Orden volatile y atomico verificado: {filename}")

dll_ir = pathlib.Path(sys.argv[3]).read_text(encoding="utf-8")
tls_globals = re.findall(r"^@[^\n]*__nsc_c_inline_[^\n]*$", dll_ir, re.MULTILINE)
if sum("thread_local" in line for line in tls_globals) < 2:
    raise SystemExit("Faltan declaraciones de las dos globales internas TLS con referencia DLL")
if any("dllimport" in line or "__imp_" in line for line in tls_globals):
    raise SystemExit("Una global interna TLS fue tratada como importacion DLL")
print("Globales internas TLS no se declaran dllimport")
