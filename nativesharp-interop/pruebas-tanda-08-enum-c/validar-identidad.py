"""T08-009-04 and T08-009-05: identity, persistence and consumption of C enums.

04a) A named enum declared with the same values in two headers of one project, and in a common guarded header
     included by both, is one enum; with a different value its enumerator is rejected when used (the project
     still compiles while it is unused, as the two translation units would in C).
04b) Two anonymous enums whose first enumerator has the same name and value are accepted; with another value
     that enumerator is rejected and the remaining enumerators stay usable.
04c) An anonymous enum used as the type of a field of a published record, read and compared in a template.
04d) A named enum declared inside a record (file scope in C), with its record and its values, used by a template
     consumed without the header.
05a) An enum in a non-generic public signature, called by a consumer that has the header.
05b) The same signature and the enumerators through the metadata only, header hidden.
05c) A consumer whose header gives the enum other values is rejected.
Values are compared with functions compiled by Clang from each header. Positive cases run in O0-O3; rejections
in O0.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-enum-identidad-", dir=r"A:\nsc-interop-build"))
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

COMMON = "#ifndef MOOD_H\n#define MOOD_H\nenum Mood { Sad = -1, Happy = 4 };\n#endif\n"

# 04a/04b: (name, a.h, b.h, NativeSharp body, expected diagnostic or None)
TWO_HEADERS = [
    ("nombrado-igual", "enum Mood { Sad = -1, Happy = 4 };\nint a_happy(void);\n",
     "enum Mood { Sad = -1, Happy = 4 };\nint b_happy(void);\n",
     "Mood m = Happy; return (int)m == a_happy() && a_happy() == b_happy() && Sad < Happy ? 0 : 1;", None),
    ("nombrado-comun", '#include "mood.h"\nint a_happy(void);\n', '#include "mood.h"\nint b_happy(void);\n',
     "Mood m = Happy; return (int)m == a_happy() && a_happy() == b_happy() ? 0 : 1;", None),
    ("nombrado-distinto-sin-uso", "enum Mood { Sad = -1, Happy = 4 };\nint a_happy(void);\n",
     "enum Mood { Sad = -1, Happy = 5 };\nint b_happy(void);\n",
     "return a_happy() == 4 && b_happy() == 5 ? 0 : 1;", None),
    ("anonimo-mismo-primero", "enum { First = 1, XOnly = 2 };\nint a_happy(void);\n",
     "enum { First = 1, YOnly = 3 };\nint b_happy(void);\n", "return First + XOnly + YOnly == 6 ? 0 : 1;", None),
    ("anonimo-solo-otros", "enum { First = 1, XOnly = 2 };\nint a_happy(void);\n",
     "enum { First = 5, YOnly = 3 };\nint b_happy(void);\n", "return XOnly + YOnly == 5 ? 0 : 1;", None),
    ("nombrado-distinto", "enum Mood { Sad = -1, Happy = 4 };\nint a_happy(void);\n",
     "enum Mood { Sad = -1, Happy = 5 };\nint b_happy(void);\n", "return (int)Happy;",
     "cannot import C/C++ global 'Happy': conflicting C enumerator declarations"),
    ("anonimo-primero-distinto", "enum { First = 1, XOnly = 2 };\nint a_happy(void);\n",
     "enum { First = 5, YOnly = 3 };\nint b_happy(void);\n", "return First;",
     "cannot import C/C++ global 'First': conflicting C enumerator declarations"),
]

API = """typedef enum Color { Red = 1, Green = 2, Blue = 4 } Color;
typedef struct Rec { enum { RA = 10, RB = 20 } kind; int v; } Rec;
typedef struct Outer { enum Inner { I1 = 5, I2 = 6 } v; } Outer;
Color c_next(Color c);
int rec_kind(const Rec *r);
int outer_value(const Outer *o);
"""
API_SOURCE = """Color c_next(Color c) { return c == Red ? Green : Blue; }
int rec_kind(const Rec *r) { return r->kind == RB ? 200 + r->v : r->kind; }
int outer_value(const Outer *o) { return o->v == I2 ? 66 : 55; }
"""
PRODUCER = """namespace Pub { public class Bridge {
    public static Color Next(Color c) { return c_next(c); }
    public static Color First() { return Red; }
    public static int Kind<T>(T u)
    {
        Rec r;
        r.kind = RB;
        r.v = 1;
        if (r.kind != RB || (int)r.kind != 20 || rec_kind(&r) != 201) return 1;
        r.kind = RA;
        return rec_kind(&r) == 10 ? 0 : 2;
    }
    public static int Nested<T>(T u)
    {
        Outer o;
        o.v = I2;
        Inner x = I1;
        if ((int)o.v != 6 || (int)x != 5 || outer_value(&o) != 66) return 1;
        o.v = x;
        return outer_value(&o) == 55 ? 0 : 2;
    }
} }
"""
CONSUMERS = [
    ("05a-con-encabezado",
     "Color c = Pub.Bridge.Next(Red); return c == Green && Pub.Bridge.Next(c) == Blue ? 0 : 1;", True),
    ("05b-sin-encabezado",
     "var c = Pub.Bridge.Next(Pub.Bridge.First()); Color d = Pub.Bridge.Next(Green);\n"
     "    return (int)c == 2 && c == Green && d == Blue && (int)Pub.Bridge.Next(Red) == 2 ? 0 : 1;", False),
    ("04c-campo-anonimo", "return Pub.Bridge.Kind<int>(0);", False),
    ("04d-anidado", "return Pub.Bridge.Nested<int>(0);", False),
]


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder, name, header, source):
    (folder / f"{name}.c").write_text(f'#include "{header}"\n' + source, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", f"{name}.c", "-o", f"{name}.obj"], folder)
    run([lib, "/nologo", f"/out:{name}.lib", f"{name}.obj"], folder)


def ref(header, library=None):
    extra = f' Library="{library}.lib"' if library else ""
    return f'<CppLibraryReference Header="{header}" Language="C" Standard="c11"{extra} />'


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
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


def expect_reject(label, result, text):
    if observe:
        print(label, result.returncode, errors(result), flush=True)
        return
    if result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"{label}: se esperaba «{text}»:\n{result.stdout}")
    print(label, "rechazado", flush=True)


def two_headers(opt):
    for name, a, b, body, text in TWO_HEADERS:
        if text and opt != "0":
            continue
        folder = root / f"dos-encabezados-O{opt}" / name
        folder.mkdir(parents=True)
        (folder / "mood.h").write_text(COMMON, encoding="utf-8")
        (folder / "a.h").write_text(a, encoding="utf-8")
        (folder / "b.h").write_text(b, encoding="utf-8")
        constant = "Happy" if "Mood" in a or "mood" in a else "First"
        c_library(folder, "ca", "a.h", f"int a_happy(void) {{ return {constant}; }}\n")
        c_library(folder, "cb", "b.h", f"int b_happy(void) {{ return {constant}; }}\n")
        (folder / "Main.ns").write_text("int Main() { " + body + " }\n", encoding="utf-8")
        project(folder, "P", ["Main.ns"], ref("a.h", "ca") + ref("b.h", "cb"))
        if text:
            expect_reject(f"rechazo {name}", nsc_build(folder, "P", opt, "exe", "out/P.exe"), text)
        else:
            expect_run(f"{name} O{opt}", folder, "P", opt)


def precompiled(opt):
    base = root / f"productor-O{opt}"
    producer = base / "producer"
    producer.mkdir(parents=True)
    (producer / "api.h").write_text(API, encoding="utf-8")
    c_library(producer, "capi", "api.h", API_SOURCE)
    (producer / "P.ns").write_text(PRODUCER, encoding="utf-8")
    project(producer, "Pub", ["P.ns"], ref("api.h", "capi"), library=True)
    result = nsc_build(producer, "Pub", opt, "lib", "Pub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    for name, body, header in CONSUMERS:
        if not header and (producer / "api.h").exists():
            (producer / "api.h").rename(producer / "api.h.oculto")
        folder = base / name
        folder.mkdir()
        refs = '<LibraryReference Include="../producer/Pub.lib" />'
        if header:
            (folder / "own.h").write_text(API, encoding="utf-8")
            refs += ref("own.h")
        (folder / "Main.ns").write_text("int Main()\n{\n    " + body + "\n}\n", encoding="utf-8")
        project(folder, "C", ["Main.ns"], refs)
        expect_run(f"{name} O{opt}", folder, "C", opt)
    if opt == "0":
        folder = base / "05c-conflicto"
        folder.mkdir()
        (folder / "own.h").write_text("typedef enum Color { Red = 1, Green = 3, Blue = 4 } Color;\n",
                                      encoding="utf-8")
        (folder / "Main.ns").write_text("int Main() { Color c = Pub.Bridge.Next(Red); return (int)c; }\n",
                                        encoding="utf-8")
        project(folder, "C", ["Main.ns"], '<LibraryReference Include="../producer/Pub.lib" />' + ref("own.h"))
        expect_reject("rechazo 05c-conflicto", nsc_build(folder, "C", opt, "exe", "out/C.exe"),
                      "C/C++ type alias conflicts with a native type 'Color'")


for opt in opts:
    two_headers(opt)
    precompiled(opt)
print("observación terminada" if observe else "Identidad y consumo de enum C verificados:", root)
