"""T08-019-03 and T08-019-04: diagnostics for invalid operations on incomplete C types.

03a) `sizeof` and `alignof` of an incomplete struct and union.
03b) A NativeSharp field and a local of incomplete type by value.
03c) Member access through a pointer to an incomplete struct and union.
03d) Arithmetic, increment, decrement, compound assignment and indexing of a pointer to an incomplete type.
03e) A NativeSharp parameter or return of incomplete type by value, and a C function declared with one.
04) Every rejection repeated in a consumer that sees the types only through the metadata of a precompiled producer;
    the diagnostic must name the type and the invalid operation.
Rejections run in O0 in both paths; a positive control proves the opaque pointers work in each path.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-incompletos-diag-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef struct Opaque Opaque;
typedef union OpU OpU;
Opaque *op_new(int v);
int op_get(const Opaque *o);
OpU *u_new(int v);
int u_get(const OpU *u);
int take_opaque(Opaque o);
"""
SOURCE = """#include "api.h"
struct Opaque { int v; };
union OpU { int i; float f; };
static struct Opaque pool[4];
static union OpU upool[4];
static int used;
Opaque *op_new(int v) { pool[used].v = v; return &pool[used++]; }
int op_get(const Opaque *o) { return o->v; }
OpU *u_new(int v) { upool[used].i = v; return &upool[used++]; }
int u_get(const OpU *u) { return u->i; }
"""
POSITIVE = "Opaque* a = op_new(4); OpU* u = u_new(5); return op_get(a) == 4 && u_get(u) == 5 ? 0 : 1;"
# (case, declarations, body, expected texts)
REJECTIONS = [
    ("03a-sizeof-struct", "", "return (int)sizeof(Opaque);", ["Opaque", "is incomplete and has no object layout"]),
    ("03a-sizeof-union", "", "return (int)sizeof(OpU);", ["OpU", "is incomplete and has no object layout"]),
    ("03a-alignof-struct", "", "return (int)alignof(Opaque);", ["Opaque", "is incomplete"]),
    ("03a-alignof-union", "", "return (int)alignof(OpU);", ["OpU", "is incomplete"]),
    ("03b-local-struct", "", "Opaque o; return 0;", ["'o'", "incomplete"]),
    ("03b-local-union", "", "OpU o; return 0;", ["'o'", "incomplete"]),
    ("03b-campo", "public struct W { public Opaque o; }\n", "W w; return 0;", ["Opaque", "incomplete"]),
    ("03c-miembro-struct", "", "Opaque* a = op_new(1); return a->v;", ["'v'", "Opaque"]),
    ("03c-miembro-union", "", "OpU* u = u_new(1); return u->i;", ["'i'", "OpU"]),
    ("03d-suma", "", "Opaque* a = op_new(1); Opaque* b = a + 1; return 0;",
     ["pointer arithmetic requires a complete C type", "Opaque"]),
    ("03d-indice", "", "Opaque* a = op_new(1); return op_get(&a[1]);",
     ["pointer arithmetic requires a complete C type", "Opaque"]),
    ("03d-incremento", "", "Opaque* a = op_new(1); a++; return 0;", ["complete C type", "Opaque"]),
    ("03d-decremento", "", "OpU* u = u_new(1); --u; return 0;", ["complete C type", "OpU"]),
    ("03d-compuesta", "", "Opaque* a = op_new(1); a += 2; return 0;", ["complete C type", "Opaque"]),
    ("03e-parametro", "int Take(Opaque o) { return 0; }\n", "return 0;",
     ["parameter 'o' of 'Take' cannot be passed by value", "'Opaque' is incomplete"]),
    ("03e-parametro-union", "public class K { public static int Take(OpU u) { return 0; } }\n", "return 0;",
     ["parameter 'u' of 'Take' cannot be passed by value", "'OpU' is incomplete"]),
    ("03e-retorno", "Opaque Give() { return *op_new(1); }\n", "return 0;",
     ["'Give' cannot return C/C++ type 'Opaque' by value"]),
    ("03e-retorno-metodo", "public class K { public static OpU Give() { return *u_new(1); } }\n", "return 0;",
     ["'Give' cannot return C/C++ type 'OpU' by value"]),
    ("03b-var-desreferencia", "", "Opaque* a = op_new(1); var x = *a; return 0;", ["'x'", "incomplete"]),
    ("03e-funcion-c", "", "Opaque* a = op_new(1); return take_opaque(*a);", ["take_opaque"]),
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
    run([lib, "/nologo", "/out:cop.lib", "api.obj"], folder)


def project(folder, name, source, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + refs + "</ItemGroup></Project>",
        encoding="utf-8")


def build(folder, name, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O0", "--no-artifact-cache", "-o", output], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


DIRECT_REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="cop.lib" />'


def producer():
    # The producer publishes both opaque types through public signatures and is consumed without the header.
    folder = root / "producer"
    folder.mkdir(parents=True)
    c_library(folder)
    (folder / "P.ns").write_text(
        "namespace Pub { public class Op {\n"
        "    public static Opaque* Make(int v) { return op_new(v); }\n"
        "    public static int Get(Opaque* o) { return op_get(o); }\n"
        "    public static OpU* MakeU(int v) { return u_new(v); }\n"
        "    public static int GetU(OpU* u) { return u_get(u); }\n} }\n", encoding="utf-8")
    project(folder, "Pub", "P.ns", DIRECT_REF, library=True)
    result = build(folder, "Pub", "lib", "Pub.lib")
    if result.returncode:
        raise AssertionError(f"productor:\n{result.stdout}")
    (folder / "api.h").unlink()


def metadata_body(body):
    # The consumer has no header: C functions are reached through the producer's published methods.
    return (body.replace("op_new(", "Pub.Op.Make(").replace("u_new(", "Pub.Op.MakeU(")
            .replace("op_get(", "Pub.Op.Get(").replace("u_get(", "Pub.Op.GetU("))


def case(path, name, declarations, body):
    folder = root / path / name
    folder.mkdir(parents=True)
    if path == "directo":
        c_library(folder)
        refs = DIRECT_REF
    else:
        body = metadata_body(body)
        declarations = metadata_body(declarations)
        refs = '<LibraryReference Include="../../producer/Pub.lib" />'
    (folder / "Main.ns").write_text(declarations + "int Main() { " + body + " }\n", encoding="utf-8")
    project(folder, "N", "Main.ns", refs)
    return folder, build(folder, "N", "exe", "out/N.exe")


def check_positive(path):
    folder, result = case(path, "control", "", POSITIVE)
    code = run([folder / "out" / "N.exe"], Path("A:/"), check=False).returncode if result.returncode == 0 else None
    if observe:
        print(path, "control", result.returncode, code, errors(result), flush=True)
    elif result.returncode or code:
        raise AssertionError(f"{path} control: build {result.returncode}, salida {code}:\n{result.stdout}")
    else:
        print(path, "control correcto", flush=True)


def check_rejections(path):
    for name, declarations, body, texts in REJECTIONS:
        if path == "metadata" and name == "03e-funcion-c":
            continue
        _, result = case(path, name, declarations, body)
        if observe:
            print(path, name, result.returncode, errors(result), flush=True)
            continue
        missing = [t for t in texts if t not in result.stdout]
        if result.returncode == 0 or missing or "internal error" in result.stdout or "Main.ns" not in result.stdout:
            raise AssertionError(f"{path} {name}: faltan {missing}:\n{result.stdout}")
        print(path, name, "rechazado", flush=True)


producer()
for path in ("directo", "metadata"):
    check_positive(path)
    check_rejections(path)
print("observación terminada" if observe else "Diagnósticos de tipos C incompletos verificados:", root)
