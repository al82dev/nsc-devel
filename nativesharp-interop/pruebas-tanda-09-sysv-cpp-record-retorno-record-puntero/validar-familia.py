"""Contrasta punteros escalares con retorno y argumento record C++ indirectos."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

variants = {"sbyte": ("signed char", "sbyte"), "byte": ("unsigned char", "byte"),
            "short": ("short", "short"), "ushort": ("unsigned short", "ushort"),
            "int": ("int", "int"), "uint": ("unsigned int", "uint"),
            "long": ("long long", "long"), "ulong": ("unsigned long long", "ulong"),
            "float": ("float", "float"), "double": ("double", "double"), "bool": ("bool", "bool")}
parser = argparse.ArgumentParser()
parser.add_argument("--nsc", required=True)
parser.add_argument("--clang-root", required=True)
parser.add_argument("--solo", choices=tuple(variants))
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-record-punteros-familia-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for label in ((args.solo,) if args.solo else variants):
    cpp_type, ns_type = variants[label]
    case = root / label
    case.mkdir()
    for file in source.iterdir():
        if not file.is_file() or file.name == Path(__file__).name:
            continue
        text = file.read_text(encoding="utf-8")
        if file.name in ("api.h", "api.cpp"):
            text = text.replace("float*", cpp_type + "*")
            text = text.replace("float value;", cpp_type + " value;").replace("5.0f", "false" if label == "bool" else "5")
            if label == "bool":
                text = text.replace("*v+=4", "*v=!*v")
        elif file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
            target = cpp_type if file.name == "Control.cpp" else ns_type
            text = text.replace("float* value", target + "* value")
            if label == "bool":
                text = text.replace("*value==13", "*value==false")
        text = text.replace("sysv-cpp-record-retorno-record-puntero", "sysv-cpp-record-retorno-record-puntero-" + label)
        (case / file.name).write_text(text, encoding="utf-8")
    obj = case / "oracle.obj"
    for command in (["clang.exe", "-target", "x86_64-pc-windows-msvc", "-std=c++20", "-c", str(case / "api.cpp"), "-o", str(obj)],
                    ["llvm-nm.exe", "--defined-only", str(obj)]):
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        if result.returncode:
            raise SystemExit(label + ": fallo de referencia: " + result.stdout)
    names = re.findall(r"\?[^\s]+", result.stdout)
    resolved = {prefix: next((name for name in names if name.startswith("?" + prefix + "@T09Cpp@@")), None)
                for prefix in ("most", "fetchCallback", "guarded")}
    if not all(resolved.values()):
        raise SystemExit(label + ": simbolos de referencia ausentes")
    for name in ("validar.py", "validar-precompilado.py"):
        file = case / name
        text = file.read_text(encoding="utf-8")
        for old, prefix in (("?most@T09Cpp@@YA?AUSmall@1@U21@I_J_KPEAM@Z", "most"),
                            ("?fetchCallback@T09Cpp@@YAP6A?AUSmall@1@U21@I_J_KPEAM@ZXZ", "fetchCallback"),
                            ("?guarded@T09Cpp@@YAPEAMXZ", "guarded")):
            text = text.replace(old, resolved[prefix])
        file.write_text(text, encoding="utf-8")
        command = [sys.executable, str(file), "--nsc", args.nsc, "--clang-root", args.clang_root]
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (label + "-" + name + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
        print(result.stdout, flush=True)
        if result.returncode:
            raise SystemExit(label + ": fallo " + name + "; evidencia: " + str(root))
print("OK: familia seleccionada; evidencia:", root)
