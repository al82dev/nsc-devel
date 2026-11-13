"""Contrasta argumentos estrechos a ambos lados del record."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-estrechos-orden-", dir=r"A:\temp"))
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
        ("sysv-cpp-record-estrechos", "sysv-cpp-estrechos-orden"),
        ("Small, signed char, unsigned char, short, unsigned short, bool)",
         "signed char, Small, bool, unsigned char, short, unsigned short)"),
        ("Small p, signed char a, unsigned char b, short c, unsigned short d, bool flag)",
         "signed char a, Small p, bool flag, unsigned char b, short c, unsigned short d)"),
        ("(s,-7,250,-1000,65000,true)", "(-7,s,true,250,-1000,65000)"),
        ("(s,-7,250,-1000,65000,false)", "(-7,s,false,250,-1000,65000)"),
        ("(s,(sbyte)-7,(byte)250,(short)-1000,(ushort)65000,true)",
         "((sbyte)-7,s,true,(byte)250,(short)-1000,(ushort)65000)"),
        ("(s,(sbyte)-7,(byte)250,(short)-1000,(ushort)65000,false)",
         "((sbyte)-7,s,false,(byte)250,(short)-1000,(ushort)65000)"),
        ("@U21@CEFG_N@Z", "@CU21@_NEFG@Z"),
        (r"sret[^\n]*byval[^\n]*i8 signext[^\n]*i8 zeroext[^\n]*i16 signext[^\n]*i16 zeroext[^\n]*i1 zeroext",
         r"sret[^\n]*i8 signext[^\n]*byval[^\n]*i1 zeroext[^\n]*i8 zeroext[^\n]*i16 signext[^\n]*i16 zeroext"),
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