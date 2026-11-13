"""Contrasta el orden int, double en un record C++ de dos registros."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-mixto-inverso-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for file in source.iterdir():
    if not file.is_file() or file.name == Path(__file__).name:
        continue
    text = file.read_text(encoding="utf-8")
    if file.name == "api.h":
        text = text.replace("double x; int y;", "int x; double y;")
    if file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
        text = text.replace("s{10.0,20}", "s{10,20.0}").replace("s.x=10.0; s.y=20;", "s.x=10; s.y=20.0;")
        text = text.replace("s.x==10.0 && s.y==20", "s.x==10 && s.y==20.0")
    if file.name == "validar.py":
        text = text.replace(r"double[^\n]*i32[^\n]*i32", r"i32[^\n]*double[^\n]*i32")
    (root / file.name).write_text(text, encoding="utf-8")
for name in ("validar.py", "validar-precompilado.py"):
    command = [sys.executable, str(root / name), "--nsc", args.nsc, "--clang-root", args.clang_root]
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    (root / (name + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(name + ": fallo; evidencia: " + str(root))
print("OK: orden inverso; evidencia:", root)
