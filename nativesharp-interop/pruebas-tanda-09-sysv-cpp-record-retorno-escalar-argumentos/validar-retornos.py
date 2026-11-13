"""Contrasta retornos variados con argumentos escalares y dos records C++ indirectos."""
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
parser.add_argument("--solo", choices=("uint", "long", "ulong", "float", "double", "sbyte", "byte", "short", "ushort", "bool"))
args = parser.parse_args()
source = Path(__file__).resolve().parent
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-mixtos-retornos-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
# type C++, type NS, result true/false, expected true/false in each language, IR
variants = {
    "uint": ("unsigned int", "uint", "result==12 ? 4000000012U : 4000000011U", "4000000012U", "4000000011U", "(uint)4000000012", "(uint)4000000011", "i32"),
    "long": ("long long", "long", "result==12 ? -5000000012LL : -5000000011LL", "-5000000012LL", "-5000000011LL", "-5000000012L", "-5000000011L", "i64"),
    "ulong": ("unsigned long long", "ulong", "result==12 ? 9000000012ULL : 9000000011ULL", "9000000012ULL", "9000000011ULL", "(ulong)9000000012L", "(ulong)9000000011L", "i64"),
    "float": ("float", "float", "result==12 ? 12.5f : 11.5f", "12.5f", "11.5f", "12.5f", "11.5f", "float"),
    "double": ("double", "double", "result==12 ? 12.25 : 11.25", "12.25", "11.25", "12.25", "11.25", "double"),
    "sbyte": ("signed char", "sbyte", "(signed char)(result==12 ? -7 : -6)", "-7", "-6", "(sbyte)-7", "(sbyte)-6", "signext i8"),
    "byte": ("unsigned char", "byte", "(unsigned char)(result==12 ? 250 : 249)", "250", "249", "(byte)250", "(byte)249", "zeroext i8"),
    "short": ("short", "short", "(short)(result==12 ? -1000 : -999)", "-1000", "-999", "(short)-1000", "(short)-999", "signext i16"),
    "ushort": ("unsigned short", "ushort", "(unsigned short)(result==12 ? 65000 : 64999)", "65000", "64999", "(ushort)65000", "(ushort)64999", "zeroext i16"),
    "bool": ("bool", "bool", "result==12", "true", "false", "true", "false", "zeroext i1"),
}
original_most = "?most@T09Cpp@@YAHUSmall@1@CEFG_NI_J_KMNUOther@1@@Z"
original_fetch = "?fetchCallback@T09Cpp@@YAP6AHUSmall@1@CEFG_NI_J_KMNUOther@1@@ZXZ"
for label in ((args.solo,) if args.solo else variants):
    cpp_type, ns_type, expression, ca, cb, na, nb, ir_type = variants[label]
    case = root / label
    case.mkdir()
    for file in source.iterdir():
        if not file.is_file() or file.name == Path(__file__).name:
            continue
        text = file.read_text(encoding="utf-8")
        text = text.replace("sysv-cpp-record-retorno-escalar-argumentos", "sysv-cpp-record-mixtos-retorno-" + label)
        text = text.replace("using Callback = int", "using Callback = " + cpp_type)
        text = text.replace("API_STORAGE int __attribute__", "API_STORAGE " + cpp_type + " __attribute__")
        text = text.replace("int __attribute__((sysv_abi)) most", cpp_type + " __attribute__((sysv_abi)) most")
        text = text.replace("return result;", "return " + expression + ";")
        if file.name == "Control.cpp":
            text = text.replace("int a=T09Cpp::most", cpp_type + " a=T09Cpp::most").replace("int b=callback", cpp_type + " b=callback")
            text = text.replace("a==12 && b==11", "a==" + ca + " && b==" + cb)
        elif file.suffix == ".ns":
            text = text.replace("int a=T09Cpp.most", ns_type + " a=T09Cpp.most").replace("int b=callback", ns_type + " b=callback")
            text = text.replace("a==12 && b==11", "a==" + na + " && b==" + nb)
        (case / file.name).write_text(text, encoding="utf-8")
    obj = case / "oracle.obj"
    for command in (["clang.exe", "-target", "x86_64-pc-windows-msvc", "-std=c++20", "-O0", "-c", str(case / "api.cpp"), "-o", str(obj)],
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
        text = file.read_text(encoding="utf-8").replace(original_most, most).replace(original_fetch, fetch)
        text = text.replace(r"i32\s+%[^\s(]+\(", ir_type.replace(" ", r"\s+") + r"\s+%[^\s(]+\(")
        file.write_text(text, encoding="utf-8")
    for script in ("validar.py", "validar-precompilado.py"):
        command = [sys.executable, str(case / script), "--nsc", args.nsc, "--clang-root", args.clang_root]
        p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (label + "-" + script + ".log")).write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(p.returncode) + "\n" + p.stdout, encoding="utf-8")
        print(p.stdout, flush=True)
        if p.returncode:
            raise SystemExit(label + ": fallo " + script + "; evidencia: " + str(root))
print("OK: retornos seleccionados con argumentos mixtos; evidencia:", root)
