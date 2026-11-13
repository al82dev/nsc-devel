"""Contrasta records C++ de cuatro y ocho bytes en un registro."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--nsc", required=True)
parser.add_argument("--clang-root", required=True)
parser.add_argument("--solo", choices=("4", "8"))
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-registro-unico-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for size in ((args.solo,) if args.solo else ("4", "8")):
    case = root / size
    case.mkdir()
    for file in source.iterdir():
        if not file.is_file() or file.name.startswith("validar-") and file.name != "validar-precompilado.py":
            continue
        text = file.read_text(encoding="utf-8")
        if file.name == "api.h":
            text = text.replace("int x,y,z;", "int x;" if size == "4" else "int x,y;")
        if file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
            text = text.replace("s{10,20,30}", "s{10}" if size == "4" else "s{10,20}")
            text = text.replace(" s.y=20; s.z=30;", "" if size == "4" else " s.y=20;")
            text = text.replace(" && s.y==20 && s.z==30", "" if size == "4" else " && s.y==20")
        if file.name == "validar.py":
            old = r"\(i64[^\n]*i32[^\n]*i32[^\n]*byval"
            new = r"\(i32[^\n]*i32[^\n]*byval" if size == "4" else r"\(i64[^\n]*i32[^\n]*byval"
            if old not in text:
                raise SystemExit("la comprobacion IR de referencia cambio")
            text = text.replace(old, new)
        (case / file.name).write_text(text, encoding="utf-8")
    for script in ("validar.py", "validar-precompilado.py"):
        command = [sys.executable, str(case / script), "--nsc", args.nsc, "--clang-root", args.clang_root]
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (size + "-" + script + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
        print(result.stdout, flush=True)
        if result.returncode:
            raise SystemExit(size + ": fallo " + script + "; evidencia: " + str(root))
print("OK: records seleccionados; evidencia:", root)
