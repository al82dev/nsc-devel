"""T08-D07: `_Atomic` C globals of struct and union type, lock-free and through the atomic library.

Clang (x86_64-pc-windows-msvc) widens an atomic record of up to 16 bytes to the next power of two and aligns it to
that width. Widths 1, 2, 4 and 8 are accessed with `load/store atomic iN seq_cst`; every other width (12 -> 16,
16, 24) goes through the generic `__atomic_load`/`__atomic_store` calls of the atomic library, which the C side
supplies here (a lock-based implementation that also counts its calls). NativeSharp must use the same protocol
so that both languages agree on the object.

Suite "basica": external atomic records of 3, 8, 12, 16 and 24 bytes and a 4-byte union. NativeSharp reads every
record as a whole value, modifies a local copy and writes it back; C checks the values with atomic reads, writes
new ones, and NativeSharp reads them again. NativeSharp also checks that only the non lock-free records call the
library. Suite "variantes": `_Thread_local` and `volatile` atomic records (lock-free and not), and `static` atomic
records defined in the header, shared with `static inline` C accessors. Suite "dll": atomic records of 8 and 12
bytes exported as data by a C DLL that also exports the atomic library. The IR shows `load/store atomic` (volatile
where it applies) and the library calls with their sizes. Every suite is checked against a Clang control and
directly, and the first two also through a precompiled template consumed without the header, O0-O3. Rejections
(O0): reading and writing a member in place, and taking the address of the record or of a member.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-atomicas-record-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
linker = r"A:\al82dev-clang\bin\lld-link.exe"
crt = Path(r"A:\vc2026-runtime\lib\x64\release")
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
# EN: executables with C thread-locals link the static CRT TLS support with lld-link, as in validar-tls.py.
# ES: los ejecutables con thread-locals C enlazan el soporte TLS del CRT estatico con lld-link, como validar-tls.py.
tls_env = dict(env, NSC_LINKER=linker)
observe = "--observar" in sys.argv
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

BASIC_HEADER = """typedef struct R3 { unsigned char a, b, c; } R3;
typedef struct R8 { int a; int b; } R8;
typedef union U4 { float f; int i; } U4;
typedef struct R12 { int a, b, c; } R12;
typedef struct R16 { long long a; long long b; } R16;
typedef struct R24 { long long a, b, c; } R24;
extern _Atomic R3 a_r3;
extern _Atomic R8 a_r8;
extern _Atomic U4 a_u4;
extern _Atomic R12 a_r12;
extern _Atomic R16 a_r16;
extern _Atomic R24 a_r24;
int c_reads_ok(void);
void c_writes(void);
int lib_calls(void);
"""
BASIC_SOURCE = """#include "api.h"
_Atomic R3 a_r3 = (R3){1, 2, 3};
_Atomic R8 a_r8 = (R8){4, 5};
_Atomic U4 a_u4 = (U4){.i = 6};
_Atomic R12 a_r12 = (R12){7, 8, 9};
_Atomic R16 a_r16 = (R16){10, 11};
_Atomic R24 a_r24 = (R24){12, 13, 14};
int c_reads_ok(void)
{
    R3 v3 = a_r3; R8 v8 = a_r8; U4 u4 = a_u4; R12 v12 = a_r12; R16 v16 = a_r16; R24 v24 = a_r24;
    return v3.a == 101 && v3.b == 2 && v3.c == 203 && v8.a == -4 && v8.b == 50 && u4.f == 2.5f &&
        v12.a == 70 && v12.b == 8 && v12.c == -90 && v16.a == 10000000000LL && v16.b == -11 &&
        v24.a == 120 && v24.b == 13 && v24.c == -14000000000LL;
}
void c_writes(void)
{
    R3 v3 = {31, 32, 33}; R8 v8 = {81, 82}; U4 u4 = {.i = 41}; R12 v12 = {121, 122, 123};
    R16 v16 = {161, -162}; R24 v24 = {241, 242, 243};
    a_r3 = v3; a_r8 = v8; a_u4 = u4; a_r12 = v12; a_r16 = v16; a_r24 = v24;
}
"""
BASIC_BODY = """    int k0 = lib_calls();
    R3 v3 = a_r3; R8 v8 = a_r8; U4 u4 = a_u4;
    if (lib_calls() != k0) return 1;
    if (v3.a != 1 || v3.b != 2 || v3.c != 3 || v8.a != 4 || v8.b != 5 || u4.i != 6) return 2;
    R12 v12 = a_r12; R16 v16 = a_r16; R24 v24 = a_r24;
    if (lib_calls() != k0 + 3) return 3;
    if (v12.a != 7 || v12.b != 8 || v12.c != 9 || v16.a != 10 || v16.b != 11) return 4;
    if (v24.a != 12 || v24.b != 13 || v24.c != 14) return 5;
    v3.a = 101; v3.c = 203; v8.a = -4; v8.b = 50; u4.f = 2.5f;
    a_r3 = v3; a_r8 = v8; a_u4 = u4;
    if (lib_calls() != k0 + 3) return 6;
    v12.a = 70; v12.c = -90; v16.a = 10000000000L; v16.b = -11; v24.a = 120; v24.c = -14000000000L;
    a_r12 = v12; a_r16 = v16; a_r24 = v24;
    if (lib_calls() != k0 + 6) return 7;
    if (c_reads_ok() != 1) return 8;
    c_writes();
    R3 w3 = a_r3; R8 w8 = a_r8; U4 w4 = a_u4; R12 w12 = a_r12; R16 w16 = a_r16; R24 w24 = a_r24;
    if (w3.a != 31 || w3.b != 32 || w3.c != 33 || w8.a != 81 || w8.b != 82 || w4.i != 41) return 9;
    if (w12.a != 121 || w12.b != 122 || w12.c != 123 || w16.a != 161 || w16.b != -162) return 10;
    if (w24.a != 241 || w24.b != 242 || w24.c != 243) return 11;
    return 0;
"""
VARIANT_HEADER = """typedef struct V8 { int a; int b; } V8;
typedef struct V12 { int a, b, c; } V12;
typedef struct V16 { long long a; long long b; } V16;
extern _Thread_local _Atomic V8 t_v8;
extern _Thread_local _Atomic V12 t_v12;
extern volatile _Atomic V8 v_v8;
extern volatile _Atomic V16 v_v16;
static _Atomic V8 s_v8 = (V8){10, 11};
static _Atomic V12 s_v12 = (V12){12, 13, 14};
static inline int s_sum(void) { V8 e = s_v8; V12 f = s_v12; return e.a + e.b + f.a + f.c; }
static inline void s_reset(void) { s_v8 = (V8){1, 1}; s_v12 = (V12){1, 1, 1}; }
int c_reads_ok(void);
void c_writes(void);
int lib_calls(void);
"""
VARIANT_SOURCE = """#include "api.h"
_Thread_local _Atomic V8 t_v8 = (V8){1, 2};
_Thread_local _Atomic V12 t_v12 = (V12){3, 4, 5};
volatile _Atomic V8 v_v8 = (V8){6, 7};
volatile _Atomic V16 v_v16 = (V16){8, 9};
int c_reads_ok(void)
{
    V8 a = t_v8; V12 b = t_v12; V8 c = v_v8; V16 d = v_v16;
    return a.a == 100 && a.b == 2 && b.c == 500 && c.a == 600 && d.a == 8 && d.b == 900;
}
void c_writes(void)
{
    t_v8 = (V8){1, -2}; t_v12 = (V12){-3, 4, 5}; v_v8 = (V8){6, -7}; v_v16 = (V16){-8, 9};
}
"""
VARIANT_BODY = """    int k0 = lib_calls();
    V8 a = t_v8; V8 c = v_v8; V8 e = s_v8;
    if (lib_calls() != k0) return 21;
    V12 b = t_v12; V16 d = v_v16; V12 f = s_v12;
    if (lib_calls() != k0 + 3) return 22;
    if (a.a != 1 || a.b != 2 || b.c != 5 || c.a != 6 || d.b != 9 || e.b != 11 || f.c != 14) return 23;
    a.a = 100; b.c = 500; c.a = 600; d.b = 900; e.a = 1000; f.c = 1400;
    t_v8 = a; t_v12 = b; v_v8 = c; v_v16 = d; s_v8 = e; s_v12 = f;
    if (c_reads_ok() != 1) return 24;
    if (s_sum() != 2423) return 25;
    s_reset();
    V8 g8 = s_v8; V12 g12 = s_v12;
    if (g8.a != 1 || g8.b != 1 || g12.a != 1 || g12.c != 1) return 26;
    c_writes();
    V8 h = t_v8; V12 i = t_v12; V8 j = v_v8; V16 k = v_v16;
    if (h.b != -2 || i.a != -3 || i.c != 5 || j.b != -7 || k.a != -8 || k.b != 9) return 27;
    return 0;
"""
DLL_HEADER = """typedef struct D8 { int a; int b; } D8;
typedef struct D12 { int a, b, c; } D12;
__declspec(dllimport) extern _Atomic D8 d_d8;
__declspec(dllimport) extern _Atomic D12 d_d12;
__declspec(dllimport) int c_reads_ok(void);
__declspec(dllimport) void c_writes(void);
__declspec(dllimport) int lib_calls(void);
"""
DLL_SOURCE = """typedef struct D8 { int a; int b; } D8;
typedef struct D12 { int a, b, c; } D12;
void *memset(void *d, int c, unsigned long long n)
{
    unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d;
}
__declspec(dllexport) _Atomic D8 d_d8 = (D8){1, 2};
__declspec(dllexport) _Atomic D12 d_d12 = (D12){3, 4, 5};
__declspec(dllexport) int c_reads_ok(void) { D8 a = d_d8; D12 b = d_d12; return a.a == 10 && b.c == 50; }
__declspec(dllexport) void c_writes(void) { d_d8 = (D8){1, -2}; d_d12 = (D12){-3, 4, 5}; }
"""
DLL_BODY = """    int k0 = lib_calls();
    D8 a = d_d8;
    if (lib_calls() != k0) return 31;
    D12 b = d_d12;
    if (lib_calls() != k0 + 1) return 32;
    if (a.a != 1 || a.b != 2 || b.a != 3 || b.c != 5) return 33;
    a.a = 10; b.c = 50; d_d8 = a; d_d12 = b;
    if (lib_calls() != k0 + 2) return 34;
    if (c_reads_ok() != 1) return 35;
    c_writes();
    D8 c = d_d8; D12 d = d_d12;
    if (c.a != 1 || c.b != -2 || d.a != -3 || d.c != 5) return 36;
    return 0;
"""
# EN: the atomic library, as the user supplies it (clang_rt.builtins has no `__atomic_*`): one global lock.
# ES: la biblioteca atomica, como la aporta el usuario (clang_rt.builtins no tiene `__atomic_*`): un bloqueo.
LIBRARY = """static _Atomic int lock_word;
static int calls;
static void lock(void) { while (__c11_atomic_exchange(&lock_word, 1, __ATOMIC_ACQUIRE)) {} }
static void unlock(void) { __c11_atomic_store(&lock_word, 0, __ATOMIC_RELEASE); }
static void copy(unsigned char *to, const unsigned char *from, unsigned long long size)
{
    for (unsigned long long i = 0; i < size; ++i) to[i] = from[i];
}
void library_load(unsigned long long size, void *object, void *value, int order) __asm__("__atomic_load");
void library_store(unsigned long long size, void *object, void *value, int order) __asm__("__atomic_store");
void library_load(unsigned long long size, void *object, void *value, int order)
{
    (void)order; lock(); copy(value, object, size); ++calls; unlock();
}
void library_store(unsigned long long size, void *object, void *value, int order)
{
    (void)order; lock(); copy(object, value, size); ++calls; unlock();
}
int lib_calls(void) { lock(); int result = calls; unlock(); return result; }
"""
CRT_REFERENCES = "".join(
    f'<CppLibraryReference Header="crt.h" Language="C" Standard="c11" Library="{path.as_posix()}" />'
    for path in (crt / "libcmt.lib", crt / "libcpmt.lib", crt / "libvcruntime.lib", sdk / "ucrt/x64/libucrt.lib"))
# EN: (header, source, NativeSharp body, needs the CRT TLS support, the C side is a DLL).
# ES: (encabezado, fuente, cuerpo NativeSharp, necesita el soporte TLS del CRT, el lado C es una DLL).
SUITES = {
    "basica": (BASIC_HEADER, BASIC_SOURCE, BASIC_BODY, False, False),
    "variantes": (VARIANT_HEADER, VARIANT_SOURCE, VARIANT_BODY, True, False),
    "dll": (DLL_HEADER, DLL_SOURCE, DLL_BODY, False, True),
}


def c_body(body):
    return body.replace("10000000000L", "10000000000LL").replace("-14000000000L", "-14000000000LL")


def run(args, cwd, check=True, tls=False):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=tls_env if tls else env, text=True,
                            encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def fixture(folder, suite):
    header, source, _, _, dll = SUITES[suite]
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "api.h").write_text(header, encoding="utf-8")
    (folder / "crt.h").write_text("#pragma once\n", encoding="utf-8")
    (folder / "api.c").write_text(source, encoding="utf-8")
    (folder / "atomiclib.c").write_text(LIBRARY, encoding="utf-8")
    for name in ("api", "atomiclib"):
        run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", f"{name}.c", "-o", f"{name}.obj"], folder)
    if dll:
        run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/out:capi.dll", "/implib:capi.lib",
             "/export:__atomic_load", "/export:__atomic_store", "/export:lib_calls", "api.obj", "atomiclib.obj"],
            folder)
    else:
        run([lib, "/nologo", "/out:capi.lib", "api.obj", "atomiclib.obj"], folder)


def reference(suite):
    runtime = ' Runtime="capi.dll"' if SUITES[suite][4] else ""
    return (f'<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="capi.lib"{runtime} />' +
            (CRT_REFERENCES if SUITES[suite][3] else ""))


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
        "</ItemGroup></Project>", encoding="utf-8")


def build(folder, name, opt, tls=False):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", f"{name}.nsproj", "-O" + opt,
                "--no-artifact-cache", "-o", f"out/{name}.exe"], folder, check=False, tls=tls)


def build_and_run(folder, name, opt, tls=False):
    result = build(folder, name, opt, tls)
    if result.returncode:
        if observe:
            print(folder.name, [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:4],
                  flush=True)
            return None
        raise AssertionError(f"{folder.name}: compilación:\n{result.stdout}")
    return run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode


for suite, (_, _, body, tls, dll) in SUITES.items():
    control = root / suite / "control"
    fixture(control, suite)
    (control / "Control.c").write_text(
        '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n' +
        ("" if tls else "int _fltused = 0;\nvoid *memset(void *d, int c, unsigned long long n)\n"
         "{ unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }\n") +
        "static int checks(void) {\n" + c_body(body) + "}\n"
        "void mainCRTStartup(void) { ExitProcess(checks()); }\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], control)
    runtime = [crt / "libcmt.lib", crt / "libvcruntime.lib", sdk / "ucrt/x64/libucrt.lib"] if tls else []
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe"] +
        (["/include:_tls_used"] if tls else []) + ["Control.obj", "capi.lib"] + runtime +
        [sdk / "um/x64/kernel32.lib"], control)
    code = run([control / "Control.exe"], control, check=False).returncode
    if code:
        raise AssertionError(f"control C {suite}: comprobación {code} falló")
    print("control C", suite, "correcto", flush=True)

IR_CASES = {
    "basica": ("R8 v8 = a_r8; v8.a = v8.a + 1; a_r8 = v8; R3 v3 = a_r3; a_r3 = v3; R12 v12 = a_r12; a_r12 = v12; "
               "R24 v24 = a_r24; v24.c = 1; a_r24 = v24; return v8.a + v12.a;",
               [r"load atomic i64, ptr @a_r8 seq_cst, align 8", r"store atomic i64 [^\n]*@a_r8 seq_cst, align 8",
                r"load atomic i32, ptr @a_r3 seq_cst, align 4", r"store atomic i32 [^\n]*@a_r3 seq_cst, align 4",
                r"call void @__atomic_load\(i64 16, ptr @a_r12", r"call void @__atomic_store\(i64 16, ptr @a_r12",
                r"call void @__atomic_load\(i64 24, ptr @a_r24", r"call void @__atomic_store\(i64 24, ptr @a_r24"]),
    "variantes": ("V8 a = v_v8; v_v8 = a; V16 b = v_v16; v_v16 = b; V8 c = t_v8; t_v8 = c; V12 d = t_v12; "
                  "t_v12 = d; V8 e = s_v8; s_v8 = e; V12 f = s_v12; s_v12 = f; return a.a + c.a + e.a;",
                  [r"load atomic volatile i64, ptr @v_v8 seq_cst, align 8",
                   r"store atomic volatile i64 [^\n]*@v_v8 seq_cst, align 8",
                   r"call void @__atomic_load\(i64 16, ptr @v_v16", r"call void @__atomic_store\(i64 16, ptr @v_v16",
                   r"load atomic i64, ptr @t_v8 seq_cst, align 8", r"store atomic i64 [^\n]*@t_v8 seq_cst, align 8",
                   r"call void @__atomic_load\(i64 16, ptr @t_v12", r"call void @__atomic_store\(i64 16, ptr @t_v12",
                   r"load atomic i64, ptr @__nsc_c_inline_[0-9a-f]+ seq_cst, align 8",
                   r"call void @__atomic_load\(i64 16, ptr @__nsc_c_inline_"]),
    "dll": ("D8 a = d_d8; d_d8 = a; D12 b = d_d12; d_d12 = b; return a.a + b.a;",
            [r"load atomic i64, ptr @d_d8 seq_cst, align 8", r"store atomic i64 [^\n]*@d_d8 seq_cst, align 8",
             r"call void @__atomic_load\(i64 16, ptr @d_d12", r"call void @__atomic_store\(i64 16, ptr @d_d12"]),
}
for suite, (statements, expected_ir) in IR_CASES.items():
    folder = root / suite / "ir"
    fixture(folder, suite)
    (folder / "Main.ns").write_text(f"int Touch() {{ {statements} }}\nint Main() {{ return Touch(); }}\n",
                                    encoding="utf-8")
    project(folder, "Ir", ["Main.ns"], reference(suite))
    dump = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm", "Ir.nsproj"], folder,
               check=False)
    (folder / "native.ll").write_text(dump.stdout, encoding="utf-8")
    touch = re.findall(r"define [^\n]*Touch[^\n]*\{(.*?)\n\}", dump.stdout, re.S)
    body = touch[0] if touch else ""
    missing = [pattern for pattern in expected_ir if not re.search(pattern, body)]
    if suite == "variantes" and not re.search(r"@t_v(8|12) = [^\n]*thread_local", dump.stdout):
        missing.append("@t_v8/@t_v12 thread_local")
    if suite == "dll" and not re.search(r"@d_d(8|12) = external dllimport global", dump.stdout):
        missing.append("@d_d8/@d_d12 dllimport")
    plain = [line for line in body.splitlines()
             if re.search(r"(load|store) [^\n]*@(a_r|v_v|t_v|d_d|__nsc_c_inline_)", line) and "atomic" not in line]
    if observe:
        print("IR", suite, missing, (body or dump.stdout)[:3000], flush=True)
    elif missing or plain:
        raise AssertionError(f"IR {suite}: faltan {missing}, accesos no atómicos {plain}:\n" + (body or dump.stdout))
    else:
        print("IR", suite, ": load/store atomic sin bloqueo y __atomic_load/__atomic_store con su tamaño", flush=True)

for opt in opts:
    for suite, (_, _, body, tls, dll) in SUITES.items():
        folder = root / suite / f"directo-O{opt}"
        fixture(folder, suite)
        (folder / "Main.ns").write_text("int Main() {\n" + body + "}\n", encoding="utf-8")
        project(folder, "Direct", ["Main.ns"], reference(suite))
        code = build_and_run(folder, "Direct", opt, tls)
        if code:
            raise AssertionError(f"{suite} directo O{opt}: comprobación {code} falló")
        print(suite, "directo", f"O{opt}", "correcto" if code == 0 else "observado", flush=True)
        if dll:
            continue
        folder = root / suite / f"precompilado-O{opt}"
        fixture(folder / "producer", suite)
        (folder / "producer" / "Producer.ns").write_text(
            "namespace Atomics { public class Bridge { public static int Run<T>(T unused) {\n" + body + "} } }\n",
            encoding="utf-8")
        project(folder / "producer", "Atomics", ["Producer.ns"], reference(suite), library=True)
        result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Atomics.nsproj",
                      "-O" + opt, "--no-artifact-cache", "-o", "Atomics.lib"], folder / "producer", check=False,
                     tls=tls)
        if result.returncode:
            if observe:
                print("productor", suite, [l for l in result.stdout.splitlines() if "error" in l][:3], flush=True)
                continue
            raise AssertionError(f"productor {suite}:\n" + result.stdout)
        (folder / "producer" / "api.h").unlink()
        (folder / "producer" / "crt.h").unlink()
        (folder / "Consumer.ns").write_text("int Main() { return Atomics.Bridge.Run<int>(0); }\n", encoding="utf-8")
        project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Atomics.lib" />')
        code = build_and_run(folder, "Consumer", opt, tls)
        if code:
            raise AssertionError(f"{suite} precompilado O{opt}: comprobación {code} falló")
        print(suite, "precompilado", f"O{opt}", "correcto" if code == 0 else "observado", flush=True)

MEMBER = "of C atomic record storage; copy the whole value into a local first"
REJECTIONS = [
    ("basica", "miembro-lectura-8", "return a_r8.a;", MEMBER),
    ("basica", "miembro-lectura-24", "return (int)a_r24.b;", MEMBER),
    ("basica", "miembro-escritura", "a_r12.a = 1; return 0;", MEMBER),
    ("basica", "miembro-union", "a_u4.i = 1; return 0;", MEMBER),
    ("basica", "direccion", "var p = &a_r16; return 0;", "cannot take the address of C atomic storage"),
    ("basica", "direccion-miembro", "var p = &a_r8.b; return 0;", MEMBER),
    ("variantes", "miembro-tls", "return t_v12.a;", MEMBER),
    ("variantes", "miembro-volatile", "v_v8.b = 2; return 0;", MEMBER),
    ("variantes", "miembro-static", "return s_v8.a;", MEMBER),
    ("variantes", "direccion-tls", "var p = &t_v8; return 0;", "cannot take the address of C atomic storage"),
    ("variantes", "direccion-volatile", "var p = &v_v16; return 0;", "cannot take the address of"),
    ("dll", "miembro-dll", "return d_d12.b;", MEMBER),
    ("dll", "direccion-dll", "var p = &d_d8; return 0;", "cannot take the address of C atomic storage"),
]
for suite, name, statements, expected in REJECTIONS:
    folder = root / "rechazos" / name
    fixture(folder, suite)
    (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
    project(folder, "Negative", ["Main.ns"], reference(suite))
    result = build(folder, "Negative", "0", SUITES[suite][3])
    if observe:
        print(name, result.returncode, [l.split(": error: ")[-1] for l in result.stdout.splitlines()
                                         if "error" in l][:2], flush=True)
        continue
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)
print("observación terminada" if observe else "Globales C atómicas de tipo record verificadas:", root)
