"""T08-003-05: visibility and linkage of C functions.

a) Calls from a nested namespace (class method and namespace function); a local of the same name hides the
   C function only inside its scope, and calling the local as a function is rejected.
b) The same calls linked against a static library and against a DLL (`__declspec(dllimport)`, packaged
   with the executable), O0-O3; a declared function without definition fails to link with its name.
c) A precompiled template that calls both functions, consumed with the header present in the consumer and
   with the header hidden, O0-O3.
A Clang control makes the same calls through the same libraries.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-funciones-enlace-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
linker = r"A:\al82dev-clang\bin\lld-link.exe"
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

STATIC_HEADER = "int add_c(int a, int b);\nint shadow_me(int x);\n"
DLL_HEADER = "__declspec(dllimport) int dll_mul(int a, int b);\n__declspec(dllimport) int dll_shadow(int x);\n"
STATIC_SOURCE = "int add_c(int a, int b) { return a + b; }\nint shadow_me(int x) { return x * 3; }\n"
DLL_SOURCE = ("__declspec(dllexport) int dll_mul(int a, int b) { return a * b; }\n"
              "__declspec(dllexport) int dll_shadow(int x) { return x * 5; }\n")
MAIN = """namespace App.Inner
{
    public class Calls
    {
        public static int Run()
        {
            if (add_c(2, 3) != 5 || dll_mul(4, 5) != 20) return 1;
            {
                int shadow_me = 7;
                int dll_shadow = 8;
                if (shadow_me + dll_shadow != 15) return 2;
            }
            if (shadow_me(3) != 9 || dll_shadow(3) != 15) return 3;
            return 0;
        }
    }
    int Twice() { return add_c(1, 1) + dll_mul(1, 1); }
}
int Main() { return App.Inner.Calls.Run() + (App.Inner.Twice() == 3 ? 0 : 10); }
"""


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def fixtures(folder):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "static.h").write_text(STATIC_HEADER, encoding="utf-8")
    (folder / "dll.h").write_text(DLL_HEADER, encoding="utf-8")
    (folder / "static.c").write_text(STATIC_SOURCE, encoding="utf-8")
    (folder / "dll.c").write_text(DLL_SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "static.c", "-o", "static.obj"], folder)
    run([lib, "/nologo", "/out:cstatic.lib", "static.obj"], folder)
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "dll.c", "-o", "dll.obj"], folder)
    run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/out:cdll.dll", "/implib:cdll.lib", "dll.obj"],
        folder)


def references(prefix=""):
    return (f'<CppLibraryReference Header="{prefix}static.h" Language="C" Standard="c11" '
            f'Library="{prefix}cstatic.lib" />'
            f'<CppLibraryReference Header="{prefix}dll.h" Language="C" Standard="c11" Library="{prefix}cdll.lib" '
            f'Runtime="{prefix}cdll.dll" />')


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
        "</ItemGroup></Project>", encoding="utf-8")


def build(folder, name, opt):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", f"{name}.nsproj", "-O" + opt,
                "--no-artifact-cache", "-o", f"out/{name}.exe"], folder, check=False)


def build_and_run(folder, name, opt):
    result = build(folder, name, opt)
    if result.returncode:
        raise AssertionError(f"{folder.name}: compilación:\n{result.stdout}")
    if not (folder / "out" / "cdll.dll").exists():
        raise AssertionError(f"{folder.name}: la DLL C no se publicó junto al ejecutable")
    # Run from another folder: only the packaged DLL next to the executable can satisfy the import.
    return run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode


control = root / "control"
fixtures(control)
(control / "Control.c").write_text('#include "static.h"\n#include "dll.h"\n'
                                   "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
                                   "void mainCRTStartup(void) { ExitProcess(add_c(2, 3) != 5 || dll_mul(4, 5) != 20 ||"
                                   " shadow_me(3) != 9 || dll_shadow(3) != 15); }\n", encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], control)
run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
     "Control.obj", "cstatic.lib", "cdll.lib", sdk / "um/x64/kernel32.lib"], control)
if run([control / "Control.exe"], control, check=False).returncode:
    raise AssertionError("control C falló")
print("control C correcto", flush=True)

for opt in opts:
    folder = root / f"directo-O{opt}"
    fixtures(folder)
    (folder / "Main.ns").write_text(MAIN, encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], references())
    code = build_and_run(folder, "Direct", opt)
    if code:
        raise AssertionError(f"directo O{opt}: comprobación {code} falló")
    print("directo", f"O{opt}", "correcto", flush=True)

    folder = root / f"precompilado-O{opt}"
    fixtures(folder / "producer")
    (folder / "producer" / "Producer.ns").write_text(
        "namespace Linked { public class Bridge { public static int Run<T>(T unused) { "
        "int shadow_me = 1; return add_c(2, 3) * 100 + dll_mul(4, 5) + shadow_me * 0 + dll_shadow(0); } } }\n",
        encoding="utf-8")
    project(folder / "producer", "Linked", ["Producer.ns"], references(), library=True)
    run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Linked.nsproj", "-O" + opt,
         "--no-artifact-cache", "-o", "Linked.lib"], folder / "producer")
    for mode in ("con-encabezado", "sin-encabezado"):
        consumer = folder / mode
        consumer.mkdir()
        (consumer / "Consumer.ns").write_text(
            "int Main() { return Linked.Bridge.Run<int>(0) == 520 ? 0 : 1; }\n", encoding="utf-8")
        refs = '<LibraryReference Include="../producer/Linked.lib" />'
        if mode == "con-encabezado":
            refs += references("../producer/")
        project(consumer, "Consumer", ["Consumer.ns"], refs)
        if mode == "sin-encabezado":
            for header in ("static.h", "dll.h"):
                hidden = folder / "producer" / header
                if hidden.exists():
                    hidden.rename(folder / "producer" / (header + ".oculto"))
        code = build_and_run(consumer, "Consumer", opt)
        if code:
            raise AssertionError(f"precompilado {mode} O{opt}: comprobación {code} falló")
        print("precompilado", mode, f"O{opt}", "correcto", flush=True)

REJECTIONS = [
    ("local-llamado", "int shadow_me = 7; return shadow_me(3);", "call target is not an instance method group"),
    ("sin-definicion", "return undefined_c(1);", "unresolved external symbol: 'undefined_c'"),
]
for name, statements, expected in REJECTIONS:
    folder = root / "rechazos" / name
    fixtures(folder)
    (folder / "extra.h").write_text("int undefined_c(int x);\n", encoding="utf-8")
    (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
    project(folder, "Negative", ["Main.ns"], references() +
            '<CppLibraryReference Header="extra.h" Language="C" Standard="c11" />')
    result = build(folder, "Negative", "0")
    if "--observar" in sys.argv:
        print(name, result.returncode, [l for l in result.stdout.splitlines() if "error" in l][:2], flush=True)
        continue
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)
print("Visibilidad y enlace de funciones C verificados:", root)
