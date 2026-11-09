"""T08-D18: a C function converted to a C function pointer keeps its identity, as in C.

`BinOp op = mul;` and `s.op = mul;` store the address of the C symbol `mul`, so C sees `op == mul` and `s->op == mul`,
and NativeSharp compares equal to the pointer that C returns. A C function cannot raise a NativeSharp exception, so it
needs no callback boundary; a NativeSharp method converted to the same pointer type keeps its boundary (another
address) and is still callable from C. Checked with the C function in a static library and in a DLL (imported
through the IAT), O0-O3, directly and through a template of a precompiled producer consumed without the header, and
in the IR (no `mul__c_boundary` is emitted, the NativeSharp method keeps `__c_boundary`). Clang control.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-identidad-funcion-c-", dir=r"A:\nsc-interop-build"))
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
observe = "--observar" in sys.argv
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

TYPES = "typedef int (*BinOp)(int, int);\ntypedef struct Ops { BinOp op; BinOp other; } Ops;\n"
PROTOTYPES = ["int mul(int a, int b);", "int add(int a, int b);", "int is_mul(BinOp op);",
              "int ops_are_c(const Ops *ops);", "BinOp c_mul(void);", "int apply(BinOp op, int a, int b);"]


def header(qualifier):
    return TYPES + "".join(f"{qualifier}{prototype}\n" for prototype in PROTOTYPES)


SOURCE = """#include "api.h"
int mul(int a, int b) { return a * b; }
int add(int a, int b) { return a + b; }
int is_mul(BinOp op) { return op == mul; }
int ops_are_c(const Ops *ops) { return ops->op == mul && ops->other == add; }
BinOp c_mul(void) { return mul; }
int apply(BinOp op, int a, int b) { return op(a, b); }
"""
FUNCTIONS = "public static int NativeMul(int a, int b) { return a * b; }\n"
BODY = """        BinOp op = mul;
        if (is_mul(op) != 1 || op != c_mul() || apply(op, 6, 7) != 42) return 1;
        Ops s;
        s.op = mul;
        s.other = add;
        if (ops_are_c(&s) != 1 || s.op != c_mul() || s.op(3, 4) != 12 || s.other(3, 4) != 7) return 2;
        BinOp native = NativeMul;
        if (is_mul(native) != 0 || native == c_mul() || apply(native, 5, 5) != 25) return 3;
        BinOp again = mul;
        if (again != op) return 4;
        return 0;
"""


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def fixture(folder, dll):
    """EN: C library, static or DLL; the NativeSharp side sees `dllimport` for the DLL. ES: biblioteca C."""
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "api.h").write_text(header("__declspec(dllexport) " if dll else ""), encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    if dll:
        run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/out:capi.dll", "/implib:capi.lib",
             "api.obj"], folder)
        (folder / "api.h").write_text(header("__declspec(dllimport) "), encoding="utf-8")
    else:
        run([lib, "/nologo", "/out:capi.lib", "api.obj"], folder)


def reference(dll):
    return ('<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="capi.lib"' +
            (' Runtime="capi.dll"' if dll else "") + " />")


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
        "</ItemGroup></Project>", encoding="utf-8")


def build(folder, name, opt, kind="exe", output=None):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output or f"out/{name}.exe"], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


# EN: Clang control. ES: control Clang.
control = root / "control"
fixture(control, False)
(control / "Control.c").write_text(
    '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n'
    "static int NativeMul(int a, int b) { return a * b; }\n"
    "static int checks(void) {\n" + BODY + "}\nvoid mainCRTStartup(void) { ExitProcess(checks()); }\n",
    encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], control)
run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
     "Control.obj", "capi.lib", sdk / "um/x64/kernel32.lib"], control)
code = run([control / "Control.exe"], control, check=False).returncode
if code:
    raise AssertionError(f"control C: comprobación {code} falló")
print("control C correcto", flush=True)

# EN: IR: the C function is stored by address; only the NativeSharp method has a boundary.
# ES: IR: la funcion C se guarda por direccion; solo el metodo NativeSharp tiene frontera.
ir = root / "ir"
fixture(ir, False)
(ir / "Main.ns").write_text("public class Identity {\n" + FUNCTIONS + "public static int Run() {\n" + BODY +
                            "    }\n}\nint Main() { return Identity.Run(); }\n", encoding="utf-8")
project(ir, "Ir", ["Main.ns"], reference(False))
dump = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm", "Ir.nsproj"], ir, check=False)
(ir / "native.ll").write_text(dump.stdout, encoding="utf-8")
if dump.returncode or "mul__c_boundary" in dump.stdout or "add__c_boundary" in dump.stdout or \
        not re.search(r"NativeMul[^\n]*__c_boundary", dump.stdout):
    if observe:
        print("IR", dump.returncode, [l for l in dump.stdout.splitlines() if "boundary" in l][:6], flush=True)
    else:
        raise AssertionError("IR: la función C no debe tener frontera y el método sí:\n" + dump.stdout[:3000])
else:
    print("IR correcto: mul y add sin frontera, NativeMul con frontera", flush=True)

for dll in (False, True):
    kind = "dll" if dll else "estatica"
    for opt in opts:
        folder = root / f"directo-{kind}-O{opt}"
        fixture(folder, dll)
        (folder / "Main.ns").write_text("public class Identity {\n" + FUNCTIONS + "public static int Run() {\n" +
                                        BODY + "    }\n}\nint Main() { return Identity.Run(); }\n", encoding="utf-8")
        project(folder, "Direct", ["Main.ns"], reference(dll))
        result = build(folder, "Direct", opt)
        if result.returncode:
            if observe:
                print("directo", kind, f"O{opt}", errors(result), flush=True)
                continue
            raise AssertionError(f"directo {kind} O{opt}:\n{result.stdout}")
        code = run([folder / "out" / "Direct.exe"], Path("A:/"), check=False).returncode
        if code:
            raise AssertionError(f"directo {kind} O{opt}: comprobación {code} falló")
        print("directo", kind, f"O{opt}", "correcto", flush=True)
        folder = root / f"precompilado-{kind}-O{opt}"
        fixture(folder / "producer", dll)
        (folder / "producer" / "Producer.ns").write_text(
            "namespace Identity { public class Bridge {\n" + FUNCTIONS + "public static int Run<T>(T unused) {\n" +
            BODY + "    }\n} }\n", encoding="utf-8")
        project(folder / "producer", "Identity", ["Producer.ns"], reference(dll), library=True)
        result = build(folder / "producer", "Identity", opt, "lib", "Identity.lib")
        if result.returncode:
            raise AssertionError("productor:\n" + result.stdout)
        (folder / "producer" / "api.h").unlink()
        (folder / "Consumer.ns").write_text("int Main() { return Identity.Bridge.Run<int>(0); }\n", encoding="utf-8")
        project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Identity.lib" />')
        result = build(folder, "Consumer", opt)
        if result.returncode:
            if observe:
                print("consumidor", kind, f"O{opt}", errors(result), flush=True)
                continue
            raise AssertionError(f"consumidor {kind} O{opt}:\n{result.stdout}")
        code = run([folder / "out" / "Consumer.exe"], Path("A:/"), check=False).returncode
        if code:
            raise AssertionError(f"precompilado {kind} O{opt}: comprobación {code} falló")
        print("precompilado", kind, f"O{opt}", "correcto", flush=True)
print("observación terminada" if observe else "Identidad de funciones C verificada:", root)
