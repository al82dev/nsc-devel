import re
import sys
from pathlib import Path

clang_ir = Path(sys.argv[1]).read_text(encoding="utf-8")
native_ir = Path(sys.argv[2]).read_text(encoding="utf-8")
probe = re.search(r"^define[^\n]*@c_bool_probe\([^\n]*\)[^\n]*\{(.*?)^\}", clang_ir, re.M | re.S)
write = re.search(r"^define[^\n]*@c_bool_write\([^\n]*\)[^\n]*\{(.*?)^\}", clang_ir, re.M | re.S)
if not probe or not re.search(r"\bload i8\b", probe.group(1)):
    raise SystemExit("Clang: falta lectura de byte por puntero")
if not write or not re.search(r"\bstore i8\b", write.group(1)):
    raise SystemExit("Clang: falta escritura de byte por puntero")
if re.search(r"%pointer\.load\d* = load i1\b", native_ir):
    raise SystemExit("NSC: la lectura por puntero sigue usando i1")
if re.search(r"\bstore i1 (?:true|false), ptr %raw\.element\.addr", native_ir):
    raise SystemExit("NSC: la escritura por puntero sigue usando i1")
if not re.search(r"%pointer\.load\d* = load i8\b", native_ir):
    raise SystemExit("NSC: falta lectura de byte por puntero")
if not re.search(r"\bstore i8 [^,]+, ptr %raw\.element\.addr", native_ir):
    raise SystemExit("NSC: falta escritura de byte por puntero")
print("Lecturas y escrituras bool por puntero usan bytes en el IR comprobado.")
