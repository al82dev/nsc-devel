"""Contrasta los mismos escalares con el record en otra posición."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-record-escalares-orden-", dir=r"A:\temp"))
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
        ("sysv-cpp-record-escalares", "sysv-cpp-record-escalares-orden"),
        ("Small, unsigned int, long long, unsigned long long, int*)",
         "unsigned int, Small, int*, long long, unsigned long long)"),
        ("Small p, unsigned int u, long long n, unsigned long long w, int* v)",
         "unsigned int u, Small p, int* v, long long n, unsigned long long w)"),
        ("(s,4000000000U,-5000000000LL,5000000000ULL,&value)",
         "(4000000000U,s,&value,-5000000000LL,5000000000ULL)"),
        ("(s,(uint)4000000000,-5000000000L,(ulong)5000000000L,&value)",
         "((uint)4000000000,s,&value,-5000000000L,(ulong)5000000000L)"),
        ("@U21@I_J_KPEAH@Z", "@IU21@PEAH_J_K@Z"),
        (r"sret[^\n]*byval[^\n]*i32[^\n]*i64[^\n]*i64[^\n]*ptr",
         r"sret[^\n]*i32[^\n]*byval[^\n]*ptr[^\n]*i64[^\n]*i64"),
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