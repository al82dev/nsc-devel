"""T08-015-06b/c and T08-015-07: C qualifiers and the native volatile contract.

06b) A native NativeSharp `volatile` field keeps its contract (volatile accesses without atomic ordering) in a
     project without C headers.
06c) The same native field keeps the same IR in a project that imports a C header in Microsoft mode.
07a) Writing through a `const T*` imported from C is rejected.
07b) A `volatile T*` from C is read, written and indexed; the IR keeps every access volatile, as Clang does. The
     ordering through the NativeSharp alias (`var`) depends on the pending decision T08-010-05a.
07c) `const volatile T*` in fields, globals and parameters: read, passed to C, and writing is rejected.
07d) Qualifiers of the pointee and of the pointer in `T* const` and `T**` fields.
Positive cases run in O0-O3, directly and through a precompiled template consumed without the header;
rejections run in O0.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-calificadores-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
observe = "--observar" in sys.argv
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

HEADER = """typedef struct Q {
    const volatile int cv; volatile int v; const int c; int *const pc; const int *pci;
    volatile int *pvi; const volatile int *pcvi; int **pp;
} Q;
extern const volatile int g_cv;
extern volatile int g_v;
extern const volatile int *g_pcv;
volatile int *vptr(int *p);
const int *cptr(const int *p);
int read_cv(const volatile int *p);
void write_v(volatile int *p, int v);
Q *make_q(int *t);
int check_slots(const int *slots);
"""
SOURCE = """#include "api.h"
const volatile int g_cv = 21;
volatile int g_v = 22;
static int backing = 23;
const volatile int *g_pcv = &backing;
static int *pointer;
static Q q = {31, 32, 33, 0, 0, 0, 0, 0};
volatile int *vptr(int *p) { return p; }
const int *cptr(const int *p) { return p; }
int read_cv(const volatile int *p) { return *p; }
void write_v(volatile int *p, int v) { *p = v; }
Q *make_q(int *t)
{
    pointer = t;
    *(int **)&q.pc = t;
    q.pci = t; q.pvi = t; q.pcvi = t; q.pp = &pointer;
    return &q;
}
int check_slots(const int *slots) { return slots[0] == 5 && slots[1] == 6; }
"""
IR_C = """#include "api.h"
int readv(int *raw) { volatile int *v = vptr(raw); return *v + v[1]; }
void writev(int *raw) { volatile int *v = vptr(raw); *v = 5; v[1] = 6; }
"""
IR_NS = """namespace Ir { public class V {
    public static int ReadV(int* raw) { var v = vptr(raw); return *v + v[1]; }
    public static void WriteV(int* raw) { var v = vptr(raw); *v = 5; v[1] = 6; }
} }
"""
NATIVE = """namespace Native { public class Counter {
    public volatile int count;
    public static int Bump(Counter* c) { c->count = c->count + 1; return c->count; }
} }
int Main() { return 0; }
"""
BODY = """    Q* q = make_q(null);
    int slots0 = 0;
    int target = 4;
    q = make_q(&target);
    // 07b: volatile T* from C, read, written and indexed.
    Pair2 p2;
    p2.a = 0;
    p2.b = 0;
    var v = vptr(&p2.a);
    *v = 5;
    v[1] = 6;
    if (check_slots(&p2.a) != 1 || *v + v[1] != 11) return 1;
    write_v(v, 7);
    if (p2.a != 7) return 2;
    // 07c: const volatile in fields, globals and parameters.
    if (q->cv != 31 || q->v != 32 || q->c != 33 || g_cv != 21 || g_v != 22 || *g_pcv != 23) return 3;
    if (*q->pcvi != 4 || read_cv(q->pcvi) != 4 || read_cv(&target) != 4 || read_cv(v) != 7) return 4;
    q->v = 40;
    g_v = 41;
    if (q->v != 40 || g_v != 41) return 5;
    // 07d: T* const and T** fields.
    *q->pc = 9;
    if (target != 9 || *q->pci != 9 || **q->pp != 9 || *q->pvi != 9) return 6;
    **q->pp = 10;
    return target == 10 && *cptr(&target) == 10 ? 0 : 7;
"""
HEADER = HEADER.replace("typedef struct Q {", "typedef struct Pair2 { int a; int b; } Pair2;\ntypedef struct Q {")
REJECTIONS = [
    ("07a-escribir-const-importado", "int x = 1; var c = cptr(&x); *c = 2; return 0;", "not assignable"),
    ("07c-escribir-campo-const-volatile", "int x = 1; Q* q = make_q(&x); q->cv = 2; return 0;", "not assignable"),
    ("07c-escribir-apuntado-const-volatile", "int x = 1; Q* q = make_q(&x); *q->pcvi = 2; return 0;",
     "not assignable"),
    ("07c-escribir-global-const-volatile", "g_cv = 3; return 0;", "not assignable"),
    ("07c-quitar-const-volatile", "int x = 1; Q* q = make_q(&x); int* r = q->pcvi; return *r;", "discard"),
    ("07d-reasignar-puntero-const", "int x = 1; Q* q = make_q(&x); q->pc = &x; return 0;", "not assignable"),
]


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder):
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:cqual.lib", "api.obj"], folder)


def ref(mode=None, library=True):
    extra = f' VolatileMode="{mode}"' if mode else ""
    return f'<CppLibraryReference Header="api.h" Language="C" Standard="c11"{extra}' + \
        (' Library="cqual.lib"' if library else "") + " />"


def project(folder, name, source, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + refs + "</ItemGroup></Project>",
        encoding="utf-8")


def build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def dump(folder, name):
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm-opt", f"{name}.nsproj", "-O1"],
                 folder, check=False)
    if result.returncode:
        raise AssertionError(f"IR {name}:\n{result.stdout}")
    return result.stdout


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


def expect_run(label, folder, name, opt):
    result = build(folder, name, opt, "exe", f"out/{name}.exe")
    code = run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode \
        if result.returncode == 0 else None
    if observe:
        print(label, result.returncode, code, errors(result), flush=True)
        return
    if result.returncode or code:
        raise AssertionError(f"{label}: build {result.returncode}, salida {code}:\n{result.stdout}")
    print(label, "correcto", flush=True)


def volatile_lines(text, token=None):
    lines = []
    inside = token is None
    for line in text.splitlines():
        if token and line.startswith("define "):
            inside = token in line
        if inside and "volatile" in line and ("load" in line or "store" in line):
            ordering = "acquire" if " acquire" in line else "release" if " release" in line else "none"
            lines.append(("load" if "load" in line else "store", ordering))
    return lines


def expect(label, condition, detail):
    if observe:
        print(label, detail, flush=True)
    elif not condition:
        raise AssertionError(f"{label}: {detail}")
    else:
        print(label, "correcto", flush=True)


def ir_checks():
    folder = root / "ir"
    folder.mkdir()
    c_library(folder)
    (folder / "c.c").write_text(IR_C, encoding="utf-8")
    run([clang, "-S", "-emit-llvm", "-O1", "--target=x86_64-pc-windows-msvc", "-std=c11", "c.c", "-o", "c.ll"],
        folder)
    clang_lines = volatile_lines((folder / "c.ll").read_text(encoding="utf-8"))
    (folder / "M.ns").write_text(IR_NS + "int Main() { return 0; }\n", encoding="utf-8")
    project(folder, "M", "M.ns", ref())
    text = dump(folder, "M")
    nsc_lines = volatile_lines(text, "ReadV") + volatile_lines(text, "WriteV")
    # The accesses stay volatile and match Clang in kind and number. Their ordering through a NativeSharp alias of
    # a pointer returned by C follows the pending decision T08-010-05a (Clang -fms-volatile: acquire/release).
    expect("07b IR volatile T* frente a Clang", sorted(k for k, _ in nsc_lines) == sorted(k for k, _ in clang_lines),
           f"clang={clang_lines} nsc={nsc_lines}")
    native = root / "nativo"
    native.mkdir()
    (native / "N.ns").write_text(NATIVE, encoding="utf-8")
    project(native, "N", "N.ns", "")
    plain = volatile_lines(dump(native, "N"), "Bump")
    expect("06b volatile nativo sin encabezado", plain and all(o == "none" for _, o in plain), f"{plain}")
    mixed = root / "nativo-con-c"
    mixed.mkdir()
    c_library(mixed)
    (mixed / "N.ns").write_text(NATIVE, encoding="utf-8")
    project(mixed, "N", "N.ns", ref("Microsoft"))
    with_header = volatile_lines(dump(mixed, "N"), "Bump")
    expect("06c volatile nativo con encabezado Microsoft", with_header == plain, f"{with_header}")


def paths(opt):
    direct = root / f"directo-O{opt}"
    direct.mkdir()
    c_library(direct)
    (direct / "Main.ns").write_text("int Main()\n{\n" + BODY + "}\n", encoding="utf-8")
    project(direct, "Direct", "Main.ns", ref())
    expect_run(f"directo O{opt}", direct, "Direct", opt)
    base = root / f"precompilado-O{opt}"
    producer = base / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "P.ns").write_text("namespace Qual { public class Probe { public static int Run<T>(T unused)\n{\n" +
                                   BODY + "} } }\n", encoding="utf-8")
    project(producer, "QualPub", "P.ns", ref(), library=True)
    result = build(producer, "QualPub", opt, "lib", "QualPub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (producer / "api.h").unlink()
    (base / "C.ns").write_text("int Main() { return Qual.Probe.Run<int>(0); }\n", encoding="utf-8")
    project(base, "C", "C.ns", '<LibraryReference Include="producer/QualPub.lib" />')
    expect_run(f"precompilado sin encabezado O{opt}", base, "C", opt)


def rejections():
    for name, body, text in REJECTIONS:
        folder = root / "rechazos" / name
        folder.mkdir(parents=True)
        c_library(folder)
        (folder / "Main.ns").write_text("int Main() { " + body + " }\n", encoding="utf-8")
        project(folder, "N", "Main.ns", ref())
        result = build(folder, "N", "0", "exe", "out/N.exe")
        if observe:
            print("rechazo", name, result.returncode, errors(result), flush=True)
        elif result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
            raise AssertionError(f"rechazo {name}: se esperaba «{text}»:\n{result.stdout}")
        else:
            print("rechazo", name, "correcto", flush=True)


ir_checks()
for opt in opts:
    paths(opt)
rejections()
print("observación terminada" if observe else "Calificadores C verificados:", root)
