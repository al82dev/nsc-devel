"""Contrasta retornos de puntero y de record con argumento Extra**."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--nsc", required=True)
parser.add_argument("--clang-root", required=True)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-record-anidado-retornos-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for label, directory in (("puntero", "pruebas-tanda-09-sysv-cpp-retorno-puntero-distinto"),
                         ("record", "pruebas-tanda-09-sysv-cpp-record-retorno-valor-puntero-distinto")):
    source = repo / "nativesharp-interop" / directory
    case = root / label
    case.mkdir()
    for file in source.iterdir():
        if not file.is_file():
            continue
        text = file.read_text(encoding="utf-8")
        if file.name in ("api.h", "api.cpp"):
            text = text.replace("Extra*", "Extra**").replace("out->x", "(*out)->x")
        elif file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
            cpp = file.name == "Control.cpp"
            text = text.replace("auto callback=T09Cpp::fetchCallback();", "T09Cpp::Extra* pointer=&out; auto callback=T09Cpp::fetchCallback();") if cpp else text.replace("T09Cpp.Callback callback=T09Cpp.fetchCallback();", "T09Cpp.Extra* pointer=&out; T09Cpp.Callback callback=T09Cpp.fetchCallback();")
            text = text.replace("s,&out", "s,&pointer").replace("out.x==", "pointer==&out && out.x==")
            if label == "puntero":
                text = text.replace("T09Cpp::Extra* a", "T09Cpp::Extra** a").replace("T09Cpp::Extra* b", "T09Cpp::Extra** b")
                text = text.replace("T09Cpp.Extra* a", "T09Cpp.Extra** a").replace("T09Cpp.Extra* b", "T09Cpp.Extra** b")
                text = text.replace("a==&out", "a==&pointer").replace("a->x", "(*a)->x").replace("(*a).x", "(**a).x")
        (case / file.name).write_text(text, encoding="utf-8")
    obj = case / "oracle.obj"
    for command in (["clang.exe", "-target", "x86_64-pc-windows-msvc", "-std=c++20", "-c", str(case / "api.cpp"), "-o", str(obj)],
                    ["llvm-nm.exe", "--defined-only", str(obj)]):
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        if result.returncode:
            raise SystemExit(label + ": fallo de referencia: " + result.stdout)
    names = re.findall(r"\?[^\s]+", result.stdout)
    most = next((name for name in names if name.startswith("?most@T09Cpp@@")), None)
    fetch = next((name for name in names if name.startswith("?fetchCallback@T09Cpp@@")), None)
    if not most or not fetch:
        raise SystemExit(label + ": simbolos de referencia ausentes")
    for name in ("validar.py", "validar-precompilado.py"):
        file = case / name
        text = file.read_text(encoding="utf-8")
        text = re.sub(r'\?most@T09Cpp@@[^"\'\s,]+', lambda _: most, text)
        text = re.sub(r'\?fetchCallback@T09Cpp@@[^"\'\s,]+', lambda _: fetch, text)
        file.write_text(text, encoding="utf-8")
        command = [sys.executable, str(file), "--nsc", args.nsc, "--clang-root", args.clang_root]
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (label + "-" + name + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
        print(result.stdout, flush=True)
        if result.returncode:
            raise SystemExit(label + ": fallo " + name + "; evidencia: " + str(root))
print("OK: retornos con puntero anidado; evidencia:", root)
