"""Contrasta un record C++ indirecto después de dos enteros."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-int-int-record-", dir=r"A:\temp"))
case = root / "fixture"
case.mkdir()
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for file in source.iterdir():
    if not file.is_file() or file.name == "validar-final.py":
        continue
    text = file.read_text(encoding="utf-8")
    text = text.replace("sysv-cpp-int-record-int", "sysv-cpp-int-int-record")
    text = text.replace("int, Small, int)", "int, int, Small)")
    text = text.replace("int leading, Small p, int bias)", "int leading, int bias, Small p)")
    text = text.replace("(3,s,7)", "(3,7,s)")
    text = text.replace("@HU21@H@Z", "@HHU21@@Z")
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
print("OK: record al final, directo y precompilado; evidencia:", root)