"""Contrasta argumento double* y retorno short* con dos records C++ indirectos."""
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
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-punteros-inversos-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for file in source.iterdir():
    if not file.is_file() or file.name == Path(__file__).name:
        continue
    text = file.read_text(encoding="utf-8")
    if file.name == "api.h":
        text = text.replace("double*", "TEMP_POINTER").replace("short*", "double*").replace("TEMP_POINTER", "short*")
    elif file.name == "api.cpp":
        text = text.replace("static double value; double*", "static short value; short*")
        text = text.replace("short* out", "double* out").replace("*out=(short)(p.x+q.z)", "*out=p.x+q.z+0.5")
        text = text.replace("value=317.5", "value=317")
    elif file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
        text = text.replace("short x=0", "double x=0").replace("short y=0", "double y=0")
        text = text.replace("double* a=", "short* a=").replace("double* b=", "short* b=")
        text = text.replace("x==310 && y==310", "x==310.5 && y==310.5")
        text = text.replace("*a==317.5", "*a==317").replace("*a=55.25", "*a=55").replace("*b==55.25", "*b==55")
    text = text.replace("sysv-cpp-record-punteros-escalares", "sysv-cpp-record-punteros-inversos")
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
    text = file.read_text(encoding="utf-8").replace("?most@T09Cpp@@YAPEANUSmall@1@PEAFUOther@1@@Z", most)
    text = text.replace("?fetchCallback@T09Cpp@@YAP6APEANUSmall@1@PEAFUOther@1@@ZXZ", fetch)
    file.write_text(text, encoding="utf-8")
    command = [sys.executable, str(file), "--nsc", args.nsc, "--clang-root", args.clang_root]
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    (root / (name + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(name + ": fallo; evidencia: " + str(root))
print("OK: punteros inversos; evidencia:", root)
