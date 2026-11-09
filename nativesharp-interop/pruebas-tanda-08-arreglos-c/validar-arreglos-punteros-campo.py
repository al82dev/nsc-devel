"""T08-D23 (T08-016-03d1): arrays of qualified pointers as C record fields.

03d1) A C struct field may be an array of `const` pointers (`int *const cps[2]`), of pointers to `const`,
      `volatile` or `const volatile` data, with scalar, enum, record and `char` pointees, as a matrix, and inside a
      `pack(1)` struct. As for C globals (T08-016-03d):
      - writing through a `const` pointer element is accepted; reassigning the element is rejected;
      - reassigning an element that points to `const` data is accepted; writing through it is rejected;
      - an element that points to `volatile` data keeps the qualifier with `var` (in the Microsoft mode the store
        is an atomic `volatile` release in the IR, as in Clang) and dropping it is rejected.
04a) Sizes compared with Clang; the element offsets are checked by 04b: C fills and NativeSharp reads, then
     NativeSharp writes and C checks. As for globals, `&` of an element is rejected: a `const` pointer would be a
     `T* const*`, and a `const` or `volatile` pointee needs the element-address rule of T08-016-03d;
04d) the same body through a template of a precompiled producer consumed without the header.
The body also runs in a Clang control. Positive cases run in O0-O3; rejections in O0.
T08-D23: arreglos de punteros calificados como campo de record C.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-arreglos-punteros-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef struct Pt { int x, y; } Pt;
typedef enum Mode { MA = 1, MB = 2 } Mode;
typedef struct QP {
    char tag; int *const cps[2]; const int *pcs[2]; volatile int *pvs[2]; const volatile int *pcvs[2];
    const Pt *rps[2]; const Mode *eps[2]; const char *names[3]; const int *grid[2][2];
} QP;
#pragma pack(push, 1)
typedef struct PQ { char c; const int *pcs[2]; volatile int *pvs[1]; } PQ;
#pragma pack(pop)
int layout(int which);
void fill_qp(QP *q);
int check_qp(const QP *q);
void fill_pq(PQ *p);
int check_pq(const PQ *p);
int pointee(int which);
"""
LAYOUT = ["sizeof(QP)", "sizeof(PQ)"]
SOURCE = """#include <stddef.h>
#include "api.h"
static int pointees[4] = {61, 62, 63, 64};
static Pt pts[2] = {{1, 2}, {3, 4}};
static Mode modes[2] = {MA, MB};
static const QP initial = {
    0, {&pointees[0], &pointees[1]}, {&pointees[2], &pointees[3]}, {&pointees[0], &pointees[1]},
    {&pointees[2], &pointees[3]}, {&pts[0], &pts[1]}, {&modes[0], &modes[1]}, {"ab", "cd", "ef"},
    {{&pointees[0], &pointees[1]}, {&pointees[2], &pointees[3]}}};
int layout(int which)
{
    switch (which) {
""" + "".join(f"    case {k}: return (int){e};\n" for k, e in enumerate(LAYOUT)) + """    }
    return -1;
}
void fill_qp(QP *q)
{
    const unsigned char *from = (const unsigned char *)&initial;
    unsigned char *to = (unsigned char *)q;
    for (size_t i = 0; i < sizeof initial; ++i) to[i] = from[i];
    pointees[0] = 61;
    pointees[1] = 62;
}
int check_qp(const QP *q)
{
    return *q->pcs[0] == 73 && q->pcs[0] == &pointees[1] && pointees[0] == 99 &&
        q->rps[0] == &pts[1] && q->grid[1][0] == &pointees[0] && q->names[0] == q->names[2];
}
void fill_pq(PQ *p) { p->c = 1; p->pcs[0] = &pointees[2]; p->pcs[1] = &pointees[3]; p->pvs[0] = &pointees[1]; }
int check_pq(const PQ *p) { return p->pcs[1] == &pointees[2] && pointees[1] == 55; }
int pointee(int which) { return pointees[which]; }
"""
BODY = """    QP q;
    PQ p;
    // 04a: sizes compared with Clang; every element offset is checked by the cross reads and writes below.
    if ((int)sizeof(QP) != layout(0) || (int)sizeof(PQ) != layout(1)) return 1;
    // 04b: C fills, NativeSharp reads.
    fill_qp(&q);
    if (*q.cps[0] != 61 || *q.pcs[1] != 64 || *q.pvs[1] != 62 || *q.pcvs[0] != 63) return 5;
    if (q.rps[1]->y != 4 || *q.eps[1] != MB || q.names[1][0] != 99 || *q.grid[1][1] != 64) return 6;
    // 03d1: write through a const pointer, reassign pointers to const, keep volatile with var.
    *q.cps[1] = 72;
    q.pcs[0] = q.cps[1];
    *q.pvs[0] = 99;
    var vslot = q.pvs[1];
    q.rps[0] = q.rps[1];
    q.grid[1][0] = q.cps[0];
    q.names[0] = q.names[2];
    if (*q.pcs[0] != 72 || pointee(0) != 99 || *vslot != 72) return 7;
    *vslot = 73;
    // NativeSharp writes, C checks.
    if (check_qp(&q) != 1) return 8;
    // 03d1 in a packed struct.
    fill_pq(&p);
    if (*p.pcs[1] != 64 || *p.pvs[0] != 73) return 9;
    p.pcs[1] = p.pcs[0];
    *p.pvs[0] = 55;
    return check_pq(&p) == 1 ? 0 : 10;
"""
# EN: the reference keeps the target's default (Microsoft) volatile mode: as with Clang -fms-volatile, each store is an
#     atomic release, directly through the element and through the `var` alias (T08-D13).
# ES: la referencia conserva el modo volatile predeterminado del destino (Microsoft): como Clang -fms-volatile, cada
#     store es atomico release, directo por el elemento y por el alias `var` (T08-D13).
IR_CHECKS = ["store atomic volatile i32 99", "store atomic volatile i32 73", "store atomic volatile i32 55"]
REJECTIONS = [
    ("03d1-escribir-puntero-const", "QP q; fill_qp(&q); int x = 1; q.cps[0] = &x; return 0;", "not assignable"),
    ("03d1-escribir-apuntado-const", "QP q; fill_qp(&q); *q.pcs[0] = 2; return 0;", "not assignable"),
    ("03d1-escribir-record-const", "QP q; fill_qp(&q); q.rps[0]->x = 5; return 0;", "not assignable"),
    ("03d1-quitar-const", "QP q; fill_qp(&q); int* r = q.pcs[0]; return *r;", "would discard pointee constness"),
    ("03d1-quitar-volatile", "QP q; fill_qp(&q); int* r = q.pvs[0]; return *r;",
     "would discard pointer pointee volatile qualification"),
    ("03d1-escribir-const-volatile", "QP q; fill_qp(&q); *q.pcvs[0] = 2; return 0;", "not assignable"),
    ("03d1-empaquetado-const", "PQ p; fill_pq(&p); *p.pcs[0] = 2; return 0;", "not assignable"),
    ("03d1-direccion-puntero-const", "QP q; fill_qp(&q); var r = &q.cps[0]; return 0;",
     "cannot take the address of a readonly pointer"),
    ("03d1-direccion-apuntado-const", "QP q; fill_qp(&q); var r = &q.pcs[0]; return 0;",
     "cannot take the address of an array element with a const pointee"),
    ("03d1-direccion-apuntado-volatile", "QP q; fill_qp(&q); var r = &q.pvs[0]; return 0;",
     "cannot take the address of an array element with a volatile pointee"),
]


def c_spelling(text):
    return text.replace("(byte*)", "(unsigned char*)").replace("var vslot", "volatile int* vslot")


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
    run([lib, "/nologo", "/out:carr.lib", "api.obj"], folder)


REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="carr.lib" />'


def project(folder, name, source, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + refs + "</ItemGroup></Project>",
        encoding="utf-8")


def build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


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


def control():
    folder = root / "control"
    folder.mkdir()
    c_library(folder)
    (folder / "control.c").write_text(
        '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\nint _fltused = 0;\n'
        f"static int run(void)\n{{\n{c_spelling(BODY)}}}\n"
        "void mainCRTStartup(void) { ExitProcess(run()); }\n", encoding="utf-8")
    result = run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"],
                 folder, check=False)
    if result.returncode:
        raise AssertionError(f"control Clang:\n{result.stdout}")
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:control.exe",
         "control.obj", "carr.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control Clang: paso {code}")
    print("control Clang correcto", flush=True)


def paths(opt):
    direct = root / f"directo-O{opt}"
    direct.mkdir()
    c_library(direct)
    (direct / "Main.ns").write_text("int Main()\n{\n" + BODY + "}\n", encoding="utf-8")
    project(direct, "Direct", "Main.ns", REF)
    expect_run(f"directo O{opt}", direct, "Direct", opt)
    base = root / f"precompilado-O{opt}"
    producer = base / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "P.ns").write_text("namespace QualifiedArrays { public class Probe {\n"
                                   "public static int Run<T>(T unused)\n{\n" + BODY + "} } }\n", encoding="utf-8")
    project(producer, "QpPub", "P.ns", REF, library=True)
    result = build(producer, "QpPub", opt, "lib", "QpPub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (producer / "api.h").unlink()
    (base / "C.ns").write_text("int Main() { return QualifiedArrays.Probe.Run<int>(0); }\n", encoding="utf-8")
    project(base, "C", "C.ns", '<LibraryReference Include="producer/QpPub.lib" />')
    expect_run(f"precompilado sin encabezado O{opt}", base, "C", opt)


def ir_check():
    """EN: the stores through `volatile` pointee elements are volatile. ES: los stores son volatile."""
    folder = root / "ir"
    folder.mkdir()
    c_library(folder)
    (folder / "Main.ns").write_text("int Main()\n{\n" + BODY + "}\n", encoding="utf-8")
    project(folder, "Ir", "Main.ns", REF)
    dump = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm", "Ir.nsproj"], folder,
               check=False)
    (folder / "native.ll").write_text(dump.stdout, encoding="utf-8")
    missing = [check for check in IR_CHECKS if check not in dump.stdout]
    if observe:
        print("IR", dump.returncode, missing, flush=True)
    elif dump.returncode or missing:
        raise AssertionError(f"IR: faltan {missing} (codigo {dump.returncode})")
    else:
        print("IR volatile correcto", flush=True)


def rejections():
    for name, body, text in REJECTIONS:
        folder = root / "rechazos" / name
        folder.mkdir(parents=True)
        c_library(folder)
        (folder / "Main.ns").write_text("int Main() { " + body + " }\n", encoding="utf-8")
        project(folder, "N", "Main.ns", REF)
        result = build(folder, "N", "0", "exe", "out/N.exe")
        if observe:
            print("rechazo", name, result.returncode, errors(result), flush=True)
        elif result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
            raise AssertionError(f"rechazo {name}: se esperaba «{text}»:\n{result.stdout}")
        else:
            print("rechazo", name, "correcto", flush=True)


control()
for opt in opts:
    paths(opt)
ir_check()
rejections()
print("observación terminada" if observe else "Arreglos de punteros calificados en campos verificados:", root)
