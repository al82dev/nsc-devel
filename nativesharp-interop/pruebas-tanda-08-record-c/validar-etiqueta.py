"""T08-010-10: C records and enums declared only with a tag, published through templates.

`struct Holder { ... };` without a typedef is usable directly, but a template that named it was published without
the record: the consumer without the header reported "unknown type name 'Holder' in module 'Pub'". The producer
now seeds the C records, unions and enums whose tag matches a type name written in a published template.

Cases (direct with the header, and a template of a precompiled producer consumed without the header, O0-O3):
- a struct with only a tag, read and written in the template and checked by C;
- a union and an enum with only a tag;
- a struct with only a tag whose field is another struct with only a tag (record closure);
- a tag plus a separate typedef with another name, the template using the tag.
Rejection in O0: a tag that no published template names is not published, so the consumer without the header
cannot name it ("unknown type name 'Unused'").
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-etiqueta-", dir=r"A:\nsc-interop-build"))
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

HEADER = """struct Holder { int v; short s; };
union Number { int i; unsigned u; };
enum Tone { Low = 2, High = 9 };
struct Inner { char c; int n; };
struct Outer { struct Inner in2; enum Tone t; };
struct Tagged { long long big; };
typedef struct Tagged TaggedAlias;
struct Unused { int z; };
int check_holder(const struct Holder *h);
int check_outer(const struct Outer *o);
"""
SOURCE = """#include "api.h"
int check_holder(const struct Holder *h) { return h->v == 41 && h->s == -7; }
int check_outer(const struct Outer *o) { return o->in2.c == 'x' && o->in2.n == 123 && o->t == High; }
"""
BODY = """    Holder h;
    h.v = 41;
    h.s = -7;
    if (check_holder(&h) != 1 || h.v + h.s != 34) return 1;
    Number n;
    n.i = -1;
    if (n.u != 4294967295) return 2;
    Tone t = High;
    if ((int)t != 9 || Low >= High) return 3;
    Outer o;
    o.in2.c = (sbyte)'x';
    o.in2.n = 123;
    o.t = t;
    if (check_outer(&o) != 1) return 4;
    Inner copy = o.in2;
    if (copy.n != 123) return 5;
    Tagged g;
    g.big = 5000000000L;
    return g.big == 5000000000L ? 0 : 6;
"""


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder):
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "tags.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "tags.c", "-o", "tags.obj"], folder)
    run([lib, "/nologo", "/out:tags.lib", "tags.obj"], folder)


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


REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="tags.lib" />'
for opt in opts:
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
    (producer / "P.ns").write_text("namespace Pub { public class Tags { public static int Run<T>(T unused)\n{\n" +
                                   BODY + "} } }\n", encoding="utf-8")
    project(producer, "Pub", "P.ns", REF, library=True)
    result = build(producer, "Pub", opt, "lib", "Pub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (producer / "api.h").unlink()
    (base / "C.ns").write_text("int Main() { return Pub.Tags.Run<int>(0); }\n", encoding="utf-8")
    project(base, "C", "C.ns", '<LibraryReference Include="producer/Pub.lib" />')
    expect_run(f"precompilado sin encabezado O{opt}", base, "C", opt)
    if opt == "0":
        negative = base / "rechazo"
        negative.mkdir()
        (negative / "N.ns").write_text("int Main() { Unused u; u.z = 1; return Pub.Tags.Run<int>(0) + u.z; }\n",
                                       encoding="utf-8")
        project(negative, "N", "N.ns", '<LibraryReference Include="../producer/Pub.lib" />')
        result = build(negative, "N", "0", "exe", "out/N.exe")
        if observe:
            print("rechazo", result.returncode, errors(result), flush=True)
        elif result.returncode == 0 or "unknown type name 'Unused'" not in result.stdout or \
                "internal error" in result.stdout:
            raise AssertionError(f"rechazo: se esperaba «unknown type name 'Unused'»:\n{result.stdout}")
        else:
            print("rechazo correcto", flush=True)
print("observación terminada" if observe else "Records y enums C solo con etiqueta verificados:", root)
