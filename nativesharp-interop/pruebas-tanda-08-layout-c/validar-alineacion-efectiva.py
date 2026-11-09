"""T08-D15: Clang's effective alignment for C member accesses, used where the record's alignment is guaranteed.

Clang emits a member access with `alignmentAtOffset(record alignment, offset)`: a `long long`-aligned struct gives
`align 8` to an `int` at offset 8, `_Alignas(16)` gives `align 16`, a `pack(2)` struct gives `align 2`, a nested
record combines its offset with the outer one, and `p[i].m` keeps the record alignment because the stride is a
multiple of it. NativeSharp derived `min(natural alignment, ...)` and never went above the natural alignment. Now it
uses the same value as Clang, computed from the record alignment and member offsets that NSMETA already carries.

Checks, per access, that NativeSharp's IR alignment equals Clang's (globals, pointers, a local copy, nested
members, packed and `_Alignas` records, array elements through a pointer, and a pointer to a member, which only
keeps the natural alignment), and runs the accesses O0-O3 directly and through a precompiled template consumed
without the header, with a C object handed over by pointer and a NativeSharp class that owns an `_Alignas(16)`
record (allocated with the record's alignment).
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-alineacion-efectiva-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef struct S { long long a; int b; int c; } S;
typedef struct A { char tag; _Alignas(16) int x; int y; } A;
#pragma pack(push, 2)
typedef struct P { char c; long long v; int w; } P;
#pragma pack(pop)
typedef struct N { int pre; S inner; } N;
extern S gs;
extern A ga;
extern P gp;
extern N gn;
S *c_items(void);
A *c_aligned(void);
int c_sum(void);
"""
SOURCE = """#include "api.h"
S gs = {1, 2, 3};
A ga = {4, 5, 6};
P gp = {7, 8, 9};
N gn = {10, {11, 12, 13}};
static S items[3] = {{20, 21, 22}, {23, 24, 25}, {26, 27, 28}};
static A aligned = {30, 31, 32};
S *c_items(void) { return items; }
A *c_aligned(void) { return &aligned; }
int c_sum(void) { return gs.b + ga.x + (int)gp.v + gn.inner.b + items[1].b + aligned.x; }
"""
# EN: one function per access, as in the Clang probe; each returns the value read.
# ES: una funcion por acceso, como en la sonda de Clang; cada una devuelve el valor leido.
ACCESSES = {
    "global_b": ("int", "gs.b", 8), "global_c": ("int", "gs.c", 4), "global_x": ("int", "ga.x", 16),
    "global_y": ("int", "ga.y", 4), "global_pv": ("long", "gp.v", 2), "global_inner_b": ("int", "gn.inner.b", 8),
    "ptr_b": ("int", "c_items()->b", 8), "ptr_x": ("int", "c_aligned()->x", 16),
}
NS_FUNCTIONS = "".join(f"public static {t} {name}() {{ return {expr}; }}\n" for name, (t, expr, _) in ACCESSES.items())
NS_FUNCTIONS += ("public static int local_b() { S s = gs; return s.b; }\n"
                 "public static int array_b(int i) { S* p = c_items(); return p[i].b; }\n"
                 "public static int member_pointer_b() { int* q = &gs.b; return *q; }\n")
EXPECTED_EXTRA = {"local_b": 8, "array_b": 8, "member_pointer_b": 4}
# EN: generic accessors of the precompiled template, instantiated by the consumer without the header.
# ES: accesores genericos de la plantilla precompilada, instanciados por el consumidor sin el encabezado.
GENERIC = {"generic_x": ("ga.x", 16), "generic_inner_b": ("gn.inner.b", 8), "generic_pv": ("(int)gp.v", 2),
           "generic_ptr_x": ("c_aligned()->x", 16), "generic_array_b": ("c_items()[i].b", 8)}
GENERIC_FUNCTIONS = "".join(f"public static int {name}<T>(T unused, int i) {{ return {expr}; }}\n"
                            for name, (expr, _) in GENERIC.items())
GENERIC_BODY = """        if (generic_x<T>(unused, 0) != 5 || generic_inner_b<T>(unused, 0) != 12 || generic_pv<T>(unused, 0) != 8)
            return 7;
        if (generic_ptr_x<T>(unused, 0) != 31 || generic_array_b<T>(unused, 2) != 27) return 8;
"""
BODY = """        if (global_b() != 2 || global_c() != 3 || global_x() != 5 || global_y() != 6) return 1;
        if (global_pv() != 8 || global_inner_b() != 12 || ptr_b() != 21 || ptr_x() != 31) return 2;
        if (local_b() != 2 || array_b(1) != 24 || array_b(2) != 27 || member_pointer_b() != 2) return 3;
        if (c_sum() != 2 + 5 + 8 + 12 + 24 + 31) return 4;
        Holder h = new Holder();
        h.value = ga;
        h.value.x = 40;
        if (h.value.x != 40 || h.value.y != 6) return 5;
        S* items = c_items();
        items[1].b = 50;
        if (c_sum() != 2 + 5 + 8 + 12 + 50 + 31) return 6;
        return 0;
"""


def program(namespace_open, namespace_close, generic):
    signature = "public static int Run<T>(T unused)" if generic else "public static int Run()"
    return (namespace_open + "public class Holder { public A value; }\npublic class Accesses {\n" + NS_FUNCTIONS +
            (GENERIC_FUNCTIONS if generic else "") + signature + " {\n" + (GENERIC_BODY if generic else "") +
            BODY + "    }\n}\n" + namespace_close)


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


def build(folder, name, opt, kind="exe", output=None):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output or f"out/{name}.exe"], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


def last_load_alignments(text, names):
    """EN: (name, expected, loads) for each accessor whose last load differs. ES: discrepancias por accesor."""
    found = []
    for name, alignment in names.items():
        bodies = re.findall(r"define [^\n]*Accesses[^\n]*" + name + r"[^\n]*\{(.*?)\n\}", text, re.S)
        loads = re.findall(r"= load (?:i32|i64), ptr [^\n]*, align (\d+)", bodies[0]) if bodies else []
        if not loads or int(loads[-1]) != alignment:
            found.append((name, alignment, loads))
    return found



# EN: IR: the alignment of the single load of each accessor must be Clang's.
# ES: IR: la alineacion de la unica lectura de cada accesor debe ser la de Clang.
ir = root / "ir"
fixture(ir)
(ir / "Main.ns").write_text(program("", "", False) + "int Main() { return Accesses.Run(); }\n", encoding="utf-8")
project(ir, "Ir", ["Main.ns"], REFERENCE)
dump = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm", "Ir.nsproj"], ir, check=False)
(ir / "native.ll").write_text(dump.stdout, encoding="utf-8")
expected = {name: alignment for name, (_, _, alignment) in ACCESSES.items()}
expected.update(EXPECTED_EXTRA)
mismatches = last_load_alignments(dump.stdout, expected)
if observe:
    print("IR", mismatches or "coincide con Clang", flush=True)
elif mismatches:
    raise AssertionError(f"IR: alineaciones distintas de Clang (acceso, esperada, obtenidas): {mismatches}\n" +
                         dump.stdout[:3000])
else:
    print("IR: las", len(expected), "alineaciones coinciden con Clang", flush=True)



for opt in opts:
    folder = root / f"directo-O{opt}"
    fixture(folder)
    (folder / "Main.ns").write_text(program("", "", False) + "int Main() { return Accesses.Run(); }\n",
                                    encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], REFERENCE)
    result = build(folder, "Direct", opt)
    if result.returncode:
        if observe:
            print("directo", f"O{opt}", errors(result), flush=True)
            continue
        raise AssertionError(f"directo O{opt}:\n{result.stdout}")
    code = run([folder / "out" / "Direct.exe"], Path("A:/"), check=False).returncode
    if code:
        raise AssertionError(f"directo O{opt}: comprobación {code} falló")
    print("directo", f"O{opt}", "correcto", flush=True)
    folder = root / f"precompilado-O{opt}"
    fixture(folder / "producer")
    (folder / "producer" / "Producer.ns").write_text(
        program("namespace Aligned {\n", "}\n", True), encoding="utf-8")
    project(folder / "producer", "Aligned", ["Producer.ns"], REFERENCE, library=True)
    result = build(folder / "producer", "Aligned", opt, "lib", "Aligned.lib")
    if result.returncode:
        if observe:
            print("productor", f"O{opt}", errors(result), flush=True)
            continue
        raise AssertionError("productor:\n" + result.stdout)
    (folder / "producer" / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Aligned.Accesses.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Aligned.lib" />')
    result = build(folder, "Consumer", opt)
    if result.returncode:
        if observe:
            print("consumidor", f"O{opt}", errors(result), flush=True)
            continue
        raise AssertionError(f"consumidor O{opt}:\n{result.stdout}")
    code = run([folder / "out" / "Consumer.exe"], Path("A:/"), check=False).returncode
    if code:
        raise AssertionError(f"precompilado O{opt}: comprobación {code} falló")
    if opt == opts[0]:
        # EN: the consumer instantiates the generic accessors with the alignments rebuilt from NSMETA.
        # ES: el consumidor instancia los accesores genericos con las alineaciones reconstruidas de NSMETA.
        dump = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm", "Consumer.nsproj"], folder,
                   check=False)
        (folder / "consumer.ll").write_text(dump.stdout, encoding="utf-8")
        generic = last_load_alignments(dump.stdout, {name: alignment for name, (_, alignment) in GENERIC.items()})
        if generic:
            raise AssertionError(f"IR del consumidor: alineaciones distintas de Clang: {generic}\n" +
                                 dump.stdout[:3000])
        print("IR del consumidor precompilado: las", len(GENERIC), "alineaciones coinciden con Clang", flush=True)
    print("precompilado", f"O{opt}", "correcto", flush=True)
print("observación terminada" if observe else "Alineación efectiva de Clang verificada:", root)
