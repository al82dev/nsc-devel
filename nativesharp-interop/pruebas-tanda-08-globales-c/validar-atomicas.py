"""T08-004-04d: `_Atomic` C globals, external and `static`, with crossed reads and writes.

External atomics of every supported kind (`int`, `long long`, `unsigned`, `unsigned long long`, `short`,
`unsigned char`, `_Bool`, `float`, `double`, `int *` and an enum) and `static` atomics defined in the header
(each project gets its own copy, read and written through `static inline` C accessors too). NativeSharp
writes and C reads with `__c11_atomic_load`; C writes with `__c11_atomic_store`/`fetch_add` and NativeSharp
reads. The IR uses atomic loads and stores (seq_cst) for every access. Checked against a Clang control,
directly, and through a precompiled template consumed without the header, O0-O3. Rejections (O0): taking the
address of an atomic global, and an atomic `struct` global (not supported yet).
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-globales-atomicas-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef enum Level { Low = 1, High = 9 } Level;
extern _Atomic int a_int;
extern _Atomic long long a_ll;
extern _Atomic unsigned a_u;
extern _Atomic unsigned long long a_ull;
extern _Atomic short a_s;
extern _Atomic unsigned char a_uc;
extern _Atomic _Bool a_b;
extern _Atomic float a_f;
extern _Atomic double a_d;
extern _Atomic(int *) a_p;
extern _Atomic Level a_level;
extern int target;
static _Atomic int s_counter = 5;
static inline int s_counter_read(void) { return __c11_atomic_load(&s_counter, __ATOMIC_SEQ_CST); }
static inline void s_counter_add(int v) { __c11_atomic_fetch_add(&s_counter, v, __ATOMIC_SEQ_CST); }
int c_reads_ok(void);
void c_writes(void);
"""
SOURCE = """#include "api.h"
_Atomic int a_int = 1;
_Atomic long long a_ll = -2;
_Atomic unsigned a_u = 3u;
_Atomic unsigned long long a_ull = 4u;
_Atomic short a_s = -5;
_Atomic unsigned char a_uc = 6;
_Atomic _Bool a_b = 0;
_Atomic float a_f = 7.5f;
_Atomic double a_d = 8.25;
_Atomic(int *) a_p = 0;
_Atomic Level a_level = Low;
int target = 77;
#define LOAD(x) __c11_atomic_load(&x, __ATOMIC_SEQ_CST)
int c_reads_ok(void)
{
    return LOAD(a_int) == 10 && LOAD(a_ll) == -20000000000LL && LOAD(a_u) == 4000000000u &&
        LOAD(a_ull) == 40u && LOAD(a_s) == -50 && LOAD(a_uc) == 200 && LOAD(a_b) == 1 && LOAD(a_f) == 0.5f &&
        LOAD(a_d) == -0.25 && LOAD(a_p) == &target && LOAD(a_level) == High;
}
void c_writes(void)
{
    __c11_atomic_store(&a_int, 100, __ATOMIC_SEQ_CST);
    __c11_atomic_fetch_add(&a_ll, 5, __ATOMIC_SEQ_CST);
    __c11_atomic_store(&a_f, 1.25f, __ATOMIC_SEQ_CST);
    __c11_atomic_store(&a_p, 0, __ATOMIC_SEQ_CST);
    __c11_atomic_store(&a_level, Low, __ATOMIC_SEQ_CST);
}
"""
NS_BODY = """    if (a_int != 1 || a_ll != -2 || a_u != 3 || a_ull != 4 || a_s != -5 || a_uc != 6) return 1;
    if (a_b || a_f != 7.5f || a_d != 8.25 || a_p != null || a_level != Low) return 2;
    a_int = 10; a_ll = -20000000000L; a_u = 4000000000u; a_ull = 40; a_s = -50; a_uc = 200; a_b = true;
    a_f = 0.5f; a_d = -0.25; a_p = &target; a_level = High;
    if (c_reads_ok() != 1) return 3;
    c_writes();
    if (a_int != 100 || a_ll != -19999999995L || a_f != 1.25f || a_p != null || a_level != Low) return 4;
    if (s_counter != 5 || s_counter_read() != 5) return 5;
    s_counter = 6;
    s_counter_add(4);
    if (s_counter != 10 || s_counter_read() != 10) return 6;
    return 0;
"""
C_BODY = (NS_BODY.replace("a_p != null", "a_p != 0").replace("a_b ||", "a_b ||").replace("true", "1")
          .replace("-20000000000L", "-20000000000LL").replace("-19999999995L", "-19999999995LL"))


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def fixture(folder):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:capi.lib", "api.obj"], folder)


REFERENCE = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="capi.lib" />'


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


control = root / "control"
fixture(control)
(control / "Control.c").write_text('#include "api.h"\n'
                                   "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
                                   "int _fltused = 0;\nstatic int checks(void) {\n" + C_BODY + "}\n"
                                   "void mainCRTStartup(void) { ExitProcess(checks()); }\n", encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], control)
run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
     "Control.obj", "capi.lib", sdk / "um/x64/kernel32.lib"], control)
code = run([control / "Control.exe"], control, check=False).returncode
if code:
    raise AssertionError(f"control C: comprobación {code} falló")
print("control C correcto", flush=True)

ir = root / "ir"
fixture(ir)
(ir / "Main.ns").write_text("int Touch() { a_int = a_int + 1; a_d = a_d * 2.0; s_counter = s_counter + 1; "
                            "return a_int; }\nint Main() { return Touch() == 2 ? 0 : 1; }\n", encoding="utf-8")
project(ir, "Ir", ["Main.ns"], REFERENCE)
dump = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm", "Ir.nsproj"], ir, check=False)
(ir / "native.ll").write_text(dump.stdout, encoding="utf-8")
touch = re.findall(r"define [^\n]*Touch[^\n]*\{(.*?)\n\}", dump.stdout, re.S)
plain = [line for line in (touch[0].splitlines() if touch else []) if re.search(r"= load |^\s*store ", line) and
         "atomic" not in line]
if observe:
    print("IR", (touch[0] if touch else dump.stdout)[:1500], flush=True)
elif not touch or plain or len(re.findall(r"load atomic", touch[0])) < 3 or \
        len(re.findall(r"store atomic", touch[0])) < 3:
    raise AssertionError("IR: se esperaban solo accesos atómicos:\n" + (touch[0] if touch else dump.stdout))
else:
    print("IR: todas las lecturas y escrituras de las globales atómicas son atómicas", flush=True)

for opt in opts:
    folder = root / f"directo-O{opt}"
    fixture(folder)
    (folder / "Main.ns").write_text("int Main() {\n" + NS_BODY + "}\n", encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], REFERENCE)
    code = build_and_run(folder, "Direct", opt)
    if code:
        raise AssertionError(f"directo O{opt}: comprobación {code} falló")
    print("directo", f"O{opt}", "correcto" if code == 0 else "observado", flush=True)
    folder = root / f"precompilado-O{opt}"
    fixture(folder / "producer")
    (folder / "producer" / "Producer.ns").write_text(
        "namespace Atomics { public class Bridge { public static int Run<T>(T unused) {\n" + NS_BODY + "} } }\n",
        encoding="utf-8")
    project(folder / "producer", "Atomics", ["Producer.ns"], REFERENCE, library=True)
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Atomics.nsproj", "-O" + opt,
                  "--no-artifact-cache", "-o", "Atomics.lib"], folder / "producer", check=False)
    if result.returncode:
        if observe:
            print("productor", [l for l in result.stdout.splitlines() if "error" in l][:3], flush=True)
            continue
        raise AssertionError("productor:\n" + result.stdout)
    (folder / "producer" / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Atomics.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Atomics.lib" />')
    code = build_and_run(folder, "Consumer", opt)
    if code:
        raise AssertionError(f"precompilado O{opt}: comprobación {code} falló")
    print("precompilado", f"O{opt}", "correcto" if code == 0 else "observado", flush=True)

REJECTIONS = [
    ("direccion", "var p = &a_int; return 0;", "cannot take the address of C atomic storage"),
    ("record-atomico", "return a_pair.a;", "cannot import C/C++ global 'a_pair': C/C++ global type is not yet mapped"),
]
for name, statements, expected in REJECTIONS:
    folder = root / "rechazos" / name
    fixture(folder)
    (folder / "extra.h").write_text("typedef struct Pair { int a; } Pair;\nextern _Atomic Pair a_pair;\n",
                                    encoding="utf-8")
    (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
    project(folder, "Negative", ["Main.ns"], REFERENCE +
            '<CppLibraryReference Header="extra.h" Language="C" Standard="c11" />')
    result = build(folder, "Negative", "0")
    if observe:
        print(name, result.returncode, [l.split(": error: ")[-1] for l in result.stdout.splitlines()
                                         if "error" in l][:2], flush=True)
        continue
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)
print("observación terminada" if observe else "Globales C atómicas verificadas:", root)
