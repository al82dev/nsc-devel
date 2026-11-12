"""Contrasta tres copias de un record C++ indirecto sysv_abi."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-tres-records-", dir=r"A:\temp"))
case = root / "fixture"
case.mkdir()
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for file in source.iterdir():
    if not file.is_file() or file.name == "validar-tres.py":
        continue
    text = file.read_text(encoding="utf-8")
    replacements = (
        ("sysv-cpp-dos-records", "sysv-cpp-tres-records"),
        ("Small, int, Small)", "Small, int, Small, Small)"),
        ("Small p, int bias, Small q)", "Small p, int bias, Small q, Small r)"),
        ("p.x+=q.x;", "p.x+=q.x+r.x;"),
        ("p.z+=q.z;", "p.z+=q.z+r.z;"),
        ("(s,7,t)", "(s,7,t,s)"),
        ("(s,7,s)", "(s,7,s,t)"),
        ("a.x==110", "a.x==120"), ("b.x==110", "b.x==120"), ("c.x==20", "c.x==120"),
        ("a.z==330", "a.z==360"), ("b.z==330", "b.z==360"), ("c.z==60", "c.z==360"),
        ("@U21@H0@Z", "@U21@H00@Z"),
        (r"byval[^\n]*byval", r"byval[^\n]*byval[^\n]*byval"),
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
print("OK: tres records, directos y precompilados; evidencia:", root)