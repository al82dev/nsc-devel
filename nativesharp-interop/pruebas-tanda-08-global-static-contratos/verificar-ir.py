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
        pointer_pattern = rf"\b{operation} atomic (?:i64|ptr)\b[^\n]*\bseq_cst\b[^\n]*\balign 8\b"
        updates = len(re.findall(pointer_pattern, ir))
        if operation == "store":
            updates += len(re.findall(r"\batomicrmw add\b[^\n]*\bseq_cst\b[^\n]*\balign 8\b", ir))
        if updates < 2:
            raise SystemExit(f"{filename}: faltan dos operaciones {operation} atomicas de punteros")
    print(f"Orden volatile y atomico verificado: {filename}")

for filename in sys.argv[2:]:
    ir = pathlib.Path(filename).read_text(encoding="utf-8")
    loads = set(re.findall(r"load atomic i64, ptr @(__nsc_c_inline_\w+) seq_cst, align 8", ir))
    stores = set(re.findall(r"store atomic i64 [^\n]*, ptr @(__nsc_c_inline_\w+) seq_cst, align 8", ir))
    updates = set(re.findall(r"atomicrmw add ptr @(__nsc_c_inline_\w+), i64 [^\n]* seq_cst, align 8", ir))
    if len(loads) < 5 or len(loads & (stores | updates)) < 5:
        raise SystemExit(f"{filename}: los cinco punteros no tienen lecturas y escrituras atomicas")
    for offset in (-4, -1, 1, 4):
        pattern = rf"\batomicrmw add\b[^\n]*, i64 {offset} seq_cst, align 8\b"
        if not re.search(pattern, ir):
            raise SystemExit(f"{filename}: falta actualización atómica con desplazamiento {offset}")

dll_ir = pathlib.Path(sys.argv[3]).read_text(encoding="utf-8")
tls_globals = re.findall(r"^@[^\n]*__nsc_c_inline_[^\n]*$", dll_ir, re.MULTILINE)
if sum("thread_local" in line for line in tls_globals) < 2:
    raise SystemExit("Faltan declaraciones de las dos globales internas TLS con referencia DLL")
if any("dllimport" in line or "__imp_" in line for line in tls_globals):
    raise SystemExit("Una global interna TLS fue tratada como importacion DLL")
print("Globales internas TLS no se declaran dllimport")
