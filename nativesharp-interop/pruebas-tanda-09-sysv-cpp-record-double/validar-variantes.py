"""Contrasta campos float y campos double/entero mezclados."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-record-float-mixto-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for variant in ("float", "mixto"):
    case = root / variant
    case.mkdir()
    for file in source.iterdir():
        if not file.is_file() or file.name.startswith("validar-variantes") or file.name == "validar-retorno.py":
            continue
        text = file.read_text(encoding="utf-8")
        text = text.replace("sysv-cpp-record-double", "sysv-cpp-record-" + variant)
        if variant == "float":
            changes = (
                ("struct Other { double x,y,z; };", "struct Other { float x,y,z,w,v; };"),
                ("T09Cpp::Other t{100.5,200.25,300.75};", "T09Cpp::Other t{100.5f,200.25f,300.75f,400.25f,500.5f};"),
                ("t.x=(double)100.5; t.y=(double)200.25; t.z=(double)300.75;",
                 "t.x=(float)100.5; t.y=(float)200.25; t.z=(float)300.75; t.w=(float)400.25; t.v=(float)500.5;"),
                ("t.z==300.75", "t.z==300.75 && t.w==400.25 && t.v==500.5"),
            )
        else:
            changes = (
                ("struct Other { double x,y,z; };", "struct Other { double x; long long y; double z; };"),
                ("T09Cpp::Other t{100.5,200.25,300.75};", "T09Cpp::Other t{100.5,200,300.75};"),
                ("t.y=(double)200.25;", "t.y=200;"),
                ("t.y==200.25", "t.y==200"),
            )
        for old, new in changes:
            text = text.replace(old, new)
        (case / file.name).write_text(text, encoding="utf-8")
    for script in ("validar.py", "validar-precompilado.py"):
        command = [sys.executable, str(case / script), "--nsc", args.nsc, "--clang-root", args.clang_root]
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (variant + "-" + script + ".log")).write_text("COMMAND: " + repr(command) +
            "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
        print(result.stdout, flush=True)
        if result.returncode:
            raise SystemExit(variant + ": fallo " + script + "; evidencia: " + str(root))
print("OK: campos float y mixtos, directos y precompilados; evidencia:", root)