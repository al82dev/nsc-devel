"""T08-D16: the consumer's own forward declaration of a C record published through NSMETA.

A precompiled producer publishes `Opaque` (opaque or complete) and a consumer that does not have the producer's
header declares the record itself in its own header (`typedef struct Opaque Opaque;` or `struct Opaque;`). As in C,
where an incomplete type is compatible with the record of the same tag in another translation unit, both names are
the same type: the consumer passes the producer's `Opaque*` to its own C functions and the other way round, and a
template of the producer that uses `Opaque*` is instantiated by the consumer. When the producer published the
complete record, the consumer's name is that record, with its members. Runs O0-O3 with a Clang control, plus
rejections (O0): a union against a struct of the same tag, an unrelated tag, a consumer that sees two producers
with the same opaque tag (ambiguous) and a consumer that defines the record a producer published as opaque.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-opaco-consumidor-", dir=r"A:\nsc-interop-build"))
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

# EN: the producer's C library owns the definition; the consumer's C library has its own private definition, which
#     is compatible in C because it has the same tag and members.
# ES: la biblioteca C del productor tiene la definicion; la del consumidor tiene su propia definicion privada,
#     compatible en C porque tiene la misma etiqueta y miembros.
PRODUCER_C = """#include "api.h"
#ifndef OPAQUE_DEFINED
struct Opaque { int v; };
#endif
static struct Opaque pool[4];
static int used;
struct Opaque *op_new(int v) { pool[used].v = v; return &pool[used++]; }
int op_get(const struct Opaque *o) { return o->v; }
"""
CONSUMER_C = """#include "own.h"
#ifndef OPAQUE_DEFINED
struct Opaque { int v; };
#endif
static struct Opaque mine = { 40 };
int own_get(const struct Opaque *o) { return o->v + 100; }
struct Opaque *own_new(void) { return &mine; }
"""
PRODUCER_HEADERS = {
    "opaco-typedef": "typedef struct Opaque Opaque;\nOpaque *op_new(int v);\nint op_get(const Opaque *o);\n",
    "opaco-etiqueta": "struct Opaque;\nstruct Opaque *op_new(int v);\nint op_get(const struct Opaque *o);\n",
    "completo": "#define OPAQUE_DEFINED\ntypedef struct Opaque { int v; } Opaque;\nOpaque *op_new(int v);\n"
                "int op_get(const Opaque *o);\n",
}
CONSUMER_HEADERS = {
    "typedef": "typedef struct Opaque Opaque;\nint own_get(const Opaque *o);\nOpaque *own_new(void);\n",
    "etiqueta": "struct Opaque;\nint own_get(const struct Opaque *o);\nstruct Opaque *own_new(void);\n",
}
PRODUCER_NS = """namespace Pub
{
    public class Bridge
    {
        public static Opaque* Make(int v) { return op_new(v); }
        public static int Get(Opaque* o) { return op_get(o); }
        public static int Twice<T>(T unused, Opaque* o) { Opaque* copy = o; return op_get(copy) * 2; }
    }
}
"""
BODY = """    Opaque* o = Pub.Bridge.Make(7);
    if (own_get(o) != 107 || Pub.Bridge.Get(o) != 7) return 1;
    Opaque* mine = own_new();
    if (Pub.Bridge.Get(mine) != 40 || Pub.Bridge.Twice<int>(0, mine) != 80) return 2;
    if (mine == o) return 3;
"""
# EN: (name, producer header, consumer header, extra consumer statements)
# ES: (nombre, encabezado del productor, del consumidor, sentencias adicionales del consumidor)
CASES = [
    ("opaco-typedef", "opaco-typedef", "typedef", ""),
    ("opaco-etiqueta", "opaco-etiqueta", "etiqueta", ""),
    ("opaco-mixto", "opaco-etiqueta", "typedef", ""),
    ("completo", "completo", "typedef",
     "    o->v = 9;\n    if (own_get(o) != 109 || (int)sizeof(Opaque) != 4) return 4;\n"),
]


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder, header_name, header, source, name):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / header_name).write_text(header, encoding="utf-8")
    (folder / f"{name}.c").write_text(source, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", f"{name}.c", "-o", f"{name}.obj"], folder)
    run([lib, "/nologo", f"/out:{name}.lib", f"{name}.obj"], folder)


def project(folder, name, source, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + refs + "</ItemGroup></Project>",
        encoding="utf-8")


def build(folder, name, opt, kind="exe", output=None):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output or f"out/{name}.exe"], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


def producer(folder, header, opt, name="Pub", source=PRODUCER_NS, c_source=PRODUCER_C):
    c_library(folder, "api.h", header, c_source, "capi")
    (folder / "Producer.ns").write_text(source, encoding="utf-8")
    project(folder, name, "Producer.ns",
            '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="capi.lib" />', True)
    result = build(folder, name, opt, "lib", f"{name}.lib")
    if result.returncode:
        raise AssertionError(f"productor {folder}:\n{result.stdout}")
    (folder / "api.h").unlink()


def consumer(folder, header, statements, references, source=CONSUMER_C):
    c_library(folder, "own.h", header, source, "own")
    (folder / "Main.ns").write_text("int Main()\n{\n" + statements + "    return 0;\n}\n", encoding="utf-8")
    project(folder, "Consumer", "Main.ns", references +
            '<CppLibraryReference Header="own.h" Language="C" Standard="c11" Library="own.lib" />')


# EN: Clang control: the two translation units with their own declarations link and run together.
# ES: control Clang: las dos unidades con sus propias declaraciones se enlazan y se ejecutan juntas.
for name, producer_header, consumer_header, extra in CASES:
    folder = root / "control" / name
    c_library(folder / "p", "api.h", PRODUCER_HEADERS[producer_header], PRODUCER_C, "capi")
    c_library(folder, "own.h", CONSUMER_HEADERS[consumer_header], CONSUMER_C, "own")
    checks = ("    struct Opaque *o = op_new(7);\n    if (own_get(o) != 107 || op_get(o) != 7) return 1;\n"
              "    struct Opaque *mine = own_new();\n    if (op_get(mine) != 40) return 2;\n")
    if extra:
        checks += "    o->v = 9;\n    if (own_get(o) != 109 || (int)sizeof(struct Opaque) != 4) return 4;\n"
    main_header = '#include "p/api.h"\n' if extra else '#include "own.h"\n#include "p/api.h"\n'
    if extra:
        main_header += "int own_get(const struct Opaque *o);\nstruct Opaque *own_new(void);\n"
    (folder / "Control.c").write_text(
        main_header + "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
        "static int checks(void) {\n" + checks + "    return 0;\n}\n"
        "void mainCRTStartup(void) { ExitProcess(checks()); }\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], folder)
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
         "Control.obj", "own.lib", "p/capi.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "Control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control C {name}: comprobación {code} falló")
print("control C correcto:", len(CASES), "casos", flush=True)

for opt in opts:
    for name, producer_header, consumer_header, extra in CASES:
        folder = root / f"O{opt}" / name
        producer(folder / "p", PRODUCER_HEADERS[producer_header], opt)
        consumer(folder, CONSUMER_HEADERS[consumer_header], BODY + extra,
                 '<LibraryReference Include="p/Pub.lib" />')
        result = build(folder, "Consumer", opt)
        if result.returncode:
            if observe:
                print(name, f"O{opt}", errors(result), flush=True)
                continue
            raise AssertionError(f"{name} O{opt}:\n{result.stdout}")
        code = run([folder / "out" / "Consumer.exe"], Path("A:/"), check=False).returncode
        if code:
            raise AssertionError(f"{name} O{opt}: comprobación {code} falló")
        print(name, f"O{opt}", "correcto", flush=True)

# EN: rejections. ES: rechazos.
OPAQUE_UNION = "typedef union Opaque Opaque;\nOpaque *op_new(int v);\nint op_get(const Opaque *o);\n"
OTHER = "typedef struct Other Other;\nint own_get(const Other *o);\nOther *own_new(void);\n"
OTHER_C = CONSUMER_C.replace("struct Opaque", "struct Other")
OWN_COMPLETE = ("#define OPAQUE_DEFINED\ntypedef struct Opaque { int v; } Opaque;\nint own_get(const Opaque *o);\n"
                "Opaque *own_new(void);\n")
CONFLICT = "C/C++ type alias conflicts with a native type 'Opaque'"
REJECTIONS = [
    ("union-frente-a-struct", OPAQUE_UNION, CONSUMER_HEADERS["typedef"], False, CONFLICT),
    ("etiqueta-distinta", PRODUCER_HEADERS["opaco-typedef"], OTHER, False,
     "cannot initialize local 'other' of type 'type:Consumer::Other*' from 'type:Pub::Opaque*'"),
    ("dos-productores", PRODUCER_HEADERS["opaco-typedef"], CONSUMER_HEADERS["typedef"], True, CONFLICT),
    ("consumidor-completo", PRODUCER_HEADERS["opaco-typedef"], OWN_COMPLETE, False, CONFLICT),
]
for name, producer_header, consumer_header, two_producers, expected in REJECTIONS:
    folder = root / "rechazos" / name
    producer(folder / "p", producer_header, "0", c_source=PRODUCER_C.replace(
        "struct Opaque", "union Opaque" if producer_header == OPAQUE_UNION else "struct Opaque"))
    references = '<LibraryReference Include="p/Pub.lib" />'
    statements = "    Opaque* o = Pub.Bridge.Make(7);\n    if (own_get(o) != 107) return 1;\n"
    if name == "etiqueta-distinta":
        statements = "    Opaque* o = Pub.Bridge.Make(7);\n    Other* other = o;\n    return own_get(other);\n"
    if two_producers:
        producer(folder / "q", producer_header, "0", "Pub2", PRODUCER_NS.replace("namespace Pub", "namespace Pub2"))
        references += '<LibraryReference Include="q/Pub2.lib" />'
    consumer(folder, consumer_header, statements, references,
             OTHER_C if name == "etiqueta-distinta" else CONSUMER_C)
    result = build(folder, "Consumer", "0")
    if observe:
        print(name, result.returncode, errors(result), flush=True)
        continue
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)
print("observación terminada" if observe else "Declaración opaca propia del consumidor verificada:", root)
