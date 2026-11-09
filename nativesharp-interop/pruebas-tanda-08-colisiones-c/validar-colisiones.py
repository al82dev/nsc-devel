"""T08-018-02 and T08-018-03: name collisions between C declarations and NativeSharp.

02a) A C function and a NativeSharp namespace function with the same name.
02b) A C global and a C enumerator hidden by a NativeSharp local and parameter.
02c) A C typedef and a NativeSharp type with the same name.
02d) Shadows in blocks, loops, `catch` and lambdas.
03a) Resolution before, inside and after the shadowing scope.
03b) Ambiguity between two visible C declarations of different projects.
03c) The function and global cases from a template of a precompiled producer consumed without the header.
A private member used by a published template is not published; see T08-018-03c1 in the commit record.
Every positive case returns a value computed by C and checked in NativeSharp; positive cases run in O0-O3,
rejections in O0.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-colisiones-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef enum Level { Low = 1, High = 9 } Level;
typedef struct Gauge { int value; } Gauge;
extern int counter;
int twice(int v);
int read_counter(void);
"""
SOURCE = """#include "api.h"
int counter = 40;
int twice(int v) { return 2 * v; }
int read_counter(void) { return counter; }
"""
CASES = [
    # 02a: the NativeSharp namespace function and the C function are distinct callables.
    ("02a-funcion", "namespace Mine { public class F { public static int twice(int v) { return 3 * v; } } }\n",
     "return Mine.F.twice(2) == 6 && twice(2) == 4 ? 0 : 1;"),
    # 02b / 03a: local and parameter hide the C global and enumerator only inside their scope.
    ("02b-local-parametro", "int Use(int counter, int High) { return counter + High; }\n",
     "int before = counter;\n    int inside = 0;\n    { int counter = 1; int High = 2; inside = counter + High; }\n"
     "    int after = counter + (int)High;\n"
     "    return before == 40 && inside == 3 && after == 49 && Use(5, 6) == 11 && read_counter() == 40 ? 0 : 1;"),
    # 02b: a static field of the enclosing class hides the C global; a member method hides the C function.
    ("02b-miembros-clase",
     "public class Names\n{\n    static int counter = 7;\n    static int twice(int v, int w) { return v + w; }\n"
     "    public static int Run() { return counter == 7 && twice(1, 1) == 2 && read_counter() == 40 ? 0 : 1; }\n}\n",
     "return Names.Run() == 0 && counter == 40 && twice(5) == 10 ? 0 : 1;"),
    # 02c: a NativeSharp type with the same name as a C typedef in another namespace.
    ("02c-typedef-tipo", "namespace Mine { public struct Gauge { public long big; } }\n",
     "Gauge c; c.value = 3; Mine.Gauge n; n.big = 5000000000L;\n"
     "    return c.value == 3 && n.big == 5000000000L ? 0 : 1;"),
    # 02d: shadows in a loop, a catch and a lambda; the global is visible again after each one.
    ("02d-bucle-catch-lambda",
     "namespace NativeSharp.Core { public class Exception { } }\nusing NativeSharp.Core;\n"
     "public class Boom : Exception { }\ndelegate int IntFn(int v);\n",
     "int sum = 0;\n    for (int counter = 0; counter < 3; counter++) { sum += counter; }\n"
     "    int caught = 0;\n    try { throw new Boom(); } catch (Boom counter) { caught = counter != null ? 1 : 2; }\n"
     "    IntFn f = (counter) => counter + 1;\n    IntFn g = (v) => v + counter;\n"
     "    return sum == 3 && caught == 1 && f(1) == 2 && g(2) == 42 && counter == 40 ? 0 : 1;"),
]
REJECTIONS = [
    # 02a: inside a class, a member method named like the C function hides it for every arity.
    ("02a-miembro-oculta-funcion",
     "public class Names { static int twice(int v, int w) { return v + w; }\n"
     "    public static int Run() { return twice(1); } }\n", "return Names.Run();", "no overload of 'twice'"),
    ("02c-typedef-tipo-mismo-namespace", "public struct Gauge { public long big; }\n", "Gauge g; return 0;", "Gauge"),
    ("03b-dos-globales-c", "", "return (int)dup;", "dup"),
    ("03b-dos-encabezados", "", "return (int)dup;", "dup"),
]


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder, name="ccol", header=HEADER, source=SOURCE, file="api"):
    (folder / f"{file}.h").write_text(header, encoding="utf-8")
    (folder / f"{file}.c").write_text(f'#include "{file}.h"\n' + source.replace('#include "api.h"\n', ""),
                                      encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", f"{file}.c", "-o", f"{file}.obj"], folder)
    run([lib, "/nologo", f"/out:{name}.lib", f"{file}.obj"], folder)


def ref(header="api.h", library="ccol"):
    return f'<CppLibraryReference Header="{header}" Language="C" Standard="c11" Library="{library}.lib" />'


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


def expect_reject(label, result, text):
    if observe:
        print(label, result.returncode, errors(result), flush=True)
        return
    if result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"{label}: se esperaba «{text}»:\n{result.stdout}")
    print(label, "rechazado", flush=True)


def positive(opt):
    for name, declarations, body in CASES:
        folder = root / f"O{opt}" / name
        folder.mkdir(parents=True)
        c_library(folder)
        (folder / "Main.ns").write_text(declarations + "int Main()\n{\n    " + body + "\n}\n", encoding="utf-8")
        project(folder, "P", "Main.ns", ref())
        expect_run(f"{name} O{opt}", folder, "P", opt)


def precompiled(opt):
    # 03c: the template reads its public class field named like the C global, calls the C function, reads the C
    # enumerator, and shadows the field with a local. A private member is not published (T08-018-03c1).
    base = root / f"O{opt}" / "03c-precompilado"
    producer = base / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "P.ns").write_text(
        "namespace Pub { public class Names {\n"
        "    public static int counter = 7;\n"
        "    public static int Run<T>(T unused)\n    {\n"
        "        int field = counter;\n"
        "        int call = twice(1);\n"
        "        int inside = 0;\n        { int counter = 2; inside = counter + twice(1); }\n"
        "        int after = counter + (int)High;\n"
        "        return field == 7 && call == 2 && inside == 4 && after == 16 && read_counter() == 40 ? 0 : 1;\n"
        "    }\n} }\n",
        encoding="utf-8")
    project(producer, "Pub", "P.ns", ref(), library=True)
    result = build(producer, "Pub", opt, "lib", "Pub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (producer / "api.h").unlink()
    (base / "C.ns").write_text("int Main() { return Pub.Names.Run<int>(0); }\n", encoding="utf-8")
    project(base, "C", "C.ns", '<LibraryReference Include="producer/Pub.lib" />')
    expect_run(f"03c-precompilado O{opt}", base, "C", opt)


def conflicting_sides(folder):
    sides = []
    for letter, ctype, value in (("a", "int", "1"), ("b", "long long", "2")):
        side = folder / letter
        side.mkdir()
        c_library(side, f"c{letter}", f"extern {ctype} dup;\n", f"{ctype} dup = {value};\n", f"d{letter}")
        sides.append(side)
    return sides


def rejections():
    for name, declarations, body, text in REJECTIONS:
        folder = root / "rechazos" / name
        folder.mkdir(parents=True)
        c_library(folder)
        refs = ref()
        if name == "03b-dos-globales-c":
            # Two referenced projects publish incompatible C declarations of the same global.
            for side, letter in zip(conflicting_sides(folder), "ab"):
                (side / f"{letter}.ns").write_text(f"namespace Side{letter.upper()} {{ public class X {{ }} }}\n",
                                                   encoding="utf-8")
                project(side, f"Side{letter.upper()}", f"{letter}.ns",
                        ref(f"d{letter}.h", f"c{letter}"), library=True)
            refs = '<ProjectReference Include="a/SideA.nsproj" /><ProjectReference Include="b/SideB.nsproj" />'
        elif name == "03b-dos-encabezados":
            # The same project imports two headers whose translation units declare the global differently.
            conflicting_sides(folder)
            refs = ref("a/da.h", "a/ca") + ref("b/db.h", "b/cb")
        (folder / "Main.ns").write_text(declarations + "int Main() { " + body + " }\n", encoding="utf-8")
        project(folder, "N", "Main.ns", refs)
        expect_reject(f"rechazo {name}", build(folder, "N", "0", "exe", "out/N.exe"), text)


for opt in opts:
    positive(opt)
    precompiled(opt)
rejections()
print("observación terminada" if observe else "Colisiones de nombres C verificadas:", root)
