"""Contrasta float** e int*** con records C++ indirectos."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-anidados-variantes-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for label in ("float2", "int3"):
    case = root / label
    case.mkdir()
    for file in source.iterdir():
        if not file.is_file() or file.name == Path(__file__).name:
            continue
        text = file.read_text(encoding="utf-8")
        if label == "float2":
            if file.name in ("api.h", "api.cpp"):
                text = text.replace("int**", "float**")
            elif file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
                text = text.replace("int value=7", "float value=7").replace("int* inner", "float* inner").replace("int** a", "float** a").replace("int** b", "float** b")
            if file.name == "Control.cpp":
                text = text.replace('#include "api.h"', '#include "api.h"\nextern "C" { int _fltused = 0; }')
            if file.name == "api.cpp":
                text = text.replace('#include "api.h"', '#include "api.h"\n#ifdef T09_BUILD_DLL_HELPER\nextern "C" { int _fltused = 0; }\n#endif')
            if file.name == "validar.py":
                text = text.replace('*flags, "-c", str(case / "api.cpp")', '*flags, *(["-DT09_BUILD_DLL_HELPER"] if mode == "dll" else []), "-c", str(case / "api.cpp")')
            if file.name == "validar-precompilado.py":
                text = text.replace('"-std=c++20", "-O" + str(opt), "-c", str(producer / "api.cpp")', '"-std=c++20", "-O" + str(opt), *(["-DT09_BUILD_DLL_HELPER"] if mode == "dll" else []), "-c", str(producer / "api.cpp")')
        else:
            if file.name in ("api.h", "api.cpp"):
                text = text.replace("int**", "int***").replace("**out", "***out")
            elif file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
                text = text.replace("int* inner=&value;", "int* inner=&value; int** middle=&inner;")
                text = text.replace("int** a", "int*** a").replace("int** b", "int*** b")
                text = text.replace("(s,&inner,t)", "(s,&middle,t)").replace("a==&inner", "a==&middle")
                text = text.replace("*a==inner", "*a==middle && **a==inner").replace("**b", "***b").replace("**a=55", "***a=55")
        text = text.replace("sysv-cpp-record-puntero-anidado", "sysv-cpp-record-" + label)
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
        text = file.read_text(encoding="utf-8").replace("?most@T09Cpp@@YAPEAPEAHUSmall@1@PEAPEAHUOther@1@@Z", most)
        text = text.replace("?fetchCallback@T09Cpp@@YAP6APEAPEAHUSmall@1@PEAPEAHUOther@1@@ZXZ", fetch)
        file.write_text(text, encoding="utf-8")
        command = [sys.executable, str(file), "--nsc", args.nsc, "--clang-root", args.clang_root]
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (label + "-" + name + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
        print(result.stdout, flush=True)
        if result.returncode:
            raise SystemExit(label + ": fallo " + name + "; evidencia: " + str(root))
print("OK: variantes anidadas; evidencia:", root)
