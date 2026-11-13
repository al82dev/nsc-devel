"""Contrasta distintas mezclas de campos float e int de 32 bits."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--nsc", required=True)
parser.add_argument("--clang-root", required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-mixto32-variantes-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
variants = {
    "iif": ("int x,y; float z;", "10,20,30.0f", "s.x=10; s.y=20; s.z=30.0f;", "s.x==10 && s.y==20 && s.z==30.0f", r"i64[^\n]*float[^\n]*i32"),
    "iff": ("int x; float y,z;", "10,20.0f,30.0f", "s.x=10; s.y=20.0f; s.z=30.0f;", "s.x==10 && s.y==20.0f && s.z==30.0f", r"i64[^\n]*float[^\n]*i32"),
    "ffii": ("float x,y; int z,w;", "10.0f,20.0f,30,40", "s.x=10.0f; s.y=20.0f; s.z=30; s.w=40;", "s.x==10.0f && s.y==20.0f && s.z==30 && s.w==40", r"<2 x float>[^\n]*i64[^\n]*i32"),
    "iiff": ("int x,y; float z,w;", "10,20,30.0f,40.0f", "s.x=10; s.y=20; s.z=30.0f; s.w=40.0f;", "s.x==10 && s.y==20 && s.z==30.0f && s.w==40.0f", r"i64[^\n]*<2 x float>[^\n]*i32"),
}
for label, (fields, values, init, check, ir) in variants.items():
    case = root / label
    case.mkdir()
    for file in source.iterdir():
        if not file.is_file() or file.name == Path(__file__).name:
            continue
        text = file.read_text(encoding="utf-8")
        if file.name == "api.h":
            text = text.replace("float x,y; int z;", fields)
        if file.name == "api.cpp" and label in ("ffii", "iiff"):
            text = text.replace("p.y!=20 || p.z!=30", "p.y!=20 || p.z!=30 || p.w!=40")
        if file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
            text = text.replace("s{10.0f,20.0f,30}", "s{" + values + "}")
            text = text.replace("s.x=10.0f; s.y=20.0f; s.z=30;", init)
            text = text.replace("s.x==10.0f && s.y==20.0f && s.z==30", check)
        if file.name == "validar.py":
            text = text.replace(r"<2 x float>[^\n]*i32[^\n]*i32", ir)
        (case / file.name).write_text(text, encoding="utf-8")
    for name in ("validar.py", "validar-precompilado.py"):
        command = [sys.executable, str(case / name), "--nsc", args.nsc, "--clang-root", args.clang_root]
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (label + "-" + name + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
        print(result.stdout, flush=True)
        if result.returncode:
            raise SystemExit(label + ": fallo " + name + "; evidencia: " + str(root))
print("OK: variantes mixtas; evidencia:", root)
