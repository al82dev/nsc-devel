"""Contrasta un retorno C++ distinto del único argumento record."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-retorno-distinto-", dir=r"A:\temp"))
case = root / "fixture"
case.mkdir()
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for file in source.iterdir():
    if not file.is_file() or file.name in ("validar-retorno-distinto.py", "validar-rechazos.py"):
        continue
    text = file.read_text(encoding="utf-8")
    replacements = (
        ("sysv-cpp-records-distintos", "sysv-cpp-retorno-distinto"),
        ("Small, int, Other)", "int, Other)"),
        ("Small p, int bias, Other q)", "int bias, Other q)"),
        ("{ p.x+=q.x;", "{ Small p{10,20,30,40,50,60}; p.x+=q.x;"),
        ("(s,7,t)", "(7,t)"),
        ("@U21@HUOther@1@@Z", "@HUOther@1@@Z"),
        (r"byval[^\n]*byval", "byval"),
    )
    for old, new in replacements:
        text = text.replace(old, new)
    (case / file.name).write_text(text, encoding="utf-8")
for script in ("validar.py", "validar-precompilado.py"):
    command = [sys.executable, str(case / script), "--nsc", args.nsc,
               "--clang-root", args.clang_root]
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    (root / (script + ".log")).write_text("COMMAND: " + repr(command) +
        "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
    print(result.stdout, flush=True)
    if result.returncode:
        raise SystemExit(script + ": fallo; evidencia: " + str(root))
print("OK: retorno distinto, directo y precompilado; evidencia:", root)