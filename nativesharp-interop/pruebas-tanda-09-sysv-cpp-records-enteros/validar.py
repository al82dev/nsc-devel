"""Compara una familia de records C++ sysv_abi con Clang y precompilados."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--nsc", required=True)
parser.add_argument("--clang-root", required=True)
parser.add_argument("--solo-diez", action="store_true")
args = parser.parse_args()
source = Path(__file__).resolve().parent.parent / "pruebas-tanda-09-sysv-cpp-record24"
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-records-enteros-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
for count in ((10,) if args.solo_diez else (7, 8, 10)):
    case = root / str(count)
    case.mkdir()
    extra = ("t", "q", "r", "h")[:count - 6]
    for file in source.iterdir():
        if not file.is_file():
            continue
        text = file.read_text(encoding="utf-8")
        text = text.replace("sysv-cpp-record24", "sysv-cpp-records-" + str(count))
        text = text.replace("int x,y,z,w,v,u;", "int x,y,z,w,v,u," + ",".join(extra) + ";")
        text = text.replace("p.u+=6;", "p.u+=6; " + " ".join(
            "p." + name + "+=" + str(i + 7) + ";" for i, name in enumerate(extra)))
        text = text.replace("10,20,30,40,50,60}", "10,20,30,40,50,60," +
                            ",".join(str((i + 7) * 10) for i in range(len(extra))) + "}")
        text = text.replace("s.u=60;", "s.u=60; " + " ".join(
            "s." + name + "=" + str((i + 7) * 10) + ";" for i, name in enumerate(extra)))
        for variable, original in (("s", 60), ("a", 66), ("b", 66)):
            text = text.replace(variable + ".u==" + str(original),
                variable + ".u==" + str(original) + "".join(
                    " && " + variable + "." + name + "==" +
                    str((i + 7) * (10 if variable == "s" else 11))
                    for i, name in enumerate(extra)))
        if count >= 10 and file.name in ("Control.cpp", "api.cpp"):
            helper = ('extern "C" __attribute__((optnone,noinline)) '
                      'void* memcpy(void* d, const void* s, unsigned long long n) { '
                      'unsigned char* a=(unsigned char*)d; const unsigned char* b=(const unsigned char*)s; '
                      'for (unsigned long long i=0;i<n;++i) a[i]=b[i]; return d; }\n')
            if file.name == "api.cpp":
                helper = "#ifdef T09_BUILD_DLL_HELPER\n" + helper + "#endif\n"
            text = text.replace('#include "api.h"', '#include "api.h"\n' + helper)
        if count >= 10 and file.name == "validar.py":
            text = text.replace('flags = ["-target", "x86_64-pc-windows-msvc", "-std=c++20", "-O" + str(opt)]',
                                'flags = ["-target", "x86_64-pc-windows-msvc", "-std=c++20", "-O" + str(opt)] + (["-DT09_BUILD_DLL_HELPER"] if mode == "dll" else [])')
        if count >= 10 and file.name == "validar-precompilado.py":
            text = text.replace('"-std=c++20", "-O" + str(opt), "-c", str(producer / "api.cpp")',
                                '"-std=c++20", *(["-DT09_BUILD_DLL_HELPER"] if mode == "dll" else []), "-O" + str(opt), "-c", str(producer / "api.cpp")')
        (case / file.name).write_text(text, encoding="utf-8")
    scripts = ["validar.py", "validar-precompilado.py"]
    if count == 8:
        scripts.append("validar-rechazos.py")
    for script in scripts:
        command = [sys.executable, str(case / script), "--nsc", args.nsc,
                   "--clang-root", args.clang_root]
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors="replace")
        (root / (str(count) + "-" + script + ".log")).write_text(
            "COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) +
            "\n" + result.stdout, encoding="utf-8")
        print(result.stdout, flush=True)
        if result.returncode:
            raise SystemExit(str(count) + ": fallo " + script + "; evidencia: " + str(root))
print("OK: casos seleccionados, directos y precompilados; evidencia:", root)