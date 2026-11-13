"""Contrasta argumento y retorno void** con records C++ indirectos."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--nsc", required=True)
parser.add_argument("--clang-root", required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parent.parent / "pruebas-tanda-09-sysv-cpp-record-puntero-anidado"
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-voidptr-anidado-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for file in source.iterdir():
    if not file.is_file() or file.name == "validar-variantes.py":
        continue
    text = file.read_text(encoding="utf-8")
    if file.name in ("api.h", "api.cpp"):
        text = text.replace("int**", "void**").replace("**out+=", "*static_cast<int*>(*out)+=")
    elif file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
        text = text.replace("int* inner=&value;", "void* inner=" + ("&value;" if file.name == "Control.cpp" else "(void*)&value;"))
        text = text.replace("int** a", "void** a").replace("int** b", "void** b")
        text = text.replace("**b==627", "value==627").replace("**a=55", "value=55").replace("**b==55", "*b==inner")
    text = text.replace("sysv-cpp-record-puntero-anidado", "sysv-cpp-record-voidptr-anidado")
    (root / file.name).write_text(text, encoding="utf-8")
obj = root / "oracle.obj"
for command in (["clang.exe", "-target", "x86_64-pc-windows-msvc", "-std=c++20", "-c", str(root / "api.cpp"), "-o", str(obj)],
                ["llvm-nm.exe", "--defined-only", str(obj)]):
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    if result.returncode:
        raise SystemExit("fallo de referencia: " + result.stdout)
names = re.findall(r"\?[^\s]+", result.stdout)
most = next((name for name in names if name.startswith("?most@T09Cpp@@")), None)
fetch = next((name for name in names if name.startswith("?fetchCallback@T09Cpp@@")), None)
if not most or not fetch:
    raise SystemExit("simbolos de referencia ausentes")
for name in ("validar.py", "validar-precompilado.py"):
    file = root / name
    text = file.read_text(encoding="utf-8").replace("?most@T09Cpp@@YAPEAPEAHUSmall@1@PEAPEAHUOther@1@@Z", most)
    text = text.replace("?fetchCallback@T09Cpp@@YAP6APEAPEAHUSmall@1@PEAPEAHUOther@1@@ZXZ", fetch)
    file.write_text(text, encoding="utf-8")
    command = [sys.executable, str(file), "--nsc", args.nsc, "--clang-root", args.clang_root]
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    (root / (name + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(name + ": fallo; evidencia: " + str(root))
print("OK: void**; evidencia:", root)
