"""T08-004-04e: `_Thread_local` C globals with and without `_Atomic`, in an executable and in a DLL.

Thread-local globals `int`, `_Atomic int`, `double` and `_Atomic long long`, plus a header `static
_Thread_local int` with a `static inline` C accessor. The main thread writes its copies and C reads them in
the same thread; a NativeSharp lambda run by a C-created thread sees the initial values, writes its own
copies (C reads them in that thread) and the main thread then still sees its own values. Checked against a
Clang control (same calls, a C callback), in an executable linked with a static C library, with the TLS
defined in a C DLL published with the executable, and through a precompiled template consumed without the
header, O0-O3. Rejection (O0): the address of a thread-local atomic global.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-globales-tls-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
linker = r"A:\al82dev-clang\bin\lld-link.exe"
crt = Path(r"A:\vc2026-runtime\lib\x64\release")
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
# Executables with C thread-locals link the static CRT TLS support; like the existing TLS tests
# (pruebas-tanda-08-tls-c, static-dos-bibliotecas-mutables/validar-tls.cmd) they use lld-link.
env["NSC_LINKER"] = linker
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
observe = "--observar" in sys.argv
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

THREAD_API = """typedef int (*TlsThreadCallback)(int value);
int run_tls_thread(TlsThreadCallback callback);
int c_tls_sum(void);
"""
THREAD_SOURCE = """typedef unsigned long Dword;
typedef void *Handle;
typedef Dword (__stdcall *ThreadRoutine)(void *);
typedef int (*TlsThreadCallback)(int);
__declspec(dllimport) Handle __stdcall CreateThread(void *, unsigned long long, ThreadRoutine, void *, Dword, Dword *);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
typedef struct Context { TlsThreadCallback callback; int result; } Context;
static Dword __stdcall worker(void *opaque)
{ Context *context = (Context *)opaque; context->result = context->callback(0); return 0; }
int run_tls_thread(TlsThreadCallback callback)
{
    Context context = {callback, 99};
    Handle thread = CreateThread(0, 0, worker, &context, 0, 0);
    if (!thread) return 3;
    Dword wait = WaitForSingleObject(thread, 0xffffffffUL);
    CloseHandle(thread);
    return wait == 0 ? context.result : 4;
}
"""
STATIC_TLS = """static _Thread_local int s_t = 5;
static inline int s_t_read(void) { return s_t; }
"""


def tls_header(dll):
    storage = "extern __declspec(thread)" if dll else "extern _Thread_local"
    return (f"{storage} int t_i;\n{storage} _Atomic int t_ai;\n{storage} double t_d;\n"
            f"{storage} _Atomic long long t_all;\n" + STATIC_TLS + THREAD_API)


def tls_source(dll):
    # A thread-local variable cannot be `dllexport`: the DLL exports it as DATA from the link line.
    storage = "__declspec(thread)" if dll else "_Thread_local"
    export = "__declspec(dllexport) " if dll else ""
    return (f"{storage} int t_i = 3;\n{storage} _Atomic int t_ai = 4;\n{storage} double t_d = 0.5;\n"
            f"{storage} _Atomic long long t_all = 6;\n"
            f"{export}int c_tls_sum(void) {{ return t_i + t_ai + (int)(t_d * 10) + (int)t_all; }}\n" +
            (THREAD_SOURCE.replace("int run_tls_thread", export + "int run_tls_thread")) +
            ("int _fltused = 0;\n" if dll else ""))


NS_WORKER = """    if (t_i != 3 || t_ai != 4 || t_d != 0.5 || t_all != 6 || s_t != 5 || s_t_read() != 5) return 1;
    t_i = 30; t_ai = 40; t_d = 2.5; t_all = 60; s_t = 50;
    if (c_tls_sum() != 155 || s_t_read() != 50) return 2;
    return 0;
"""
NS_MAIN = """    t_i = 13; t_ai = 14; t_d = 1.5; t_all = 16; s_t = 15;
    if (c_tls_sum() != 58 || s_t_read() != 15) return 1;
    TlsThreadCallback callback = (int value) => WORKER(value);
    int worker = run_tls_thread(callback);
    if (worker != 0) return 10 + worker;
    if (t_i != 13 || t_ai != 14 || t_d != 1.5 || t_all != 16 || s_t != 15 || s_t_read() != 15) return 3;
    return 0;
"""


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def fixture(folder, dll=False):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "api.h").write_text(tls_header(dll), encoding="utf-8")
    (folder / "crt.h").write_text("#pragma once\n", encoding="utf-8")
    (folder / "api.c").write_text(tls_source(dll), encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    if dll:
        run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/include:_tls_used", "/out:capi.dll",
             "/implib:capi.lib", "/export:t_i,DATA", "/export:t_ai,DATA", "/export:t_d,DATA", "/export:t_all,DATA",
             "api.obj", crt / "libcmt.lib", crt / "libvcruntime.lib", sdk / "ucrt/x64/libucrt.lib",
             sdk / "um/x64/kernel32.lib"], folder)
    else:
        run([lib, "/nologo", "/out:capi.lib", "api.obj"], folder)


# Thread-locals in the executable (the C static library's, and the header's `static _Thread_local` in every
# mode) need the CRT's TLS support (`_tls_index`, `_tls_used`) in the link.
CRT_REFERENCES = "".join(
    f'<CppLibraryReference Header="crt.h" Language="C" Standard="c11" Library="{path.as_posix()}" />'
    for path in (crt / "libcmt.lib", crt / "libcpmt.lib", crt / "libvcruntime.lib", sdk / "ucrt/x64/libucrt.lib"))


def reference(dll=False):
    runtime = ' Runtime="capi.dll"' if dll else ""
    return (f'<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="capi.lib"{runtime} />' +
            CRT_REFERENCES)


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
        if observe:
            print(folder.name, [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:4],
                  flush=True)
            return None
        raise AssertionError(f"{folder.name}: compilación:\n{result.stdout}")
    return run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode


def ns_program():
    return ("class Checks {\n    public static int Worker(int unused) {\n" + NS_WORKER + "    }\n"
            "    public static int Run() {\n" + NS_MAIN.replace("WORKER", "Checks.Worker") + "    }\n}\n"
            "int Main() { return Checks.Run(); }\n")


control = root / "control"
fixture(control)
c_main = (NS_MAIN.replace("TlsThreadCallback callback = (int value) => WORKER(value);", "")
          .replace("run_tls_thread(callback)", "run_tls_thread(worker_c)"))
(control / "Control.c").write_text('#include "api.h"\n'
                                   "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
                                   "static int worker_c(int unused) {\n" + NS_WORKER + "}\n"
                                   "static int checks(void) {\n" + c_main + "}\n"
                                   "void mainCRTStartup(void) { ExitProcess(checks()); }\n", encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], control)
run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/include:_tls_used",
     "/out:Control.exe", "Control.obj", "capi.lib", crt / "libcmt.lib", crt / "libvcruntime.lib",
     sdk / "ucrt/x64/libucrt.lib", sdk / "um/x64/kernel32.lib"], control)
code = run([control / "Control.exe"], control, check=False).returncode
if code:
    raise AssertionError(f"control C: comprobación {code} falló")
print("control C correcto", flush=True)

for opt in opts:
    for mode, dll in (("estatica", False), ("dll", True)):
        folder = root / f"directo-{mode}-O{opt}"
        fixture(folder, dll)
        (folder / "Main.ns").write_text(ns_program(), encoding="utf-8")
        project(folder, "Direct", ["Main.ns"], reference(dll))
        code = build_and_run(folder, "Direct", opt)
        if code:
            raise AssertionError(f"directo {mode} O{opt}: comprobación {code} falló")
        print("directo", mode, f"O{opt}", "correcto" if code == 0 else "observado", flush=True)
    folder = root / f"precompilado-O{opt}"
    fixture(folder / "producer")
    (folder / "producer" / "Producer.ns").write_text(
        "namespace Tls { public class Bridge {\n    public static int Worker<T>(T unused) {\n" + NS_WORKER +
        "    }\n    public static int Run<T>(T unused) {\n" +
        NS_MAIN.replace("WORKER(value)", "Worker<int>(value)") + "    }\n} }\n", encoding="utf-8")
    project(folder / "producer", "Tls", ["Producer.ns"], reference(), library=True)
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Tls.nsproj", "-O" + opt,
                  "--no-artifact-cache", "-o", "Tls.lib"], folder / "producer", check=False)
    if result.returncode:
        if observe:
            print("productor", [l for l in result.stdout.splitlines() if "error" in l][:3], flush=True)
            continue
        raise AssertionError("productor:\n" + result.stdout)
    (folder / "producer" / "api.h").unlink()
    (folder / "producer" / "crt.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Tls.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Tls.lib" />')
    code = build_and_run(folder, "Consumer", opt)
    if code:
        raise AssertionError(f"precompilado O{opt}: comprobación {code} falló")
    print("precompilado", f"O{opt}", "correcto" if code == 0 else "observado", flush=True)

folder = root / "rechazos" / "direccion-atomica"
fixture(folder)
(folder / "Main.ns").write_text("int Main() { var p = &t_ai; return 0; }\n", encoding="utf-8")
project(folder, "Negative", ["Main.ns"], reference())
result = build(folder, "Negative", "0")
expected = "cannot take the address of C atomic storage"
if observe:
    print("direccion-atomica", result.returncode, [l for l in result.stdout.splitlines() if "error" in l][:2])
elif result.returncode == 0 or expected not in result.stdout:
    raise AssertionError(f"rechazo direccion-atomica: se esperaba «{expected}»:\n{result.stdout}")
else:
    print("rechazo direccion-atomica correcto", flush=True)
print("observación terminada" if observe else "Globales C _Thread_local verificadas:", root)
