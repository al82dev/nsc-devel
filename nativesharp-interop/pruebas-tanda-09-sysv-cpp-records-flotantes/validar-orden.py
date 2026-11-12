"""Contrasta escalares flotantes y records C++ indirectos en otro orden."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-flotantes-orden-", dir=r"A:\temp"))
case = root / "fixture"
case.mkdir()
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for file in source.iterdir():
    if not file.is_file() or file.name == "validar-orden.py":
        continue
    text = file.read_text(encoding="utf-8")
    replacements = (
        ("sysv-cpp-records-flotantes", "sysv-cpp-flotantes-orden"),
        ("Small, float, Small, double, int)", "float, Small, int, double, Small)"),
        ("Small p, float scale, Small q, double drift, int bias)", "float scale, Small p, int bias, double drift, Small q)"),
        ("(s,1.5f,t,2.0,7)", "(1.5f,s,7,2.0,t)"),
        ("(s,1.5f,s,2.0,7)", "(1.5f,s,7,2.0,s)"),
        ("(s,(float)1.5,t,(double)2.0,7)", "((float)1.5,s,7,(double)2.0,t)"),
        ("(s,(float)1.5,s,(double)2.0,7)", "((float)1.5,s,7,(double)2.0,s)"),
        ("@U21@M0NH@Z", "@MU21@HN0@Z"),
        (r"sret[^\n]*byval[^\n]*float[^\n]*byval[^\n]*double[^\n]*i32",
         r"sret[^\n]*float[^\n]*byval[^\n]*i32[^\n]*double[^\n]*byval"),
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
print("OK: segundo orden, directo y precompilado; evidencia:", root)