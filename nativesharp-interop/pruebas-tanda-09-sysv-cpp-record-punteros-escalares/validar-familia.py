"""Contrasta punteros a escalares mutables junto a records C++ indirectos."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-punteros-familia-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for label in ((args.solo,) if args.solo else variants):
    cpp_type, ns_type = variants[label]
    case = root / label
    case.mkdir()
    for file in source.iterdir():
        if not file.is_file() or file.name.startswith("validar-") and file.name != "validar-precompilado.py":
            continue
        text = file.read_text(encoding="utf-8")
        if file.name in ("api.h", "api.cpp"):
            text = text.replace("double*", cpp_type + "*").replace("short*", cpp_type + "*")
            text = text.replace("static double value", "static " + cpp_type + " value")
            text = text.replace("*out=(short)(p.x+q.z); value=317.5", "*out=(" + cpp_type + ")(p.x==10 && q.z==300); value=(" + cpp_type + ")" + ("1" if label == "bool" else "2"))
            if file.name == "api.h":
                text = text.replace("API_STORAGE Callback fetchCallback();", "API_STORAGE Callback fetchCallback();\nAPI_STORAGE int checkGuard();")
            else:
                text = text.replace("static " + cpp_type + " value;", "#pragma pack(push,1)\nstruct Guard { long long before; " + cpp_type + " value; int after; };\n#pragma pack(pop)\nstatic Guard slot{1234567, 0, 7654321};")
                text = text.replace("value=(", "slot.value=(").replace("return &value;", "return &slot.value;")
                text = text.replace("Callback fetchCallback() { return most; }", "Callback fetchCallback() { return most; }\nint checkGuard() { return slot.before==1234567 && slot.after==7654321; }")
        elif file.name in ("Control.cpp", "Main.ns", "Producer.ns"):
            target = cpp_type if file.name == "Control.cpp" else ns_type
            text = text.replace("short x=0", target + " x=" + ("false" if label == "bool" else "0"))
            text = text.replace("short y=0", target + " y=" + ("false" if label == "bool" else "0"))
            text = text.replace("double* a=", target + "* a=").replace("double* b=", target + "* b=")
            text = text.replace("x==310 && y==310", "x==" + ("true" if label == "bool" else "1") + " && y==" + ("true" if label == "bool" else "1"))
            text = text.replace("*a==317.5", "*a==" + ("true" if label == "bool" else "2"))
            text = text.replace("*a=55.25", "*a=" + ("false" if label == "bool" else "3"))
            text = text.replace("*b==55.25", "*b==" + ("false" if label == "bool" else "3"))
            check = "T09Cpp::checkGuard()" if file.name == "Control.cpp" else "T09Cpp.checkGuard()"
            text = text.replace("&& ok ?", "&& ok && " + check + "==1 ?")
        text = text.replace("sysv-cpp-record-punteros-escalares", "sysv-cpp-record-punteros-" + label)
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
    guard = next((name for name in names if name.startswith("?checkGuard@T09Cpp@@")), None)
    if not most or not fetch or not guard:
        raise SystemExit(label + ": simbolos de referencia ausentes")
    for name in ("validar.py", "validar-precompilado.py"):
        file = case / name
        text = file.read_text(encoding="utf-8").replace("?most@T09Cpp@@YAPEANUSmall@1@PEAFUOther@1@@Z", most)
        text = text.replace("?fetchCallback@T09Cpp@@YAP6APEANUSmall@1@PEAFUOther@1@@ZXZ", fetch)
        text = text.replace('"/export:' + fetch + '"', '"/export:' + fetch + '", "/export:' + guard + '"')
        file.write_text(text, encoding="utf-8")
        command = [sys.executable, str(file), "--nsc", args.nsc, "--clang-root", args.clang_root]
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (label + "-" + name + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" + result.stdout, encoding="utf-8")
        print(result.stdout, flush=True)
        if result.returncode:
            raise SystemExit(label + ": fallo " + name + "; evidencia: " + str(root))
print("OK: familia de punteros seleccionada; evidencia:", root)
