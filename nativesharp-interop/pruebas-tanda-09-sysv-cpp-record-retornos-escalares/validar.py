"""Contrasta retornos escalares con records C++ indirectos."""
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
parser.add_argument("--solo", choices=("uint", "long", "ulong", "float", "double"))
args = parser.parse_args()
source = Path(__file__).resolve().parent.parent / "pruebas-tanda-09-sysv-cpp-record-retorno-int"
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-record-retornos-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
variants = {
    "uint": ("unsigned int", "uint", "4000000000U + p.x+bias+q.z", "(uint)4000000417"),
    "long": ("long long", "long", "-5000000000LL + p.x+bias+q.z", "-4999999583L"),
    "ulong": ("unsigned long long", "ulong", "5000000000ULL + p.x+bias+q.z", "(ulong)5000000417L"),
    "float": ("float", "float", "p.x+bias+q.z+0.5f", "417.5f"),
    "double": ("double", "double", "p.x+bias+q.z+0.25", "417.25"),
}
for label in ((args.solo,) if args.solo else variants):
    cpp_type, ns_type, expression, expected = variants[label]
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
        cpp_expected = {"uint": "4000000417U", "long": "-4999999583LL",
                        "ulong": "5000000417ULL", "float": "417.5f", "double": "417.25"}[label]
        value = cpp_expected if file.name == "Control.cpp" else expected
        text = text.replace("a==417", "a==" + value).replace("b==417", "b==" + value)
        if label in ("float", "double"):
            if file.name == "Control.cpp":
                text = text.replace('#include "api.h"', '#include "api.h"\nextern "C" { int _fltused = 0; }')
            if file.name == "api.cpp":
                text = text.replace('#include "api.h"', '#include "api.h"\n#ifdef T09_BUILD_DLL_HELPER\nextern "C" { int _fltused = 0; }\n#endif')
            if file.name == "validar.py":
                flag = 'flags = ["-target", "x86_64-pc-windows-msvc", "-std=c++20", "-O" + str(opt)]'
                text = text.replace(flag, flag + ' + (["-DT09_BUILD_DLL_HELPER"] if mode == "dll" else [])')
            if file.name == "validar-precompilado.py":
                text = text.replace('"-std=c++20", "-O" + str(opt), "-c", str(producer / "api.cpp")',
                    '"-std=c++20", *(["-DT09_BUILD_DLL_HELPER"] if mode == "dll" else []), "-O" + str(opt), "-c", str(producer / "api.cpp")')
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
    ir_type = {"uint": "i32", "long": "i64", "ulong": "i64", "float": "float", "double": "double"}[label]
    for file in (case / "validar.py", case / "validar-precompilado.py"):
        text = file.read_text(encoding="utf-8")
        text = text.replace("?most@T09Cpp@@YAHUSmall@1@HUOther@1@@Z", most)
        text = text.replace("?fetchCallback@T09Cpp@@YAP6AHUSmall@1@HUOther@1@@ZXZ", fetch)
        text = text.replace(r"i32\s+%[^\s(]+\(ptr", ir_type + r"\s+%[^\s(]+\(ptr")
        file.write_text(text, encoding="utf-8")
    for script in ("validar.py", "validar-precompilado.py"):
        command = [sys.executable, str(case / script), "--nsc", args.nsc, "--clang-root", args.clang_root]
        p = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (label + "-" + script + ".log")).write_text("COMMAND: " + repr(command) +
            "\nEXIT: " + str(p.returncode) + "\n" + p.stdout, encoding="utf-8")
        print(p.stdout, flush=True)
        if p.returncode:
            raise SystemExit(label + ": fallo " + script + "; evidencia: " + str(root))
print("OK: retornos escalares seleccionados; evidencia:", root)