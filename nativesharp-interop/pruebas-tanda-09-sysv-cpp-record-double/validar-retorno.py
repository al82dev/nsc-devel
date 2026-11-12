"""Contrasta un retorno C++ indirecto alineado a ocho bytes."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-retorno-double-", dir=r"A:\temp"))
case = root / "fixture"
case.mkdir()
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)
body = """T09Cpp.Small s; s.x=10; s.y=20; s.z=30; s.w=40; s.v=50; s.u=60;
T09Cpp.Other t; t.x=100.5; t.y=(double)200.25; t.z=(double)300.75;
T09Cpp.Callback callback=T09Cpp.fetchCallback(); T09Cpp.Callback direct=T09Cpp.most;
T09Cpp.Other a=T09Cpp.most(t,7,s); T09Cpp.Other b=callback(t,7,s);
return callback==direct && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 &&
t.x==100.5 && t.y==200.25 && t.z==300.75 &&
a.x==110.5 && a.y==207.25 && a.z==330.75 &&
b.x==110.5 && b.y==207.25 && b.z==330.75 ? 0 : 1;
"""
control = """#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
 T09Cpp::Small s{10,20,30,40,50,60}; T09Cpp::Other t{100.5,200.25,300.75};
 auto callback=T09Cpp::fetchCallback(); auto a=T09Cpp::most(t,7,s); auto b=callback(t,7,s);
 return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 &&
 t.x==100.5 && t.y==200.25 && t.z==300.75 &&
 a.x==110.5 && a.y==207.25 && a.z==330.75 && b.x==110.5 && b.y==207.25 && b.z==330.75 ? 0 : 1;
}
"""
implementation = """#include "api.h"
#ifdef T09_BUILD_DLL_HELPER
extern "C" { int _fltused = 0; }
#endif
namespace T09Cpp {
Other __attribute__((sysv_abi)) most(Other q, int bias, Small p) { q.x+=p.x; q.y+=bias; q.z+=p.z; p.x=999; return q; }
Callback fetchCallback() { return most; }
}
"""
for file in source.iterdir():
    if not file.is_file() or file.name == "validar-retorno.py":
        continue
    text = file.read_text(encoding="utf-8")
    text = text.replace("sysv-cpp-record-double", "sysv-cpp-retorno-double")
    text = text.replace("?most@T09Cpp@@YA?AUSmall@1@U21@HUOther@1@@Z",
                        "?most@T09Cpp@@YA?AUOther@1@U21@HUSmall@1@@Z")
    text = text.replace("?fetchCallback@T09Cpp@@YAP6A?AUSmall@1@U21@HUOther@1@@ZXZ",
                        "?fetchCallback@T09Cpp@@YAP6A?AUOther@1@U21@HUSmall@1@@ZXZ")
    text = text.replace(r"sret[^\n]*byval", r"sret[^\n]*align 8[^\n]*byval")
    if file.name == "api.h":
        text = text.replace("using Callback = Small", "using Callback = Other")
        text = text.replace("API_STORAGE Small __attribute__", "API_STORAGE Other __attribute__")
        text = text.replace("(Small, int, Other)", "(Other, int, Small)")
    elif file.name == "api.cpp":
        text = implementation
    elif file.name == "Control.cpp":
        text = control
    elif file.name == "Main.ns":
        text = "int Main() {\n" + body + "}\n"
    elif file.name == "Producer.ns":
        text = "namespace T09 { public class RegBridge { public static int Run<T>(T unused) {\n" + body + "} } }\n"
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
print("OK: retorno de doubles, directo y precompilado; evidencia:", root)