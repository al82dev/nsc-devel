"""Contrasta tres enteros después de un record C++ indirecto sysv_abi."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-record-tres-int-", dir=r"A:\temp"))
case = root / "fixture"
case.mkdir()
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for file in source.iterdir():
    if not file.is_file() or file.name == "validar-multiples.py":
        continue
    text = file.read_text(encoding="utf-8")
    text = text.replace("sysv-cpp-record-int", "sysv-cpp-record-tres-int")
    text = text.replace("Small, int)", "Small, int, int, int)")
    text = text.replace("Small p, int bias)", "Small p, int bias, int extra, int tail)")
    text = text.replace("p.y+=bias;", "p.y+=bias+extra*2+tail*3;")
    text = text.replace("(s,7)", "(s,7,2,3)")
    text = text.replace("a.y==27", "a.y==40").replace("b.y==27", "b.y==40")
    text = text.replace("@U21@H@Z", "@U21@HHH@Z")
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
print("OK: tres escalares directos y precompilados; evidencia:", root)