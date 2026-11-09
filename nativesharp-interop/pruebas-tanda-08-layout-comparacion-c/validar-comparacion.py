"""T08-020-02 to T08-020-04: layout and access comparison against Clang.

02a/02b) Natural and packed structs and unions, measured with `sizeof`, `alignof` and `offsetof`.
02c) Anonymous struct and union members (natural and `pack(1)`), and tagged records nested in a struct.
02d) Arrays of records (natural and packed elements) and flexible array members of scalars and records.
03c) `volatile` fields, aligned and in a `pack(1)` struct, in the Microsoft and ISO modes: the loads and stores of
     NSC (direct and from a precompiled template consumed without the header) match Clang with `-fms-volatile` and
     `-fno-ms-volatile` in width, ordering and alignment.
04a-d) A consumer without the header measures the published struct, union, enum and typedef in its own code, declares
     and casts pointers to them, and reads and writes their members, cross-checked by C.
The values come from Clang. Layout and accesses run in O0-O3 on the direct path and on the metadata-only path.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-layout-comparacion-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef struct Inner { short s; long long ll; } Inner;
typedef union Num { int i; double d; char bytes[12]; } Num;
typedef enum Color { Red = 1, Blue = 300000 } Color;
typedef struct Anon { char tag; union { int i; float f; }; struct { short a; short b; }; double tail; } Anon;
typedef struct Outer { char c; Inner inr; Num u; int last; } Outer;
typedef Outer OuterAlias;
#pragma pack(push, 1)
typedef struct PAnon { char tag; union { int pi; double pd; }; struct { char x; int y; }; } PAnon;
typedef union PNum { char c; long long ll; } PNum;
typedef struct PHolder { char c; PNum n; Inner inr; } PHolder;
#pragma pack(pop)
typedef struct Rows { short h; Inner rows[3]; PAnon packed[2]; } Rows;
typedef struct Flex { short n; int items[]; } Flex;
typedef struct FlexRec { char c; Inner rows[]; } FlexRec;
Flex *flex_new(void);
FlexRec *flexrec_new(void);
int layout(int which);
int color_value(Color c);
"""
TYPES = ["Inner", "Num", "Color", "Anon", "Outer", "OuterAlias", "PAnon", "PNum", "PHolder", "Rows", "Flex",
         "FlexRec"]
OFFSETS = [("Anon", "i"), ("Anon", "f"), ("Anon", "a"), ("Anon", "b"), ("Anon", "tail"), ("Outer", "inr"),
           ("Outer", "u"), ("Outer", "last"), ("OuterAlias", "last"), ("PAnon", "pi"), ("PAnon", "pd"),
           ("PAnon", "x"), ("PAnon", "y"), ("PHolder", "n"), ("PHolder", "inr"), ("Rows", "rows"),
           ("Rows", "packed"), ("Flex", "items"), ("FlexRec", "rows"), ("Num", "bytes")]
LAYOUT = [(f"sizeof({t})", f"sizeof({t})") for t in TYPES] + \
    [(f"_Alignof({t})", f"alignof({t})") for t in TYPES] + \
    [(f"offsetof({t}, {f})", f"offsetof({t}, {f})") for t, f in OFFSETS]
# (record, access path, NativeSharp type, C type, value); flexible records are allocated by C.
ACCESSES = [
    ("Anon", "i", "int", "int", "-123456"), ("Anon", "f", "float", "float", "2.5f"),
    ("Anon", "b", "short", "short", "-77"), ("Anon", "tail", "double", "double", "6.75"),
    ("Outer", "inr.ll", "long", "long long", "-5000000001"), ("Outer", "u.d", "double", "double", "-0.125"),
    ("Outer", "u.bytes[11]", "sbyte", "char", "42"), ("Outer", "last", "int", "int", "777"),
    ("OuterAlias", "inr.s", "short", "short", "-31000"),
    ("PAnon", "pd", "double", "double", "1.75"), ("PAnon", "y", "int", "int", "-424242"),
    ("PHolder", "n.ll", "long", "long long", "7000000003"), ("PHolder", "inr.ll", "long", "long long", "-9"),
    ("Rows", "rows[2].ll", "long", "long long", "8000000005"), ("Rows", "packed[1].y", "int", "int", "31337"),
    ("Rows", "packed[1].pd", "double", "double", "-3.5"),
    ("Flex", "items[3]", "int", "int", "99999"), ("FlexRec", "rows[1].ll", "long", "long long", "-6000000007"),
]
FLEX = {"Flex": "flex_new", "FlexRec": "flexrec_new"}

SOURCE = '#include <stddef.h>\n#include <stdlib.h>\n#include "api.h"\n' + \
    "static long long flex_storage[16];\nstatic long long flexrec_storage[16];\n" + \
    "Flex *flex_new(void) { return (Flex *)flex_storage; }\n" + \
    "FlexRec *flexrec_new(void) { return (FlexRec *)flexrec_storage; }\n" + \
    "int color_value(Color c) { return (int)c; }\n" + \
    "int layout(int which)\n{\n    switch (which) {\n" + \
    "".join(f"    case {k}: return (int){c};\n" for k, (c, _) in enumerate(LAYOUT)) + \
    "    }\n    return -1;\n}\n" + \
    "".join(f"{ct} cget_{k}(const {r} *p) {{ return p->{a}; }}\n"
            f"void cset_{k}({r} *p, {ct} v) {{ p->{a} = v; }}\n" for k, (r, a, _, ct, _) in enumerate(ACCESSES))
HEADER_FULL = HEADER + "".join(f"{ct} cget_{k}(const {r} *p);\nvoid cset_{k}({r} *p, {ct} v);\n"
                               for k, (r, a, _, ct, _) in enumerate(ACCESSES))
REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="clayout.lib" />'


def ns_literal(value, nstype):
    return value + "L" if nstype == "long" and abs(int(value)) > 2147483647 else value


def layout_body(source):
    return "".join(f"    if ((int){ns} != {source}({k})) return {k + 1};\n" for k, (_, ns) in enumerate(LAYOUT))


def access_body(c):
    # `c` prefixes the C entry points: empty on the direct path, the producer's wrappers on the metadata path.
    lines = []
    for k, (r, a, nt, _, value) in enumerate(ACCESSES):
        code = 100 + k
        v = ns_literal(value, nt)
        if r in FLEX:
            obj, ptr = f"{r}* o = {c}{FLEX[r]}();", "o"
            member = f"o->{a}"
        else:
            obj, ptr = f"{r} o;", "&o"
            member = f"o.{a}"
        lines.append(f"    {{ {obj} {member} = ({nt}){v}; if ({c}cget_{k}({ptr}) != ({nt}){v}) return {code};\n"
                     f"      {c}cset_{k}({ptr}, ({nt})1); if ({member} != ({nt})1) return {code}; }}\n")
    return "".join(lines)


EXTRA_BODY = """    // 04b and 04d: pointer declarations and casts, enum and typedef published by the producer.
    Outer whole;
    whole.last = 5;
    void* raw = &whole;
    Outer* back = (Outer*)raw;
    OuterAlias* alias = (OuterAlias*)raw;
    Num* num = (Num*)&whole.u;
    num->i = 11;
    if (back->last != 5 || alias->last != 5 || whole.u.i != 11) return 90;
    Color c = Blue;
    if (CPREFIXcolor_value(c) != 300000 || (int)sizeof(c) != 4) return 91;
"""


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder):
    (folder / "api.h").write_text(HEADER_FULL, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:clayout.lib", "api.obj"], folder)


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


def wrappers():
    # The producer publishes every record through these signatures and forwards to C.
    text = "    public static int Layout(int which) { return layout(which); }\n"
    text += "    public static int color_value(Color c) { return global_color(c); }\n"
    text += "    public static Flex* flex_new() { return global_flex(); }\n"
    text += "    public static FlexRec* flexrec_new() { return global_flexrec(); }\n"
    for k, (r, _, nt, _, _) in enumerate(ACCESSES):
        text += (f"    public static {nt} cget_{k}({r}* p) {{ return global_get_{k}(p); }}\n"
                 f"    public static void cset_{k}({r}* p, {nt} v) {{ global_set_{k}(p, v); }}\n")
    return text


def bridge():
    # Namespace functions with distinct names reach the C declarations hidden by the wrappers' member names.
    text = "Flex* global_flex() { return flex_new(); }\nFlexRec* global_flexrec() { return flexrec_new(); }\n"
    text += "int global_color(Color c) { return color_value(c); }\n"
    for k, (r, _, nt, _, _) in enumerate(ACCESSES):
        text += (f"{nt} global_get_{k}({r}* p) {{ return cget_{k}(p); }}\n"
                 f"void global_set_{k}({r}* p, {nt} v) {{ cset_{k}(p, v); }}\n")
    return text


def paths(opt):
    direct = root / f"directo-O{opt}"
    direct.mkdir()
    c_library(direct)
    (direct / "Main.ns").write_text("int Main()\n{\n" + layout_body("layout") + access_body("") +
                                    EXTRA_BODY.replace("CPREFIX", "") + "    return 0;\n}\n", encoding="utf-8")
    project(direct, "Direct", "Main.ns", REF)
    expect_run(f"directo O{opt}", direct, "Direct", opt)
    base = root / f"metadata-O{opt}"
    producer = base / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "P.ns").write_text("namespace L {\n" + bridge() + "public class C {\n" + wrappers() +
                                   "    public static OuterAlias* Alias(OuterAlias* p) { return p; }\n"
                                   "    public static Num* AsNum(Num* p) { return p; }\n} }\n", encoding="utf-8")
    project(producer, "LayoutPub", "P.ns", REF, library=True)
    result = build(producer, "LayoutPub", opt, "lib", "LayoutPub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (producer / "api.h").unlink()
    (base / "C.ns").write_text("int Main()\n{\n" + layout_body("L.C.Layout") + access_body("L.C.") +
                               EXTRA_BODY.replace("CPREFIX", "L.C.") + "    return 0;\n}\n", encoding="utf-8")
    project(base, "C", "C.ns", '<LibraryReference Include="producer/LayoutPub.lib" />')
    expect_run(f"solo metadata O{opt}", base, "C", opt)


VOLATILE_H = """typedef struct VA { char c; volatile int v; volatile long long w; } VA;
#pragma pack(push, 1)
typedef struct VP { char c; volatile int v; volatile long long w; } VP;
#pragma pack(pop)
"""
VOLATILE_BODY = "a->v = 1; a->w = 2; b->v = 3; b->w = 4; return a->v + (int)a->w + b->v + (int)b->w;"
ACCESS = re.compile(r"\b(load|store)( atomic)? volatile (i\d+)\b.*?(?: (acquire|release|seq_cst|monotonic))?, "
                    r"align (\d+)")


def descriptors(text, marker):
    # Only the accesses of the function under comparison: after inlining into a caller with local objects, the
    # optimizer may promote a packed local and raise its alignment, which says nothing about the field access.
    found = set()
    inside = False
    for line in text.splitlines():
        if line.startswith("define "):
            inside = marker in line
        elif line.startswith("}"):
            inside = False
        match = ACCESS.search(line) if inside else None
        if match:
            found.add((match.group(1), match.group(3), match.group(4) or "-", match.group(5)))
    return sorted(found)


def volatile_ir():
    folder = root / "volatile"
    folder.mkdir()
    (folder / "api.h").write_text(VOLATILE_H, encoding="utf-8")
    (folder / "c.c").write_text('#include "api.h"\nint crun(VA *a, VP *b) { ' + VOLATILE_BODY + " }\n",
                                encoding="utf-8")
    for mode, flag in (("Microsoft", "-fms-volatile"), ("ISO", "-fno-ms-volatile")):
        run([clang, "-S", "-emit-llvm", "--target=x86_64-pc-windows-msvc", "-std=c11", "-O1", flag, "c.c", "-o",
             f"c-{mode}.ll"], folder)
        expected = descriptors((folder / f"c-{mode}.ll").read_text(encoding="utf-8"), "@crun(")
        ref = f'<CppLibraryReference Header="api.h" Language="C" Standard="c11" VolatileMode="{mode}" />'
        direct = folder / f"directo-{mode}"
        direct.mkdir()
        (direct / "api.h").write_text(VOLATILE_H, encoding="utf-8")
        (direct / "M.ns").write_text("namespace V { public class R {\n    public static int Run(VA* a, VP* b) { " +
                                     VOLATILE_BODY + " }\n} }\nint Main() { return 0; }\n", encoding="utf-8")
        project(direct, "M", "M.ns", ref)
        got = {}
        result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm-opt", "M.nsproj", "-O1"],
                     direct, check=False)
        got["directo"] = (result.returncode, descriptors(result.stdout, "__Run"))
        producer = folder / f"productor-{mode}"
        producer.mkdir()
        (producer / "api.h").write_text(VOLATILE_H, encoding="utf-8")
        (producer / "P.ns").write_text("namespace V { public class R {\n    public static VA* A(VA* p) { return p; }\n"
                                       "    public static VP* B(VP* p) { return p; }\n"
                                       "    public static int Run<T>(T unused, VA* a, VP* b) { " + VOLATILE_BODY +
                                       " }\n} }\n", encoding="utf-8")
        project(producer, "VPub", "P.ns", ref, library=True)
        result = build(producer, "VPub", "1", "lib", "VPub.lib")
        if result.returncode:
            raise AssertionError(f"productor volatile {mode}:\n{result.stdout}")
        (producer / "api.h").unlink()
        consumer = folder / f"consumidor-{mode}"
        consumer.mkdir()
        (consumer / "C.ns").write_text("int Main() { VA a; VP b; return V.R.Run<int>(0, &a, &b) == 10 ? 0 : 1; }\n",
                                       encoding="utf-8")
        project(consumer, "C", "C.ns", f'<LibraryReference Include="../productor-{mode}/VPub.lib" />')
        result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm-opt", "C.nsproj", "-O1"],
                     consumer, check=False)
        got["plantilla sin encabezado"] = (result.returncode, descriptors(result.stdout, "__Run__g1"))
        for label, (code, found) in got.items():
            if observe:
                print("volatile", mode, label, code, found, "clang", expected, flush=True)
                continue
            if code or found != expected:
                raise AssertionError(f"volatile {mode} {label}: NSC {found}, Clang {expected}")
            print("volatile", mode, label, "igual a Clang:", found, flush=True)


volatile_ir()
for opt in opts:
    paths(opt)
print("observación terminada" if observe else "Layout y accesos comparados con Clang:", root)
