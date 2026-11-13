"""Contrasta records con un campo float o double transmitido en un registro."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-flotante-simple-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for label in ("float", "double"):
    case = root / label
    case.mkdir()
    for file in source.iterdir():
        if not file.is_file() or file.name == Path(__file__).name:
            continue
        text = file.read_text(encoding="utf-8")
        if file.name == "api.h":
            text = text.replace("float x,y;", label + " x;")
        if file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
            text = text.replace("s{10.0f,20.0f}", "s{10.0f}" if label == "float" else "s{10.0}")
            text = text.replace("s.x=10.0f; s.y=20.0f;", "s.x=10.0f;" if label == "float" else "s.x=10.0;")
            text = text.replace("s.x==10.0f && s.y==20.0f", "s.x==10.0f" if label == "float" else "s.x==10.0")
        if file.name == "validar.py":
            text = text.replace("<2 x float>", label)
        (case / file.name).write_text(text, encoding="utf-8")
    for name in ("validar.py", "validar-precompilado.py"):
        command = [sys.executable, str(case / name), "--nsc", args.nsc, "--clang-root", args.clang_root]
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (label + "-" + name + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
        print(result.stdout, flush=True)
        if result.returncode:
            raise SystemExit(label + ": fallo " + name + "; evidencia: " + str(root))
print("OK: records flotantes simples; evidencia:", root)
