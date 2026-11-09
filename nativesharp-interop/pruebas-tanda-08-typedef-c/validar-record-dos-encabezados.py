"""T08-D09: a C struct (and union) defined equivalently in two headers of one project, used by value.

Two `CppLibraryReference`s bring the same record:
- "identico": two headers with the same definition text (two separate translation units in C);
- "comun": one common header with include guards, included by both headers.

C11 6.2.7 makes those definitions compatible across translation units, and Clang gives them the same layout and
ODR hash. NativeSharp used them only by pointer ("cannot be created by value until object lifetime is
implemented"). Now they have value layout: locals, copies, a global, a field of another record, by-value
arguments and returns to both libraries' functions, `sizeof`/`alignof`, directly and through a template of a
precompiled producer consumed without the headers, O0-O3, checked against a Clang control.

A struct with a flexible array member duplicated in both headers is also usable by value (its fixed part), as with
one definition. Rejections (O0): two headers whose definitions differ (another member type, another member name,
another member order) stay conflicting when the record is used.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-record-dos-encabezados-", dir=r"A:\nsc-interop-build"))
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

RECORDS = ("typedef struct Pair { int a; long long b; } Pair;\n"
           "typedef union Num { int i; float f; } Num;\n")
A_DECLS = ("int a_sum(Pair p);\nPair a_make(int a, long long b);\nint a_num(Num n);\nextern Pair g_pair;\n"
           "typedef struct Outer { Pair inner; int tag; } Outer;\nint a_outer(Outer o);\n")
B_DECLS = "long long b_diff(Pair p);\nvoid b_scale(Pair *p, int k);\nNum b_num(int i);\n"
LAYOUTS = {
    "identico": {
        "a.h": "#pragma once\n" + RECORDS + A_DECLS,
        "b.h": "#pragma once\n" + RECORDS + B_DECLS,
    },
    "comun": {
        "pair.h": "#ifndef PAIR_H\n#define PAIR_H\n" + RECORDS + "#endif\n",
        "a.h": '#pragma once\n#include "pair.h"\n' + A_DECLS,
        "b.h": '#pragma once\n#include "pair.h"\n' + B_DECLS,
    },
}
SOURCES = {
    "ca": '#include "a.h"\nint a_sum(Pair p) { return p.a + (int)(p.b / 1000000000LL); }\n'
          "Pair a_make(int a, long long b) { Pair p = {a, b}; return p; }\n"
          "int a_num(Num n) { return n.i; }\nPair g_pair = {1, 2};\n"
          "int a_outer(Outer o) { return o.inner.a + o.tag; }\n",
    "cb": '#include "b.h"\nlong long b_diff(Pair p) { return p.b - p.a; }\n'
          "void b_scale(Pair *p, int k) { p->a *= k; p->b *= k; }\n"
          "Num b_num(int i) { Num n; n.i = i; return n; }\n",
}
BODY = """    Pair p;
    p.a = 3;
    p.b = 4000000000L;
    if (a_sum(p) != 7 || b_diff(p) != 3999999997L) return 1;
    Pair q = a_make(5, 6);
    b_scale(&q, 2);
    if (q.a != 10 || q.b != 12 || b_diff(q) != 2) return 2;
    Pair copy = q;
    copy.a = 100;
    if (q.a != 10 || a_sum(copy) != 100) return 3;
    Pair r = a_make(7, 8000000000L);
    r = p;
    if (a_sum(r) != 7 || b_diff(a_make(7, 8000000000L)) != 7999999993L) return 4;
    Num n = b_num(41);
    if (a_num(n) != 41) return 5;
    Num m;
    m.i = 9;
    if (a_num(m) != 9) return 6;
    if (sizeof(Pair) != 16 || alignof(Pair) != 8 || sizeof(Num) != 4) return 7;
    if (g_pair.a != 1 || b_diff(g_pair) != 1) return 8;
    g_pair = p;
    if (a_sum(g_pair) != 7) return 9;
    Outer o;
    o.inner = q;
    o.tag = 2;
    if (a_outer(o) != 12 || o.inner.b != 12 || sizeof(Outer) != 24) return 10;
    return 0;
"""
C_BODY = (BODY.replace("4000000000L", "4000000000LL").replace("3999999997L", "3999999997LL")
          .replace("8000000000L", "8000000000LL").replace("7999999993L", "7999999993LL")
          .replace("alignof(Pair)", "_Alignof(Pair)"))


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def fixture(folder, headers, sources=SOURCES):
    folder.mkdir(parents=True, exist_ok=True)
    for name, text in headers.items():
        (folder / name).write_text(text, encoding="utf-8")
    for name, text in sources.items():
        (folder / f"{name}.c").write_text(text, encoding="utf-8")
        run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", f"{name}.c", "-o", f"{name}.obj"], folder)
        run([lib, "/nologo", f"/out:{name}.lib", f"{name}.obj"], folder)


REFERENCES = ('<CppLibraryReference Header="a.h" Language="C" Standard="c11" Library="ca.lib" />'
              '<CppLibraryReference Header="b.h" Language="C" Standard="c11" Library="cb.lib" />')


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
        "</ItemGroup></Project>", encoding="utf-8")


def build(folder, name, opt):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", f"{name}.nsproj", "-O" + opt,
                "--no-artifact-cache", "-o", f"out/{name}.exe"], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


def build_and_run(folder, name, opt):
    result = build(folder, name, opt)
    if result.returncode:
        if observe:
            print(folder.name, errors(result), flush=True)
            return None
        raise AssertionError(f"{folder}: compilación:\n{result.stdout}")
    return run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode


for layout, headers in LAYOUTS.items():
    control = root / layout / "control"
    fixture(control, headers)
    # EN: one C translation unit cannot include two different headers that both define Pair (C11 forbids the
    #     redefinition), so the identical-text control declares the b.h functions itself.
    # ES: una unidad C no puede incluir dos encabezados distintos que definan Pair (C11 prohíbe la redefinición),
    #     así que el control de texto idéntico declara por sí mismo las funciones de b.h.
    includes = '#include "a.h"\n' + (B_DECLS if layout == "identico" else '#include "b.h"\n')
    (control / "Control.c").write_text(
        includes + "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\nint _fltused = 0;\n"
        "void *memset(void *d, int c, unsigned long long n)\n"
        "{ unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }\n"
        "static int checks(void) {\n" + C_BODY + "}\nvoid mainCRTStartup(void) { ExitProcess(checks()); }\n",
        encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], control)
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
         "Control.obj", "ca.lib", "cb.lib", sdk / "um/x64/kernel32.lib"], control)
    code = run([control / "Control.exe"], control, check=False).returncode
    if code:
        raise AssertionError(f"control C {layout}: comprobación {code} falló")
    print("control C", layout, "correcto", flush=True)

for opt in opts:
    for layout, headers in LAYOUTS.items():
        folder = root / layout / f"directo-O{opt}"
        fixture(folder, headers)
        (folder / "Main.ns").write_text("int Main() {\n" + BODY + "}\n", encoding="utf-8")
        project(folder, "Direct", ["Main.ns"], REFERENCES)
        code = build_and_run(folder, "Direct", opt)
        if code:
            raise AssertionError(f"{layout} directo O{opt}: comprobación {code} falló")
        print(layout, "directo", f"O{opt}", "correcto" if code == 0 else "observado", flush=True)
        folder = root / layout / f"precompilado-O{opt}"
        fixture(folder / "producer", headers)
        (folder / "producer" / "Producer.ns").write_text(
            "namespace Records { public class Bridge { public static int Run<T>(T unused) {\n" + BODY +
            "} } }\n", encoding="utf-8")
        project(folder / "producer", "Records", ["Producer.ns"], REFERENCES, library=True)
        result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Records.nsproj",
                      "-O" + opt, "--no-artifact-cache", "-o", "Records.lib"], folder / "producer", check=False)
        if result.returncode:
            if observe:
                print("productor", layout, errors(result), flush=True)
                continue
            raise AssertionError(f"productor {layout}:\n" + result.stdout)
        for name in headers:
            (folder / "producer" / name).unlink()
        (folder / "Consumer.ns").write_text("int Main() { return Records.Bridge.Run<int>(0); }\n", encoding="utf-8")
        project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Records.lib" />')
        code = build_and_run(folder, "Consumer", opt)
        if code:
            raise AssertionError(f"{layout} precompilado O{opt}: comprobación {code} falló")
        print(layout, "precompilado", f"O{opt}", "correcto" if code == 0 else "observado", flush=True)

# EN: a struct with a flexible array member, duplicated in both headers: like one definition, a local has the fixed
#     part only (C11 6.7.2.1) and the record keeps its indirect passing.
# ES: un struct con miembro de arreglo flexible, duplicado en los dos encabezados: como con una definicion, un local
#     tiene solo la parte fija (C11 6.7.2.1) y el record conserva su paso indirecto.
FLEX = "typedef struct Flex { int n; int items[]; } Flex;\n"
FLEX_HEADERS = {"a.h": "#pragma once\n" + FLEX + "int a_n(Flex *f);\n",
                "b.h": "#pragma once\n" + FLEX + "int b_n(Flex *f);\n"}
FLEX_SOURCES = {"ca": '#include "a.h"\nint a_n(Flex *f) { return f->n; }\n',
                "cb": '#include "b.h"\nint b_n(Flex *f) { return f->n * 2; }\n'}
for opt in opts:
    folder = root / "flexible-dos" / f"directo-O{opt}"
    fixture(folder, FLEX_HEADERS, FLEX_SOURCES)
    (folder / "Main.ns").write_text("int Main() { Flex f; f.n = 3; if (sizeof(Flex) != 4) return 1; "
                                    "return a_n(&f) == 3 && b_n(&f) == 6 ? 0 : 2; }\n", encoding="utf-8")
    project(folder, "Flex", ["Main.ns"], REFERENCES)
    code = build_and_run(folder, "Flex", opt)
    if code:
        raise AssertionError(f"flexible-dos O{opt}: comprobación {code} falló")
    print("flexible-dos", f"O{opt}", "correcto" if code == 0 else "observado", flush=True)

CONFLICTS = {
    "tipo": "typedef struct Pair { int a; int b; } Pair;\n",
    "nombre": "typedef struct Pair { int a; long long c; } Pair;\n",
    "orden": "typedef struct Pair { long long b; int a; } Pair;\n",
}
REJECTIONS = []
for name, other in CONFLICTS.items():
    REJECTIONS.append((f"conflicto-{name}", {
        "a.h": "#pragma once\ntypedef struct Pair { int a; long long b; } Pair;\nint a_sum(Pair p);\n",
        "b.h": "#pragma once\n" + other + "long long b_diff(Pair p);\n",
    }, {"ca": '#include "a.h"\nint a_sum(Pair p) { return p.a; }\n',
        "cb": '#include "b.h"\nlong long b_diff(Pair p) { return p.a; }\n'},
        "Pair p; p.a = 1; return a_sum(p);", "conflicting"))
for name, headers, sources, statements, expected in REJECTIONS:
    folder = root / "rechazos" / name
    fixture(folder, headers, sources)
    (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
    project(folder, "Negative", ["Main.ns"], REFERENCES)
    result = build(folder, "Negative", "0")
    if observe:
        print(name, result.returncode, errors(result), flush=True)
        continue
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)
print("observación terminada" if observe else "Record C definido en dos encabezados, por valor, verificado:", root)
