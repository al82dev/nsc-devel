"""T08-D17: `int const** q = &p` with `p` an `int*` is rejected; `T const**` means what it means in C.

In a pointer to pointer the postfix `const` qualifies the base type, as in C: `int const**` is a pointer to a mutable
pointer to a constant `int`. Then `*q` is an assignable `int const*`, `**q` is readonly, `&c` with `c` an
`int const*` is an `int const**`, and an `int**` does not become an `int const**` implicitly (C warns, C++ rejects),
because a `int const*` stored through it would be writable through the original `int**`. NativeSharp cannot spell
`int const* const*`, so the only way is an explicit cast. A readonly pointer has no address (it would be
`int* const*`).

Positive cases O0-O3, directly and through a template of a precompiled producer consumed without the header, with
C functions that read and write through `int**` and `const int*`, a NativeSharp function and a public method of the
producer with an `int const**` parameter. Rejections (O0): initialization, assignment, call and return from `int**`,
three levels, writing `**q`, `int*` from `*q`, `int**` from `&c`, mixed conditional, the address of an `in` pointer
parameter, and a consumer that passes an `int**` to the producer's published `int const**` parameter. A Clang C++
control shows the same rejection, and C reports it as a qualifier discard.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-puntero-a-puntero-const-", dir=r"A:\nsc-interop-build"))
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

HEADER = """int read_const(const int *p);
int read_pp(int **pp);
void retarget(int **pp, int *target);
const int *c_limit(void);
"""
SOURCE = """#include "api.h"
static const int limit = 70;
int read_const(const int *p) { return *p; }
int read_pp(int **pp) { return **pp; }
void retarget(int **pp, int *target) { *pp = target; }
const int *c_limit(void) { return &limit; }
"""
FUNCTIONS = """public static int ReadBase(int const** q) { return **q; }
public static void Store(int const** q, int const* value) { *q = value; }
public static int const** Same(int const** q) { return q; }
"""
BODY = """        int one = 1;
        int two = 2;
        int const* c = &one;
        int const** q = &c;
        if (**q != 1 || *q != &one || q[0] != &one) return 1;
        int const* d = &two;
        *q = d;
        if (c != &two || **q != 2 || read_const(*q) != 2) return 2;
        q[0] = c_limit();
        if (c != c_limit() || **q != 70 || ReadBase(q) != 70) return 3;
        Store(q, &one);
        if (c != &one || **Same(q) != 1) return 4;
        int const** empty = null;
        if (empty != null) return 5;
        int* p = &two;
        int** pp = &p;
        if (read_pp(pp) != 2) return 6;
        retarget(pp, &one);
        if (p != &one || **pp != 1) return 7;
        int const** forced = (int const**)pp;
        if (**forced != 1) return 8;
        var inferred = &c;
        if (**inferred != 1 || ReadBase(inferred) != 1) return 9;
        return 0;
"""


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


# EN: Clang control: C++ rejects `const int **q = &p`; C reports the discarded qualifier in a nested pointer.
# ES: control Clang: C++ rechaza `const int **q = &p`; C informa el calificador descartado en un puntero anidado.
control = root / "control"
control.mkdir()
(control / "nested.c").write_text("int f(void) { int x = 1; int *p = &x; const int **q = &p; return **q; }\n",
                                  encoding="utf-8")
cxx = run([clang, "-fsyntax-only", "--target=x86_64-pc-windows-msvc", "-x", "c++", "nested.c"], control, False)
c = run([clang, "-fsyntax-only", "--target=x86_64-pc-windows-msvc", "-std=c11", "nested.c"], control, False)
if cxx.returncode == 0 or "discards qualifiers in nested pointer types" not in c.stdout:
    raise AssertionError(f"control Clang:\nC++:\n{cxx.stdout}\nC:\n{c.stdout}")
print("control Clang correcto: C++ rechaza y C avisa", flush=True)

for opt in opts:
    folder = root / f"directo-O{opt}"
    fixture(folder)
    (folder / "Main.ns").write_text(
        "public class Nested {\n" + FUNCTIONS + "public static int Run() {\n" + BODY + "    }\n}\n"
        "int Main() { return Nested.Run(); }\n", encoding="utf-8")
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
        "namespace Nested { public class Bridge {\n" + FUNCTIONS +
        "public static int Run<T>(T unused) {\n" + BODY + "    }\n} }\n", encoding="utf-8")
    project(folder / "producer", "Nested", ["Producer.ns"], REFERENCE, library=True)
    result = build(folder / "producer", "Nested", opt, "lib", "Nested.lib")
    if result.returncode:
        raise AssertionError("productor:\n" + result.stdout)
    (folder / "producer" / "api.h").unlink()
    (folder / "Consumer.ns").write_text(
        "int Main()\n{\n    int x = 5;\n    int const* c = &x;\n"
        "    if (Nested.Bridge.ReadBase(&c) != 5) return 20;\n    return Nested.Bridge.Run<int>(0);\n}\n",
        encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Nested.lib" />')
    result = build(folder, "Consumer", opt)
    if result.returncode:
        if observe:
            print("consumidor", f"O{opt}", errors(result), flush=True)
            continue
        raise AssertionError(f"consumidor O{opt}:\n{result.stdout}")
    code = run([folder / "out" / "Consumer.exe"], Path("A:/"), check=False).returncode
    if code:
        raise AssertionError(f"precompilado O{opt}: comprobación {code} falló")
    print("precompilado", f"O{opt}", "correcto", flush=True)

NESTED = "cannot become a 'T const**' implicitly"
REJECTIONS = [
    ("inicializacion", "int x = 1; int* p = &x; int const** q = &p; return 0;", NESTED),
    ("asignacion", "int x = 1; int* p = &x; int const** q = null; q = &p; return 0;", NESTED),
    ("llamada", "int x = 1; int* p = &x; return Nested.ReadBase(&p);", "no viable overload"),
    ("retorno", "return 0; } public static int const** Widen(int** p) { return p; } static int Unused() { return 0;",
     NESTED),
    ("tres-niveles", "int x = 1; int* p = &x; int** pp = &p; int const*** q = &pp; return 0;", NESTED),
    ("var", "int x = 1; int* p = &x; var r = &p; int const** q = r; return 0;", NESTED),
    ("escritura-base", "int x = 1; int const* c = &x; int const** q = &c; **q = 3; return 0;",
     "left side of assignment is not assignable"),
    ("base-a-mutable", "int x = 1; int const* c = &x; int const** q = &c; int* m = *q; return 0;",
     "would discard pointee constness"),
    ("direccion-a-mutable", "int x = 1; int const* c = &x; int** q = &c; return 0;",
     "would discard pointee constness"),
    ("condicional", "int x = 1; int* p = &x; int const* c = &x; int** a = &p; int const** b = &c; "
     "var r = x > 0 ? a : b; return 0;", "same pointee qualification"),
    ("direccion-de-in", "return 0; } public static int Address(in int* p) { int** q = &p; return 0;",
     "cannot take the address of a readonly pointer"),
]
for name, statements, expected in REJECTIONS:
    folder = root / "rechazos" / name
    fixture(folder)
    (folder / "Main.ns").write_text("public class Nested {\n" + FUNCTIONS + "public static int Run() { " +
                                    statements + " }\n}\nint Main() { return Nested.Run(); }\n", encoding="utf-8")
    project(folder, "Negative", ["Main.ns"], REFERENCE)
    result = build(folder, "Negative", "0")
    if observe:
        print(name, result.returncode, errors(result), flush=True)
        continue
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)

# EN: the producer publishes `int const**` in NSMETA; a consumer cannot pass it an `int**`.
# ES: el productor publica `int const**` en NSMETA; un consumidor no puede pasarle un `int**`.
folder = root / "rechazos" / "precompilado"
fixture(folder / "producer")
(folder / "producer" / "Producer.ns").write_text("namespace Nested { public class Bridge {\n" + FUNCTIONS + "} }\n",
                                                 encoding="utf-8")
project(folder / "producer", "Nested", ["Producer.ns"], REFERENCE, library=True)
result = build(folder / "producer", "Nested", "0", "lib", "Nested.lib")
if result.returncode:
    raise AssertionError("productor del rechazo:\n" + result.stdout)
(folder / "producer" / "api.h").unlink()
(folder / "Consumer.ns").write_text("int Main() { int x = 1; int* p = &x; return Nested.Bridge.ReadBase(&p); }\n",
                                    encoding="utf-8")
project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Nested.lib" />')
result = build(folder, "Consumer", "0")
if observe:
    print("precompilado", result.returncode, errors(result), flush=True)
elif result.returncode == 0 or "no viable overload" not in result.stdout:
    raise AssertionError("rechazo precompilado: se esperaba «no viable overload»:\n" + result.stdout)
else:
    print("rechazo precompilado correcto", flush=True)
print("observación terminada" if observe else "Punteros a puntero const verificados:", root)
