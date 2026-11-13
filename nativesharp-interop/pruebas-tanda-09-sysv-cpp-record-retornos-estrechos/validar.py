"""Contrasta retornos estrechos con records C++ indirectos."""
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
parser.add_argument("--solo", choices=("sbyte", "byte", "short", "ushort", "bool"))
args = parser.parse_args()
source = Path(__file__).resolve().parent.parent / "pruebas-tanda-09-sysv-cpp-record-retorno-int"
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-record-retornos-estrechos-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
variants = {
    "sbyte": ("signed char", "sbyte", "(signed char)(p.x==110 && q.z==300 ? -7 : 1)", "-7", "(sbyte)-7", "signext", "i8"),
    "byte": ("unsigned char", "byte", "(unsigned char)(p.x==110 && q.z==300 ? 250 : 1)", "250", "(byte)250", "zeroext", "i8"),
    "short": ("short", "short", "(short)(p.x==110 && q.z==300 ? -1000 : 1)", "-1000", "(short)-1000", "signext", "i16"),
    "ushort": ("unsigned short", "ushort", "(unsigned short)(p.x==110 && q.z==300 ? 65000 : 1)", "65000", "(ushort)65000", "zeroext", "i16"),
    "bool": ("bool", "bool", "p.x==110 && q.z==300 && bias==7", "true", "true", "zeroext", "i1"),
}
for label in ((args.solo,) if args.solo else variants):
    cpp_type, ns_type, expression, cpp_expected, ns_expected, attribute, ir_type = variants[label]
    case = root / label
    case.mkdir()
    for file in source.iterdir():
        if not file.is_file() or file.name == "validar-rechazos.py":
            continue
        text = file.read_text(encoding="utf-8")
        text = text.replace("sysv-cpp-record-retorno-int", "sysv-cpp-record-retorno-" + label)
        text = text.replace("using Callback = int", "using Callback = " + cpp_type)
        text = text.replace("API_STORAGE int __attribute__", "API_STORAGE " + cpp_type + " __attribute__")
        text = text.replace("int __attribute__((sysv_abi)) most", cpp_type + " __attribute__((sysv_abi)) most")
        text = text.replace("return p.x+bias+q.z;", "return " + expression + ";")
        if file.name == "Control.cpp":
            text = text.replace("int a=T09Cpp::most", cpp_type + " a=T09Cpp::most")
            text = text.replace("int b=callback", cpp_type + " b=callback")
        elif file.suffix == ".ns":
            text = text.replace("int a=T09Cpp.most", ns_type + " a=T09Cpp.most")
            text = text.replace("int b=callback", ns_type + " b=callback")
        value = cpp_expected if file.name == "Control.cpp" else ns_expected
        text = text.replace("a==417", "a==" + value).replace("b==417", "b==" + ("false" if label == "bool" else value))
        if label == "bool":
            text = text.replace("b=callback(s,7,t)", "b=callback(s,8,t)")
        (case / file.name).write_text(text, encoding="utf-8")
    obj = case / "oracle.obj"
    for command in (["clang.exe", "-target", "x86_64-pc-windows-msvc", "-std=c++20", "-O0",
                     "-c", str(case / "api.cpp"), "-o", str(obj)],
                    ["llvm-nm.exe", "--defined-only", str(obj)]):
        p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        if p.returncode:
            raise SystemExit(label + ": fallo de referencia; " + p.stdout)
    names = re.findall(r"\?[^\s]+", p.stdout)
    most = next((name for name in names if name.startswith("?most@T09Cpp@@")), None)
    fetch = next((name for name in names if name.startswith("?fetchCallback@T09Cpp@@")), None)
    if not most or not fetch:
        raise SystemExit(label + ": símbolos de referencia ausentes")
    for file in (case / "validar.py", case / "validar-precompilado.py"):
        text = file.read_text(encoding="utf-8")
        text = text.replace("?most@T09Cpp@@YAHUSmall@1@HUOther@1@@Z", most)
        text = text.replace("?fetchCallback@T09Cpp@@YAP6AHUSmall@1@HUOther@1@@ZXZ", fetch)
        text = text.replace(r"i32\s+%[^\s(]+\(ptr", attribute + r"\s+" + ir_type + r"\s+%[^\s(]+\(ptr")
        file.write_text(text, encoding="utf-8")
    for script in ("validar.py", "validar-precompilado.py"):
        command = [sys.executable, str(case / script), "--nsc", args.nsc, "--clang-root", args.clang_root]
        p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (label + "-" + script + ".log")).write_text("COMMAND: " + repr(command) +
            "\nEXIT: " + str(p.returncode) + "\n" + p.stdout, encoding="utf-8")
        print(p.stdout, flush=True)
        if p.returncode:
            raise SystemExit(label + ": fallo " + script + "; evidencia: " + str(root))
print("OK: retornos estrechos seleccionados; evidencia:", root)