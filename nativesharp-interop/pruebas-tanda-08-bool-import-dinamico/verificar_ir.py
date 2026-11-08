import re
import sys
from pathlib import Path

clang_ir = Path(sys.argv[1]).read_text(encoding="utf-8")
native_ir = Path(sys.argv[2]).read_text(encoding="utf-8")
clang = re.search(r"^define[^\n]*@Shared\([^\n]*\)", clang_ir, re.M)
if not clang or "zeroext i1" not in clang.group() or not re.search(r"\bi1\s+(?:noundef\s+)?zeroext\b", clang.group()):
    raise SystemExit("Clang: falta ABI bool de referencia")
for library in ("a", "b"):
    name = f"__nsc_import_{library}_dll__Shared"
    match = re.search(r"^define[^\n]*@" + name + r"\([^\n]*\)[^\n]*\{(.*?)^\}", native_ir, re.M | re.S)
    if not match:
        raise SystemExit(f"NSC: falta cuerpo de {name}")
    text = match.group()
    signature = text.splitlines()[0]
    if "zeroext i1" not in signature or not re.search(r"\bi1\s+zeroext\b", signature):
        raise SystemExit(f"NSC: firma sin zeroext para {name}: {signature}")
    forward = re.search(r"\bcall[^\n]*%import\.target\([^\n]*\)", text)
    if not forward or "zeroext i1" not in forward.group() or not re.search(r"\bi1\s+zeroext\b", forward.group()):
        raise SystemExit(f"NSC: reenvío sin zeroext para {name}")
print("Importaciones dinámicas bool: ABI de Clang y NSC coincide.")
