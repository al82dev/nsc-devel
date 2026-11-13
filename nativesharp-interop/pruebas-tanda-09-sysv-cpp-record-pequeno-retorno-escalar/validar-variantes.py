"""Contrasta un record de 16 bytes y un orden inverso de records."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-pequeno-variantes-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for label in ("16bytes", "orden-inverso"):
    case = root / label
    case.mkdir()
    for file in source.iterdir():
        if not file.is_file() or file.name == Path(__file__).name:
            continue
        text = file.read_text(encoding="utf-8")
        if label == "16bytes":
            if file.name == "api.h":
                text = text.replace("int x,y,z;", "int x,y,z,w;")
            if file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
                text = text.replace("s{10,20,30}", "s{10,20,30,40}").replace("s.z=30;", "s.z=30; s.w=40;")
                text = text.replace("s.z==30", "s.z==30 && s.w==40")
            if file.name == "validar.py":
                text = text.replace(r"i64[^\n]*i32[^\n]*i32", r"i64[^\n]*i64[^\n]*i32")
        else:
            if file.name == "api.h":
                text = text.replace("Small, int, Other", "Other, int, Small")
            if file.name == "api.cpp":
                text = text.replace("Small p, int bias, Other q", "Other q, int bias, Small p")
            if file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
                text = text.replace("s,7,t", "t,7,s")
            if file.name == "validar.py":
                text = text.replace(r"\(i64[^\n]*i32[^\n]*i32[^\n]*byval", r"\(ptr[^\n]*byval[^\n]*i32[^\n]*i64[^\n]*i32")
        (case / file.name).write_text(text, encoding="utf-8")
    obj = case / "oracle.obj"
    for command in (["clang.exe", "-target", "x86_64-pc-windows-msvc", "-std=c++20", "-c", str(case / "api.cpp"), "-o", str(obj)],
                    ["llvm-nm.exe", "--defined-only", str(obj)]):
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        if result.returncode:
            raise SystemExit(label + ": fallo de referencia: " + result.stdout)
    names = re.findall(r"\?[^\s]+", result.stdout)
    most = next((name for name in names if name.startswith("?most@T09Cpp@@")), None)
    fetch = next((name for name in names if name.startswith("?fetchCallback@T09Cpp@@")), None)
    if not most or not fetch:
        raise SystemExit(label + ": simbolos de referencia ausentes")
    for name in ("validar.py", "validar-precompilado.py"):
        file = case / name
        text = file.read_text(encoding="utf-8")
        text = re.sub(r'\?most@T09Cpp@@[^"\'\s,]+', lambda _: most, text)
        text = re.sub(r'\?fetchCallback@T09Cpp@@[^"\'\s,]+', lambda _: fetch, text)
        file.write_text(text, encoding="utf-8")
        command = [sys.executable, str(file), "--nsc", args.nsc, "--clang-root", args.clang_root]
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (label + "-" + name + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
        print(result.stdout, flush=True)
        if result.returncode:
            raise SystemExit(label + ": fallo " + name + "; evidencia: " + str(root))
print("OK: variantes seleccionadas; evidencia:", root)
