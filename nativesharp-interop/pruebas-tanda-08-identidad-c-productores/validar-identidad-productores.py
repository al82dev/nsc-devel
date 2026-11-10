"""T08-D26: one C type published by two precompiled NativeSharp producers.

Two libraries import the same C declarations (each with its own CppLibraryReference) and publish them in NSMeta.
The consumer receives one catalogue per producer. NSC reconciles a declaration of the second producer with the one
of the first only when both have the same C identity (tag, scope, kind, target and USR) and the same declaration
contract over the whole type closure (size, alignment, every field with its type, offset and bit width, nested
records and enums, enumerator values). The name or the USR alone never reconciles two declarations.
1. static libraries, consumer without the header, O0-O3: NAlias, NRecord, NInner, NMood and the opaque NOpaque from
   both libraries are one type; a pointer returned by one library is passed to the other, by value and by pointer,
   and the enumerators are not ambiguous;
2. the same libraries and a consumer with the header, which also calls the C functions directly;
3. DLL products: both libraries as DLLs, and one DLL with one static library, consumer without and with the header;
4. the same declaration from two different header files (C compatibility across translation units) is one type;
5. conflicts, each rejected with its cause while the libraries still coexist when the consumer does not name the
   type: same name and USR with a field of another type; only a nested record differs; an enum with other values;
   an opaque record against a complete one; a consumer header that declares the record differently. The
   reconciliation is per type: when only NRecord differs, NInner and NMood are still one type and a pointer to one
   library's NRecord is not accepted by the other library.
T08-D26: un tipo C publicado por dos productores NativeSharp precompilados.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-identidad-productores-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lld = r"A:\al82dev-clang\bin\lld-link.exe"
lib_tool = r"A:\al82dev-clang\bin\llvm-lib.exe"
crt = "A:\\vc2026-runtime\\lib\\x64\\release\\"
sdk = "A:\\vc2026-runtime\\windows-sdk\\10.0.28000.0\\lib\\"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib_tool
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
observe = "--observar" in sys.argv
EXPORTS = ("names_record", "names_value", "names_inner", "names_opaque", "names_opaque_value", "names_probe")

API_H = """#include <stddef.h>
typedef struct NInner { short a; long long b; } NInner;
typedef struct NRecord { char tag; int value; double real; NInner inner; } NRecord;
typedef NRecord NAlias;
typedef enum NMood { NSad = -1, NHappy = 4 } NMood;
typedef struct NOpaque NOpaque;
NRecord* names_record(void);
int names_value(NRecord* record);
long long names_inner(NInner* inner);
NOpaque* names_opaque(void);
int names_opaque_value(NOpaque* value);
long long names_probe(int id);
"""
API_C = """#include "api.h"
struct NOpaque { int v; };
static NRecord shared_record = { 'r', 1, 2.5, { 3, 4 } };
static struct NOpaque shared_opaque = { 9 };
NRecord* names_record(void) { return &shared_record; }
int names_value(NRecord* record) { return record->value; }
long long names_inner(NInner* inner) { return inner->b; }
NOpaque* names_opaque(void) { return &shared_opaque; }
int names_opaque_value(NOpaque* value) { return value->v; }
long long names_probe(int id)
{
    switch (id)
    {
    case 1: return (long long)sizeof(NAlias);
    case 2: return (long long)offsetof(NRecord, inner);
    case 3: return (long long)NSad;
    case 4: return (long long)NHappy;
    default: return -1000;
    }
}
"""
PRODUCER_NS = """namespace Pub@
{
    public struct Bridge
    {
        public static NAlias* Record() { return names_record(); }
        public static int Value(NAlias* record) { return names_value(record); }
        public static int ByValue(NRecord record) { return record.value + (int)record.inner.b; }
        public static long Inner(NInner* inner) { return names_inner(inner); }
        public static NMood Mood(NMood value) { return value; }
        public static NOpaque* Opaque() { return names_opaque(); }
        public static int Read(NOpaque* value) { return names_opaque_value(value); }
        public static long Probe(int id) { return names_probe(id); }
    }
}
"""
CONSUMER_NS = """int Main()
{
    NAlias* a = PubA.Bridge.Record();
    NRecord* b = PubB.Bridge.Record();
    if (a != b) return 1;
    a->value = 21;
    if (PubB.Bridge.Value(a) != 21 || PubA.Bridge.Value(b) != 21) return 2;
    NRecord local = *a;
    local.value = 5;
    if (PubB.Bridge.ByValue(local) != 9 || PubA.Bridge.ByValue(*b) != 25) return 3;
    NInner* inner = &b->inner;
    if (PubA.Bridge.Inner(inner) != 4) return 4;
    NMood mood = PubB.Bridge.Mood(PubA.Bridge.Mood(NHappy));
    if (mood != NHappy || (long)NSad != PubB.Bridge.Probe(3) || (long)mood != PubA.Bridge.Probe(4)) return 5;
    if (sizeof(NAlias) != (nuint)PubB.Bridge.Probe(1) || sizeof(NRecord) != sizeof(NAlias)) return 6;
    NOpaque* opaque = PubA.Bridge.Opaque();
    if (PubB.Bridge.Read(opaque) != 9 || opaque != PubB.Bridge.Opaque()) return 7;
    return 0;
}
"""
DIRECT_CALLS = """    if (names_value(PubA.Bridge.Record()) != 21) return 8;
    if (names_opaque_value(PubB.Bridge.Opaque()) != 9) return 9;
    return 0;
}
"""


def run(args, cwd):
    return subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                          errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def require(args, cwd, label):
    result = run(args, cwd)
    if result.returncode:
        raise AssertionError(f"{label}:\n{result.stdout}")
    return result.stdout


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def c_library(folder, api_h=API_H, header="include/api.h", dll=False, api_c=API_C):
    """C library: api.lib (static) and, with dll, api.dll with its import library apidll.lib."""
    write(folder / header, api_h)
    write(folder / "api.c", api_c.replace('"api.h"', '"' + header + '"'))
    require([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"],
            folder, "clang api.c")
    require([lib_tool, "/nologo", "/out:api.lib", "api.obj"], folder, "llvm-lib")
    if dll:
        require([lld, "/nologo", "/dll", "/noentry", "/nodefaultlib", *[f"/export:{name}" for name in EXPORTS],
                 "/out:api.dll", "/implib:apidll.lib", "api.obj", crt + "libcmt.lib", crt + "libvcruntime.lib",
                 sdk + r"ucrt\x64\libucrt.lib", sdk + r"um\x64\kernel32.lib"], folder, "lld-link api.dll")


def project(folder, name, source, items, kind=None):
    output = f"<OutputType>{kind}</OutputType>" if kind else ""
    write(folder / f"{name}.nsproj",
          f"<Project><PropertyGroup><Name>{name}</Name>{output}</PropertyGroup><ItemGroup>"
          f'<Compile Include="{source}" />{items}</ItemGroup></Project>')


def reference(header, library="", runtime="", public=True):
    lib = f' Library="{library}"' if library else ""
    run_dll = f' Runtime="{runtime}"' if runtime else ""
    extra = ' PublicTypes="Header"' if public else ""
    return f'<CppLibraryReference Header="{header}" Language="C" Standard="c11"{lib}{run_dll}{extra} />'


def build(folder, name, kind, output, opt="0"):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder)


def expect_built(result, label):
    if result.returncode:
        raise AssertionError(f"{label}:\n{result.stdout}")


def expect_rejected(result, label, texts):
    lines = [line for line in result.stdout.splitlines() if "error" in line][:3]
    if observe:
        print("rechazo", label, result.returncode, lines, flush=True)
        return
    if result.returncode == 0 or not any(text in result.stdout for text in texts) or \
            "internal error" in result.stdout:
        raise AssertionError(f"{label}: se esperaba uno de {texts}:\n{result.stdout}")
    print(label, "rechazado con la causa:", lines[0] if lines else "?", flush=True)


def run_exe(path, label, dlls=()):
    for dll in dlls:
        shutil.copyfile(dll, path.parent / dll.name)
    code = subprocess.run([str(path)], cwd=path.parent).returncode
    if code:
        raise AssertionError(f"{label}: salida {code}")


def producer(folder, letter, items, kind="Library", opt="0", source=PRODUCER_NS):
    """A NativeSharp library PubX; returns the LibraryReference item for a consumer two levels below root."""
    name = f"Lib{letter}"
    write(folder / f"{name}.ns", source.replace("@", letter))
    project(folder, name, f"{name}.ns", items, kind)
    product = "lib" if kind == "Library" else "dll"
    expect_built(build(folder, name, product, f"out/{name}.{product}", opt), f"productor {name} O{opt}")
    runtime = f' Runtime="{folder / "out" / (name + ".dll")}"' if kind == "SharedLibrary" else ""
    return f'<LibraryReference Include="{folder / "out" / (name + ".lib")}"{runtime} />'


class Hidden:
    """Hides header directories so that the consumer only sees the NSMeta catalogues."""

    def __init__(self, *folders):
        self.folders = folders

    def __enter__(self):
        for folder in self.folders:
            shutil.move(str(folder), str(folder) + ".hidden")

    def __exit__(self, *args):
        for folder in self.folders:
            shutil.move(str(folder) + ".hidden", str(folder))


def consumer(folder, name, items, source=CONSUMER_NS, opt="0"):
    write(folder / "C.ns", source)
    project(folder, name, "C.ns", items)
    return build(folder, name, "exe", f"out{opt}/C.exe", opt), folder / f"out{opt}" / "C.exe"


def check_static():
    folder = root / "estatico"
    c_library(folder)
    header = folder / "include"
    for opt in ("0", "1", "2", "3"):
        refs = [producer(folder / f"lib{letter}", letter, reference(str(header / "api.h"), str(folder / "api.lib")),
                         opt=opt) for letter in "AB"]
        with Hidden(header):
            result, exe = consumer(folder / "sin-encabezado", "NoHeader", "".join(refs), opt=opt)
            expect_built(result, f"estatico sin encabezado O{opt}")
            run_exe(exe, f"estatico sin encabezado O{opt}")
    print("estatico sin encabezado, O0-O3: un solo tipo para NAlias, NRecord, NInner, NMood y NOpaque", flush=True)
    result, exe = consumer(folder / "con-encabezado", "WithHeader",
                           reference(str(header / "api.h"), public=False) + "".join(refs),
                           CONSUMER_NS.replace("    return 0;\n}\n", DIRECT_CALLS))
    expect_built(result, "estatico con encabezado")
    run_exe(exe, "estatico con encabezado")
    print("estatico con encabezado: correcto, con llamadas C directas", flush=True)
    return folder, refs


def check_dll():
    folder = root / "dll"
    c_library(folder, dll=True)
    header = folder / "include"
    api = reference(str(header / "api.h"), str(folder / "apidll.lib"), str(folder / "api.dll"))
    both = [producer(folder / f"lib{letter}", letter, api, "SharedLibrary") for letter in "AB"]
    mixed = [both[0], producer(folder / "libB-estatica", "B", api)]
    for label, refs in (("dos DLL", both), ("DLL y estatica", mixed)):
        dlls = [folder / "api.dll", *[Path(item.split('Runtime="')[1].split('"')[0])
                                      for item in refs if "Runtime=" in item]]
        tag = "dll" if refs is both else "mixto"
        with Hidden(header):
            result, exe = consumer(folder / f"{tag}-sin-encabezado", f"{tag.capitalize()}NoHeader", "".join(refs))
            expect_built(result, f"{label} sin encabezado")
            run_exe(exe, f"{label} sin encabezado", dlls)
        result, exe = consumer(folder / f"{tag}-con-encabezado", f"{tag.capitalize()}WithHeader",
                               reference(str(header / "api.h"), str(folder / "apidll.lib"), str(folder / "api.dll"),
                                         public=False) + "".join(refs),
                               CONSUMER_NS.replace("    return 0;\n}\n", DIRECT_CALLS))
        expect_built(result, f"{label} con encabezado")
        run_exe(exe, f"{label} con encabezado", dlls)
        print(f"{label}: correcto sin encabezado y con encabezado", flush=True)


def check_other_header(static_folder, static_refs):
    folder = root / "otro-encabezado"
    # EN: Other functions (copy_*) so that both C libraries link into one program.
    # ES: Otras funciones (copy_*) para que las dos bibliotecas C se enlacen en un programa.
    copy = "#define NAMES_COPY 1\n" + API_H.replace("names_", "copy_") + "int copy_unrelated(void);\n"
    c_library(folder, copy, "otro/api_copia.h", api_c=API_C.replace("names_", "copy_"))
    ref = producer(folder / "libB", "B", reference(str(folder / "otro" / "api_copia.h"), str(folder / "api.lib")),
                   source=PRODUCER_NS.replace("names_", "copy_"))
    with Hidden(static_folder / "include", folder / "otro"):
        # EN: Each C library has its own objects: the pointers are not compared.
        # ES: Cada biblioteca C tiene sus propios objetos: no se comparan los punteros.
        source = CONSUMER_NS.replace("    if (a != b) return 1;\n", "")
        source = source.replace(" || opaque != PubB.Bridge.Opaque()", "")
        source = source.replace("    a->value = 21;\n", "    a->value = 21;\n    b->value = 21;\n")
        result, exe = consumer(folder / "consumidor", "OtherHeader", static_refs[0] + ref, source)
        expect_built(result, "misma declaracion en otro encabezado")
        run_exe(exe, "misma declaracion en otro encabezado")
    print("la misma declaracion en dos encabezados distintos: un solo tipo", flush=True)


def check_conflicts(static_folder, static_refs):
    cases = [
        ("campo", API_H.replace("double real;", "long long real;"), ["conflicting C/C++ type alias 'NAlias'"],
         "int Main() { NAlias* a = PubA.Bridge.Record(); return a == (NAlias*)0 ? 1 : 0; }\n"),
        ("anidado", API_H.replace("long long b;", "double b;"), ["conflicting C/C++ type alias 'NInner'",
                                                                  "conflicting C/C++ type alias 'NRecord'"],
         "int Main() { NInner* i = &PubA.Bridge.Record()->inner; return i == (NInner*)0 ? 1 : 0; }\n"),
        ("enum", API_H.replace("NHappy = 4", "NHappy = 5"), ["ambiguous namespace state reference 'NHappy'"],
         "int Main() { return (int)NHappy; }\n"),
        ("opaco", API_H.replace("typedef struct NOpaque NOpaque;", "typedef struct NOpaque { int v; } NOpaque;"),
         ["NOpaque"], "int Main() { NOpaque* o = PubA.Bridge.Opaque(); return o == (NOpaque*)0 ? 1 : 0; }\n"),
    ]
    for tag, api_h, texts, body in cases:
        folder = root / "conflicto" / tag
        source = API_C.replace("struct NOpaque { int v; };\n", "") if tag == "opaco" else API_C
        write(folder / "include" / "api.h", api_h.replace("names_", "other_"))
        write(folder / "api.c", source.replace('"api.h"', '"include/api.h"').replace("names_", "other_"))
        require([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"],
                folder, "clang api.c")
        require([lib_tool, "/nologo", "/out:api.lib", "api.obj"], folder, "llvm-lib")
        ref = producer(folder / "libB", "B", reference(str(folder / "include" / "api.h"), str(folder / "api.lib")),
                       source=PRODUCER_NS.replace("names_", "other_"))
        with Hidden(static_folder / "include", folder / "include"):
            result, _ = consumer(folder / "nombra", "Names" + tag.capitalize(), static_refs[0] + ref, body)
            expect_rejected(result, f"conflicto {tag}: el consumidor nombra el tipo", texts)
            result, exe = consumer(folder / "coexisten", "Coexist" + tag.capitalize(), static_refs[0] + ref,
                                   "int Main() { return PubA.Bridge.Probe(1) == PubB.Bridge.Probe(1) ? 0 : 1; }\n")
            expect_built(result, f"conflicto {tag}: sin nombrar el tipo")
            run_exe(exe, f"conflicto {tag}: sin nombrar el tipo")
            if tag == "campo":
                # EN: Reconciliation is per type: NInner and NMood are equal in both libraries and stay one type,
                #     while a pointer to one library's NRecord is not the other library's NRecord.
                # ES: La conciliacion es por tipo: NInner y NMood son iguales en las dos bibliotecas y siguen siendo
                #     un tipo, mientras que un puntero al NRecord de una no es el NRecord de la otra.
                result, exe = consumer(folder / "parcial", "Partial", static_refs[0] + ref,
                                       "int Main()\n{\n    NInner inner;\n    inner.b = 4;\n"
                                       "    long b = PubB.Bridge.Inner(&inner);\n    NMood mood = NHappy;\n"
                                       "    return PubA.Bridge.Inner(&inner) == b && PubA.Bridge.Mood(mood) =="
                                       " PubB.Bridge.Mood(NHappy) ? 0 : 1;\n}\n")
                expect_built(result, "conflicto campo: NInner y NMood iguales")
                run_exe(exe, "conflicto campo: NInner y NMood iguales")
                print("conflicto campo: NInner y NMood iguales siguen siendo un tipo", flush=True)
                result, _ = consumer(folder / "cruce", "Cross", static_refs[0] + ref,
                                     "int Main() { return PubB.Bridge.Value(PubA.Bridge.Record()); }\n")
                expect_rejected(result, "conflicto campo: puntero de una biblioteca a la otra",
                                ["argument", "convert", "no overload", "matches"])
    print("conflictos: rechazados al nombrar el tipo; las bibliotecas coexisten si no se nombra", flush=True)
    folder = root / "conflicto" / "encabezado-consumidor"
    write(folder / "own.h", "typedef struct NRecord { int other; } NRecord;\ntypedef NRecord NAlias;\n")
    result, _ = consumer(folder, "OwnHeader", '<CppLibraryReference Header="own.h" Language="C" Standard="c11" />' +
                         "".join(static_refs), "int Main() { NAlias* a = PubA.Bridge.Record(); return 0; }\n")
    expect_rejected(result, "encabezado propio del consumidor distinto", ["NAlias", "NRecord"])


static_folder, static_refs = check_static()
check_dll()
check_other_header(static_folder, static_refs)
check_conflicts(static_folder, static_refs)
print("observación terminada" if observe else "T08-D26 identidad de tipos C entre productores verificada:", root)
