"""T08-005-03: reconciliation of C typedefs.

a) Two headers of one project declare the same typedefs with equivalent spellings (`int` and a chain that ends in
   `int`; the same anonymous-free struct typedef). The names resolve to one type, directly and through a template of
   a precompiled producer consumed without the headers.
b) Two headers declare `score_t` and `Box` with incompatible underlying types: the project still compiles while the
   names are unused, and every use is rejected. Two producers that each see one of the headers publish
   templates with them; one consumer that uses both is ambiguous (current contract, decision T08-005-03b1).
c) Consumption through metadata: a consumer without the header calls templates and non-generic public methods
   whose signatures use the typedefs; a consumer with the header mixes its own values of the typedef types with
   those of the producer.
Clang controls: the equivalent pair compiles in one translation unit and the incompatible pair is rejected.
Positive cases run in O0-O3; rejections in O0.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import textwrap

root = Path(tempfile.mkdtemp(prefix="t08-typedef-reconciliacion-", dir=r"A:\nsc-interop-build"))
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

EQUIVALENT = {
    "a.h": "typedef int score_t;\ntypedef struct Pair { int a; int b; } Pair;\n"
           "int a_score(score_t s);\nint a_sum(Pair p);\nint a_product(Pair *p);\n",
    "b.h": "typedef int base_t;\ntypedef base_t score_t;\nint b_score(score_t s);\n",
}
EQUIVALENT_SOURCES = {
    "ca": '#include "a.h"\nint a_score(score_t s) { return s * 10; }\nint a_sum(Pair p) { return p.a + p.b; }\n'
          'int a_product(Pair *p) { return p->a * p->b; }\n',
    "cb": '#include "b.h"\nint b_score(score_t s) { return s * 100; }\n',
}
EQUIVALENT_BODY = """    score_t s = (score_t)3;
    if (a_score(s) + b_score(s) != 330) return 1;
    Pair p;
    p.a = 4;
    p.b = 5;
    if (a_sum(p) != 9 || a_product(&p) != 20) return 2;
    base_t raw = s;
    if (a_score(raw) != 30 || b_score((base_t)1) != 100) return 3;
    return 0;
"""

CONFLICT = {
    "x.h": "typedef int score_t;\ntypedef struct { int a; } Box;\nint x_score(score_t s);\nint x_box(Box b);\n",
    "y.h": "typedef long long score_t;\ntypedef struct { long long a; } Box;\nlong long y_score(score_t s);\n"
           "long long y_box(Box b);\n",
}
CONFLICT_SOURCES = {
    "cx": '#include "x.h"\nint x_score(score_t s) { return s + 1; }\nint x_box(Box b) { return b.a + 2; }\n',
    "cy": '#include "y.h"\nlong long y_score(score_t s) { return s + 3; }\n'
          'long long y_box(Box b) { return b.a + 4; }\n',
}


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_sources(folder, headers, sources):
    for name, text in headers.items():
        (folder / name).write_text(text, encoding="utf-8")
    for name, text in sources.items():
        (folder / f"{name}.c").write_text(text, encoding="utf-8")
        run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", f"{name}.c", "-o", f"{name}.obj"], folder)
        run([lib, "/nologo", f"/out:{name}.lib", f"{name}.obj"], folder)


def refs(pairs):
    return "".join(f'<CppLibraryReference Header="{header}" Language="C" Standard="c11" Library="{library}.lib" />'
                   for header, library in pairs)


def project(folder, name, sources, references, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + references +
        "</ItemGroup></Project>", encoding="utf-8")


def nsc_build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


def expect_run(label, folder, name, opt):
    result = nsc_build(folder, name, opt, "exe", f"out/{name}.exe")
    code = run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode \
        if result.returncode == 0 else None
    if observe:
        print(label, result.returncode, code, errors(result), flush=True)
        return
    if result.returncode or code:
        raise AssertionError(f"{label}: build {result.returncode}, salida {code}:\n{result.stdout}")
    print(label, "correcto", flush=True)


def expect_lib(label, folder, name, opt):
    result = nsc_build(folder, name, opt, "lib", f"{name}.lib")
    if result.returncode:
        raise AssertionError(f"{label}: {result.stdout}")


def expect_reject(label, result, text):
    if observe:
        print(label, result.returncode, errors(result), flush=True)
        return
    if result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"{label}: se esperaba «{text}»:\n{result.stdout}")
    print(label, "rechazado", flush=True)


def clang_controls():
    folder = root / "control"
    folder.mkdir()
    for name, headers, accepted in (("equivalentes", EQUIVALENT, True), ("incompatibles", CONFLICT, False)):
        for header, text in headers.items():
            (folder / header).write_text(text, encoding="utf-8")
        probe = folder / f"{name}.c"
        probe.write_text("".join(f'#include "{h}"\n' for h in headers) + "int probe(void) { return 0; }\n",
                         encoding="utf-8")
        result = run([clang, "--target=x86_64-pc-windows-msvc", "-std=c11", "-fsyntax-only", probe.name], folder,
                     check=False)
        if observe:
            print("Clang", name, result.returncode, flush=True)
        elif (result.returncode == 0) != accepted or (not accepted and "redefinition" not in result.stdout):
            raise AssertionError(f"Clang {name}:\n{result.stdout}")
    print("controles Clang correctos", flush=True)


def equivalent(opt):
    base = root / f"equivalentes-O{opt}"
    direct = base / "directo"
    direct.mkdir(parents=True)
    c_sources(direct, EQUIVALENT, EQUIVALENT_SOURCES)
    (direct / "Main.ns").write_text("int Main()\n{\n" + EQUIVALENT_BODY + "}\n", encoding="utf-8")
    project(direct, "Direct", ["Main.ns"], refs((("a.h", "ca"), ("b.h", "cb"))))
    expect_run(f"a directo O{opt}", direct, "Direct", opt)

    producer = base / "producer"
    producer.mkdir()
    c_sources(producer, EQUIVALENT, EQUIVALENT_SOURCES)
    (producer / "Producer.ns").write_text(
        "namespace Scores { public class Bridge {\n    public static int Run<T>(T unused)\n    {\n" +
        textwrap.indent(EQUIVALENT_BODY, "    ") +
        "    }\n    public static int Twice(score_t s) { return a_score(s) + b_score(s); }\n"
        "    public static Pair Make<T>(T unused, int a) { Pair p; p.a = a; p.b = a + 1; return p; }\n} }\n",
        encoding="utf-8")
    project(producer, "Scores", ["Producer.ns"], refs((("a.h", "ca"), ("b.h", "cb"))), library=True)
    expect_lib(f"a productor O{opt}", producer, "Scores", opt)

    # c) with the headers: the consumer mixes its own values with those of the producer.
    mixed = base / "con-encabezados"
    mixed.mkdir()
    (mixed / "Consumer.ns").write_text(
        "int Main()\n{\n    if (Scores.Bridge.Run<int>(0) != 0) return 1;\n"
        "    score_t own = 2;\n    if (Scores.Bridge.Twice(own) != 220) return 2;\n"
        "    Pair p = Scores.Bridge.Make<int>(0, 6);\n    if (a_sum(p) != 13 || a_product(&p) != 42) return 3;\n"
        "    return 0;\n}\n", encoding="utf-8")
    project(mixed, "Consumer", ["Consumer.ns"], '<LibraryReference Include="../producer/Scores.lib" />' +
            "".join(f'<CppLibraryReference Header="../producer/{h}" Language="C" Standard="c11" />'
                    for h in EQUIVALENT))
    expect_run(f"c con-encabezados O{opt}", mixed, "Consumer", opt)

    # c) a consumer whose own header gives the typedef another type is still ambiguous in its own code.
    if opt == "0":
        other = base / "encabezado-incompatible"
        other.mkdir()
        (other / "other.h").write_text("typedef long long score_t;\n", encoding="utf-8")
        (other / "Consumer.ns").write_text("int Main() { score_t s = 1; return Scores.Bridge.Run<int>(0); }\n",
                                           encoding="utf-8")
        project(other, "Consumer", ["Consumer.ns"], '<LibraryReference Include="../producer/Scores.lib" />'
                '<CppLibraryReference Header="other.h" Language="C" Standard="c11" />')
        expect_reject("rechazo c encabezado-incompatible", nsc_build(other, "Consumer", opt, "exe", "out/C.exe"),
                      "conflicting C/C++ type alias 'score_t'")

    # a) and c) without the headers: only metadata.
    for header in EQUIVALENT:
        (producer / header).rename(producer / f"{header}.oculto")
    hidden = base / "sin-encabezados"
    hidden.mkdir()
    (hidden / "Consumer.ns").write_text(
        "int Main()\n{\n    if (Scores.Bridge.Run<int>(0) != 0) return 1;\n"
        "    if (Scores.Bridge.Twice(3) != 330) return 2;\n"
        "    var p = Scores.Bridge.Make<int>(0, 2);\n    if (p.a + p.b != 5) return 3;\n    return 0;\n}\n",
        encoding="utf-8")
    project(hidden, "Consumer", ["Consumer.ns"], '<LibraryReference Include="../producer/Scores.lib" />')
    expect_run(f"a/c sin-encabezados O{opt}", hidden, "Consumer", opt)


def conflict(opt):
    base = root / f"incompatibles-O{opt}"
    unused = base / "sin-uso"
    unused.mkdir(parents=True)
    c_sources(unused, CONFLICT, CONFLICT_SOURCES)
    (unused / "Main.ns").write_text("int Main() { return x_score(1) == 2 && y_score(1) == 4 ? 0 : 1; }\n",
                                    encoding="utf-8")
    project(unused, "Unused", ["Main.ns"], refs((("x.h", "cx"), ("y.h", "cy"))))
    expect_run(f"b sin-uso O{opt}", unused, "Unused", opt)

    # One producer per header; one consumer uses both templates without headers. Current contract (decision
    # T08-005-03b1): the homonymous typedefs of the two catalogues are ambiguous, as in
    # typedef-funcion-record/validar.py --incompatible.
    if opt != "0":
        return
    for side, header, source, body in (
            ("X", "x.h", "cx", "score_t s = (score_t)2147483647; Box b; b.a = 5; return x_box(b) == 7 ? 0 : 1;"),
            ("Y", "y.h", "cy", "score_t s = (score_t)4294967296; Box b; b.a = s; return y_box(b) == 4294967300 ? 0 : 1;")):
        folder = base / side
        folder.mkdir()
        c_sources(folder, {header: CONFLICT[header]}, {source: CONFLICT_SOURCES[source]})
        (folder / "Producer.ns").write_text(
            f"namespace Side{side} {{ public class Bridge {{ public static int Run<T>(T unused) {{ {body} }} }} }}\n",
            encoding="utf-8")
        project(folder, f"Side{side}", ["Producer.ns"], refs(((header, source),)), library=True)
        expect_lib(f"b productor {side} O{opt}", folder, f"Side{side}", opt)
        (folder / header).unlink()
    (base / "Consumer.ns").write_text(
        "int Main() { return SideX.Bridge.Run<int>(0) + SideY.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(base, "Consumer", ["Consumer.ns"], '<LibraryReference Include="X/SideX.lib" />'
            '<LibraryReference Include="Y/SideY.lib" />')
    expect_reject("rechazo b productores separados", nsc_build(base, "Consumer", opt, "exe", "out/Consumer.exe"),
                  "conflicting C/C++ type alias 'score_t'")


def conflict_rejections():
    for name, statements, text in [
        ("score-local", "score_t s = 1; return 0;", "score_t"),
        ("score-cast", "return (int)(score_t)1;", "score_t"),
        ("box-local", "Box b; return 0;", "Box"),
    ]:
        folder = root / "rechazos" / name
        folder.mkdir(parents=True)
        c_sources(folder, CONFLICT, CONFLICT_SOURCES)
        (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
        project(folder, "Negative", ["Main.ns"], refs((("x.h", "cx"), ("y.h", "cy"))))
        expect_reject(f"rechazo {name}", nsc_build(folder, "Negative", "0", "exe", "out/Negative.exe"), text)


clang_controls()
for opt in opts:
    equivalent(opt)
    conflict(opt)
conflict_rejections()
print("observación terminada" if observe else "Reconciliación de typedef C verificada:", root)
