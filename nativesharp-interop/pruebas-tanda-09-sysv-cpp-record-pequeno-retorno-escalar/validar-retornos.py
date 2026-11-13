"""Contrasta retornos double y bool con record C++ pequeño y grande."""
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
interop = source.parent
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-pequeno-retornos-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for folder, label in (("pruebas-tanda-09-sysv-cpp-record-retornos-escalares", "double"),
                      ("pruebas-tanda-09-sysv-cpp-record-retornos-estrechos", "bool")):
    template = (interop / folder / "validar.py").read_text(encoding="utf-8")
    old = 'source = Path(__file__).resolve().parent.parent / "pruebas-tanda-09-sysv-cpp-record-retorno-int"'
    if old not in template:
        raise SystemExit("la matriz de referencia cambió")
    script = root / (label + ".py")
    template = template.replace(old, "source = Path(" + repr(str(source)) + ")")
    template = template.replace(r"\(ptr", r"\(i64")
    script.write_text(template, encoding="utf-8")
    command = [sys.executable, str(script), "--solo", label, "--nsc", args.nsc, "--clang-root", args.clang_root]
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    (root / (label + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(label + ": fallo; evidencia: " + str(root))
print("OK: retornos double y bool; evidencia:", root)
