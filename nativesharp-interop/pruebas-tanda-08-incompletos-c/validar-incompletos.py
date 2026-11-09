"""T08-013-02 and T08-013-03: incomplete C types.

02a) A struct declared ahead and defined later in the same header is complete: used by value and by pointer.
02b) A union declared ahead in one header and defined in another visible header of the same project.
02c) An opaque struct published by a precompiled producer and the same struct seen through the producer's header
     in the consumer are one type: the pointer from the library goes through the header's C functions. A separate
     copy of the forward declaration is not reconciled: see the pending decision T08-013-02c1.
02d) Two incompatible definitions with the same identity in one project are rejected when used.
03a) `Opaque*` and `Opaque**` passed, returned and stored in locals, fields and globals; also through a template
     of a precompiled producer consumed without the header.
03b) `sizeof`, a variable by value and member access of an incomplete type are rejected.
03c) Arithmetic and indexing of `Opaque*` are rejected; on `Opaque**` they are accepted.
Positive cases run in O0-O3; rejections in O0. C checks every value.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-incompletos-", dir=r"A:\nsc-interop-build"))
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

OPAQUE_H = """typedef struct Opaque Opaque;
typedef struct Holder { Opaque *p; Opaque **pp; } Holder;
extern Opaque *g_last;
Opaque *op_new(int v);
int op_get(const Opaque *o);
void op_pair(Opaque **out, int first, int second);
Opaque *op_select(Opaque **items, int index);
Opaque **op_storage(void);
int holder_sum(const Holder *h);
"""
OPAQUE_C = """#include "opaque.h"
struct Opaque { int v; };
static struct Opaque pool[8];
static int used;
Opaque *g_last;
Opaque *op_new(int v) { pool[used].v = v; g_last = &pool[used]; return &pool[used++]; }
int op_get(const Opaque *o) { return o ? o->v : -1; }
void op_pair(Opaque **out, int first, int second) { out[0] = op_new(first); out[1] = op_new(second); }
Opaque *op_select(Opaque **items, int index) { return items[index]; }
static Opaque *slots[4];
Opaque **op_storage(void) { return slots; }
int holder_sum(const Holder *h) { return op_get(h->p) + op_get(h->pp[0]) + op_get(h->pp[1]); }
"""
OPAQUE_BODY = """    Opaque* a = op_new(4);
    if (op_get(a) != 4 || g_last != a) return 1;
    Opaque** pp = op_storage();
    op_pair(pp, 10, 20);
    Opaque** second = pp + 1;
    if (op_get(pp[1]) != 20 || op_get(*second) != 20 || op_select(pp, 0) != pp[0]) return 2;
    pp[2] = a;
    if (op_select(pp, 2) != a) return 2;
    Holder h;
    h.p = a;
    h.pp = pp;
    if (holder_sum(&h) != 34 || op_get(h.pp[0]) != 10) return 3;
    g_last = null;
    if (op_get(g_last) != -1) return 4;
    g_last = h.pp[1];
    return op_get(g_last) == 20 ? 0 : 5;
"""
SAME_HEADER = ("struct Node;\nint node_sum(const struct Node *n);\n"
               "struct Node { int v; struct Node *next; };\n",
               '#include "a.h"\nint node_sum(const struct Node *n) { int s = 0; while (n) { s += n->v; n = n->next; }'
               ' return s; }\n',
               "Node second; second.v = 5; second.next = null; Node first; first.v = 2; first.next = &second;\n"
               "    return node_sum(&first) == 7 && (int)sizeof(Node) == 16 ? 0 : 1;")
POSITIVE = [
    ("02a-mismo-encabezado", {"a.h": SAME_HEADER[0]}, {"a.c": SAME_HEADER[1]}, SAME_HEADER[2]),
    ("02b-otro-encabezado",
     {"a.h": "union Val;\nint val_int(const union Val *v);\n",
      "b.h": '#include "a.h"\nunion Val { int i; float f; };\n'},
     {"a.c": '#include "b.h"\nint val_int(const union Val *v) { return v->i; }\n'},
     "Val v; v.f = 1.0f; return val_int(&v) == 1065353216 && (int)sizeof(Val) == 4 ? 0 : 1;"),
    ("03a-opaco", {"opaque.h": OPAQUE_H}, {"opaque.c": OPAQUE_C}, OPAQUE_BODY),
]
REJECTIONS = [
    ("03b-sizeof", "return (int)sizeof(Opaque);", "is incomplete and has no object layout"),
    ("03b-valor", "Opaque o; return 0;", "cannot be created by value because its type is incomplete"),
    ("03b-miembro", "Opaque* a = op_new(1); return a->v;", "has no compatible member named 'v'"),
    ("03c-aritmetica", "Opaque* a = op_new(1); Opaque* b = a + 1; return op_get(b);",
     "pointer arithmetic requires a complete C type"),
    ("03c-indexacion", "Opaque* a = op_new(1); return op_get(&a[1]);",
     "pointer arithmetic requires a complete C type"),
]
CONFLICT = ({"a.h": "struct Thing { int a; };\nint thing_a(void);\n",
             "b.h": "struct Thing { double a; };\nint thing_b(void);\n"},
            "Thing t; t.a = 1; return 0;")


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder, name, sources):
    objects = []
    for source, text in sources.items():
        (folder / source).write_text(text, encoding="utf-8")
        run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", source, "-o", source + ".obj"], folder)
        objects.append(source + ".obj")
    run([lib, "/nologo", f"/out:{name}.lib", *objects], folder)


def refs(headers, library):
    names = list(headers)
    return "".join(f'<CppLibraryReference Header="{h}" Language="C" Standard="c11"' +
                   (f' Library="{library}.lib"' if library and i == 0 else "") + " />"
                   for i, h in enumerate(names))


def project(folder, name, source, references, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + references + "</ItemGroup></Project>",
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
    for name, headers, sources, body in POSITIVE:
        folder = root / f"O{opt}" / name
        folder.mkdir(parents=True)
        for header, text in headers.items():
            (folder / header).write_text(text, encoding="utf-8")
        c_library(folder, "cinc", sources)
        (folder / "Main.ns").write_text("int Main()\n{\n    " + body + "\n}\n", encoding="utf-8")
        project(folder, "P", "Main.ns", refs(headers, "cinc"))
        expect_run(f"{name} O{opt}", folder, "P", opt)


def precompiled(opt):
    base = root / f"O{opt}" / "precompilado"
    producer = base / "producer"
    producer.mkdir(parents=True)
    (producer / "opaque.h").write_text(OPAQUE_H, encoding="utf-8")
    c_library(producer, "copaque", {"opaque.c": OPAQUE_C})
    (producer / "P.ns").write_text(
        "namespace Pub { public class Op {\n"
        "    public static Opaque* Make(int v) { return op_new(v); }\n"
        "    public static int Run<T>(T unused)\n    {\n" + OPAQUE_BODY + "    }\n} }\n", encoding="utf-8")
    project(producer, "OpPub", "P.ns", refs({"opaque.h": ""}, "copaque"), library=True)
    result = build(producer, "OpPub", opt, "lib", "OpPub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    hidden = producer / "opaque.h"
    hidden.rename(producer / "opaque.h.oculto")
    folder = base / "sin-encabezado"
    folder.mkdir()
    (folder / "C.ns").write_text("int Main() { return Pub.Op.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "C", "C.ns", '<LibraryReference Include="../producer/OpPub.lib" />')
    expect_run(f"03a-precompilado-sin-encabezado O{opt}", folder, "C", opt)
    (producer / "opaque.h.oculto").rename(hidden)
    # 02c: the consumer includes the producer's header; the opaque type from the metadata and the one from the
    # header are one type, so the library pointer reaches the header's C function and comes back.
    folder = base / "conciliado"
    folder.mkdir()
    (folder / "C.ns").write_text("int Main()\n{\n    Opaque* o = Pub.Op.Make(7);\n"
                                 "    Opaque* again = op_select(&o, 0);\n"
                                 "    return op_get(again) == 7 && again == o ? 0 : 1;\n}\n", encoding="utf-8")
    project(folder, "C", "C.ns", '<LibraryReference Include="../producer/OpPub.lib" />' +
            '<CppLibraryReference Header="../producer/opaque.h" Language="C" Standard="c11" />')
    expect_run(f"02c-conciliado O{opt}", folder, "C", opt)


def rejections():
    for name, body, text in REJECTIONS:
        folder = root / "rechazos" / name
        folder.mkdir(parents=True)
        (folder / "opaque.h").write_text(OPAQUE_H, encoding="utf-8")
        c_library(folder, "copaque", {"opaque.c": OPAQUE_C})
        (folder / "Main.ns").write_text("int Main() { " + body + " }\n", encoding="utf-8")
        project(folder, "N", "Main.ns", refs({"opaque.h": ""}, "copaque"))
        expect_reject(f"rechazo {name}", build(folder, "N", "0", "exe", "out/N.exe"), text)
    headers, body = CONFLICT
    folder = root / "rechazos" / "02d-conflicto"
    folder.mkdir(parents=True)
    for header, text in headers.items():
        (folder / header).write_text(text, encoding="utf-8")
    (folder / "Main.ns").write_text("int Main() { " + body + " }\n", encoding="utf-8")
    project(folder, "N", "Main.ns", refs(headers, None))
    expect_reject("rechazo 02d-conflicto", build(folder, "N", "0", "exe", "out/N.exe"),
                  "conflicting or unverified definitions")


for opt in opts:
    positive(opt)
    precompiled(opt)
rejections()
print("observación terminada" if observe else "Tipos C incompletos verificados:", root)
