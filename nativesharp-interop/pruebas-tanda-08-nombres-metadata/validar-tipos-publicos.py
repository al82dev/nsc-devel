"""T08-D25: a NativeSharp library publishes the C types of its interface with CppLibraryReference PublicTypes="Header".

The producer library uses none of the checked types in a public signature. Its reference declares
PublicTypes="Header", so its NSMeta C catalog carries every named C record, enum and typedef declared by the header
or by a header under the header's directory. A consumer without the header names them:
1. precompiled static library, O0-O3, and DLL product (O0): typedef alias of a record (NAlias), enum (NMood) and its
   enumerators, record from a header of the same directory (NPoint), scalar typedef (NHandle), and a record whose
   field type comes from a header outside the directory (NHolder.used); sizes and values match the C library;
2. the same consumer with the header (direct path) also passes;
3. not published: a type of the outside header that no published type needs (ExtOnly) and a reserved identifier
   (_InternalRecord) stay unknown to the consumer without the header;
4. without PublicTypes the library publishes only what its signatures use (NAlias stays unknown);
5. duplicates: two references whose headers share one identical declaration, and a typedef published both by a
   signature and by the surface, are published once and the consumer uses them;
6. conflicts with the cause: two references declaring NAlias differently (producer rejected); a consumer header
   that declares NAlias differently ("conflicting C/C++ type alias"); two libraries publishing different NMood used
   by one consumer (rejected, never one chosen); two libraries publishing the same declaration are one type since
   T08-D26 and the consumer uses it (validated in depth by pruebas-tanda-08-identidad-c-productores);
7. loader rejections: PublicTypes with another value, in an executable, and with Language C++;
8. artifact cache: a repeated build hits; removing PublicTypes from the producer misses.
T08-D25: una biblioteca NativeSharp publica los tipos C de su interfaz con PublicTypes="Header".
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-tipos-publicos-", dir=r"A:\nsc-interop-build"))
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

API_H = """#include <stddef.h>
#include "pub_types.h"
#include "../ext/ext.h"
typedef struct NRecord { char tag; int value; double real; } NRecord;
typedef NRecord NAlias;
typedef enum NMood { NSad = -1, NHappy = 4 } NMood;
typedef unsigned int NHandle;
typedef struct NHolder { ExtUsed used; short extra; } NHolder;
typedef struct _InternalRecord { int hidden; } _InternalRecord;
long long names_probe(int id);
int names_check(int value);
"""
PUB_TYPES_H = "typedef struct NPoint { int x; long long y; } NPoint;\n"
EXT_H = """typedef struct ExtUsed { int u; double w; } ExtUsed;
typedef struct ExtOnly { int e; } ExtOnly;
"""
API_C = """#include "api.h"
long long names_probe(int id)
{
    switch (id)
    {
    case 1: return (long long)sizeof(NAlias);
    case 2: return (long long)sizeof(NMood);
    case 3: return (long long)NHappy;
    case 4: return (long long)sizeof(NPoint);
    case 5: return (long long)sizeof(NHandle);
    case 6: return (long long)sizeof(NHolder);
    case 7: return (long long)offsetof(NHolder, extra);
    case 8: return (long long)NSad;
    default: return -1000;
    }
}
int names_check(int value) { return value * 2; }
"""
PRODUCER_NS = """namespace Published
{
    public struct Bridge
    {
        public static long Probe(int id) { return names_probe(id); }
        public static int Check(int value) { return names_check(value); }
    }
}
"""
CONSUMER_NS = """int Main()
{
    if (sizeof(NAlias) != (nuint)Published.Bridge.Probe(1)) return 1;
    if (sizeof(NMood) != (nuint)Published.Bridge.Probe(2)) return 2;
    if ((long)NHappy != Published.Bridge.Probe(3)) return 3;
    if (sizeof(NPoint) != (nuint)Published.Bridge.Probe(4)) return 4;
    if (sizeof(NHandle) != (nuint)Published.Bridge.Probe(5)) return 5;
    if (sizeof(NHolder) != (nuint)Published.Bridge.Probe(6)) return 6;
    if ((long)NSad != Published.Bridge.Probe(8)) return 7;
    NAlias record;
    record.value = 21;
    NPoint point;
    point.y = (long)record.value;
    NHolder holder;
    holder.used.u = Published.Bridge.Check(record.value);
    NMood mood = NHappy;
    if (mood != NHappy || holder.used.u != 42 || point.y != 21) return 8;
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


def c_library(folder, api_h=API_H, api_c=API_C):
    write(folder / "include" / "pub" / "api.h", api_h)
    write(folder / "include" / "pub" / "pub_types.h", PUB_TYPES_H)
    write(folder / "include" / "ext" / "ext.h", EXT_H)
    write(folder / "api.c", api_c)
    require([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "-Iinclude/pub", "api.c", "-o", "api.obj"],
            folder, "clang api.c")
    require([lib_tool, "/nologo", "/out:api.lib", "api.obj"], folder, "llvm-lib")


def c_dll(folder):
    require([lld, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/export:names_probe", "/export:names_check",
             "/out:api.dll", "/implib:apidll.lib", "api.obj", crt + "libcmt.lib", crt + "libvcruntime.lib",
             sdk + r"ucrt\x64\libucrt.lib", sdk + r"um\x64\kernel32.lib"], folder, "lld-link api.dll")


def project(folder, name, source, items, kind=None):
    output = f"<OutputType>{kind}</OutputType>" if kind else ""
    write(folder / f"{name}.nsproj",
          f"<Project><PropertyGroup><Name>{name}</Name>{output}</PropertyGroup><ItemGroup>"
          f'<Compile Include="{source}" />{items}</ItemGroup></Project>')


def reference(header="include/pub/api.h", extra=' PublicTypes="Header"', library="api.lib", runtime=""):
    lib = f' Library="{library}"' if library else ""
    return (f'<CppLibraryReference Header="{header}" Language="C" Standard="c11"{lib}{runtime}{extra} />')


def build(folder, name, kind, output, opt="0", cache=None):
    caching = ["--time", "--artifact-cache=" + str(cache)] if cache else ["--no-artifact-cache"]
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, *caching, "-o", output], folder)


def expect_built(result, label):
    if result.returncode:
        raise AssertionError(f"{label}:\n{result.stdout}")


def expect_rejected(result, label, text):
    if observe:
        lines = [line for line in result.stdout.splitlines() if "error" in line][:3]
        print("rechazo", label, result.returncode, lines, flush=True)
        return
    if result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"{label}: se esperaba «{text}»:\n{result.stdout}")
    print(label, "rechazado con la causa", flush=True)


def run_exe(path, label):
    code = subprocess.run([str(path)], cwd=path.parent).returncode
    if code:
        raise AssertionError(f"{label}: salida {code}")


def producer(folder, name="CTypesProducer", items=None, kind="Library", output=None, opt="0"):
    write(folder / "P.ns", PRODUCER_NS)
    project(folder, name, "P.ns", items if items is not None else reference(), kind)
    target = output or f"out/{name}.lib"
    result = build(folder, name, "lib" if kind == "Library" else "dll", target, opt)
    expect_built(result, f"productor {name} O{opt}")
    return folder / target


def hide_headers(folder):
    shutil.move(str(folder / "include"), str(folder / "include.hidden"))


def show_headers(folder):
    shutil.move(str(folder / "include.hidden"), str(folder / "include"))


def check_precompiled():
    folder = root / "precompilado"
    c_library(folder)
    consumer = folder / "consumer"
    write(consumer / "C.ns", CONSUMER_NS)
    project(consumer, "CTypesConsumer", "C.ns", '<LibraryReference Include="../out/CTypesProducer.lib" />')
    for opt in ("0", "1", "2", "3"):
        producer(folder, opt=opt)
        hide_headers(folder)
        try:
            result = build(consumer, "CTypesConsumer", "exe", f"out{opt}/C.exe", opt)
            expect_built(result, f"consumidor sin encabezado O{opt}")
            run_exe(consumer / f"out{opt}" / "C.exe", f"consumidor sin encabezado O{opt}")
        finally:
            show_headers(folder)
    print("precompilado sin encabezado: NAlias, NMood, NPoint, NHandle y NHolder en O0-O3", flush=True)
    direct = folder / "directo"
    write(direct / "C.ns", CONSUMER_NS)
    project(direct, "CTypesDirect", "C.ns", reference("../include/pub/api.h", "", "") +
            '<LibraryReference Include="../out/CTypesProducer.lib" />')
    result = build(direct, "CTypesDirect", "exe", "out/C.exe")
    expect_built(result, "consumidor con encabezado")
    run_exe(direct / "out" / "C.exe", "consumidor con encabezado")
    print("consumidor con el encabezado y la biblioteca: correcto", flush=True)
    return folder


def check_not_published(folder):
    hide_headers(folder)
    try:
        for name, body, text in (
                ("ExtOnly", "int Main() { return sizeof(ExtOnly) == (nuint)4 ? 0 : 1; }\n", "ExtOnly"),
                ("Internal", "int Main() { return sizeof(_InternalRecord) == (nuint)4 ? 0 : 1; }\n",
                 "_InternalRecord")):
            consumer = folder / f"no-{name}"
            write(consumer / "C.ns", body)
            project(consumer, f"No{name}", "C.ns", '<LibraryReference Include="../out/CTypesProducer.lib" />')
            expect_rejected(build(consumer, f"No{name}", "exe", "out/C.exe"), f"no publicado {name}",
                            f"unknown identifier '{text}'")
    finally:
        show_headers(folder)


def check_without_attribute():
    folder = root / "sin-atributo"
    c_library(folder)
    producer(folder, items=reference(extra=""))
    consumer = folder / "consumer"
    write(consumer / "C.ns", "int Main() { return sizeof(NAlias) == (nuint)16 ? 0 : 1; }\n")
    project(consumer, "NoAttribute", "C.ns", '<LibraryReference Include="../out/CTypesProducer.lib" />')
    hide_headers(folder)
    try:
        expect_rejected(build(consumer, "NoAttribute", "exe", "out/C.exe"), "sin PublicTypes",
                        "unknown identifier 'NAlias'")
    finally:
        show_headers(folder)


def check_dll():
    folder = root / "dll"
    c_library(folder)
    c_dll(folder)
    producer(folder, items=reference(library="apidll.lib", runtime=' Runtime="api.dll"'), kind="SharedLibrary",
             output="out/CTypesProducer.dll")
    consumer = folder / "consumer"
    write(consumer / "C.ns", CONSUMER_NS)
    project(consumer, "CTypesDllConsumer", "C.ns",
            '<LibraryReference Include="../out/CTypesProducer.lib" Runtime="../out/CTypesProducer.dll" />')
    hide_headers(folder)
    try:
        result = build(consumer, "CTypesDllConsumer", "exe", "out/C.exe")
        expect_built(result, "consumidor del producto DLL")
        for dll in (folder / "out").glob("*.dll"):
            shutil.copyfile(dll, consumer / "out" / dll.name)
        shutil.copyfile(folder / "api.dll", consumer / "out" / "api.dll")
        run_exe(consumer / "out" / "C.exe", "consumidor del producto DLL")
    finally:
        show_headers(folder)
    print("producto DLL: el consumidor sin encabezado usa los tipos publicados", flush=True)


def check_duplicates_and_conflicts():
    folder = root / "conflictos"
    c_library(folder)
    write(folder / "include" / "pub" / "second.h", '#include "pub_types.h"\ntypedef int NSecond;\n')
    write(folder / "include" / "pub" / "clash.h", "typedef long long NAlias;\n")
    duplicate = folder / "duplicado"
    shutil.copytree(folder / "include", duplicate / "include")
    shutil.copyfile(folder / "api.lib", duplicate / "api.lib")
    write(duplicate / "P.ns", PRODUCER_NS.replace("Published", "Duplicated").replace(
        "public static int Check", "public static NAlias* Empty() { return (NAlias*)0; }\n        public static int Check"))
    project(duplicate, "Duplicate", "P.ns", reference() + reference("include/pub/second.h", library=""), "Library")
    expect_built(build(duplicate, "Duplicate", "lib", "out/Duplicate.lib"), "productor con duplicados")
    consumer = duplicate / "consumer"
    write(consumer / "C.ns", "int Main()\n{\n    NAlias* empty = Duplicated.Bridge.Empty();\n"
          "    if (empty != (NAlias*)0) return 1;\n    NSecond second = 3;\n    NPoint point;\n"
          "    point.x = second;\n    return sizeof(NAlias) == (nuint)Duplicated.Bridge.Probe(1) && point.x == 3"
          " ? 0 : 2;\n}\n")
    project(consumer, "DuplicateConsumer", "C.ns", '<LibraryReference Include="../out/Duplicate.lib" />')
    hide_headers(duplicate)
    try:
        result = build(consumer, "DuplicateConsumer", "exe", "out/C.exe")
        expect_built(result, "consumidor de duplicados")
        run_exe(consumer / "out" / "C.exe", "consumidor de duplicados")
    finally:
        show_headers(duplicate)
    print("duplicados: NPoint (dos referencias) y NAlias (firma y superficie) publicados una vez", flush=True)
    clash = folder / "choque"
    shutil.copytree(folder / "include", clash / "include")
    shutil.copyfile(folder / "api.lib", clash / "api.lib")
    write(clash / "P.ns", PRODUCER_NS)
    project(clash, "Clash", "P.ns", reference() + reference("include/pub/clash.h", library=""), "Library")
    expect_rejected(build(clash, "Clash", "lib", "out/Clash.lib"), "productor con NAlias distinto",
                    "C type 'NAlias' has different declarations")
    library = root / "precompilado"
    consumer = folder / "consumidor-conflicto"
    write(consumer / "own.h", "typedef long long NAlias;\n")
    write(consumer / "C.ns", "int Main() { return sizeof(NAlias) == (nuint)8 ? 0 : 1; }\n")
    project(consumer, "OwnConflict", "C.ns", '<CppLibraryReference Header="own.h" Language="C" Standard="c11" />'
            '<LibraryReference Include="../../precompilado/out/CTypesProducer.lib" />')
    expect_rejected(build(consumer, "OwnConflict", "exe", "out/C.exe"), "consumidor con NAlias propio distinto",
                    "conflicting C/C++ type alias")
    other = folder / "otra"
    c_library(other, API_H.replace("NHappy = 4", "NHappy = 5"))
    write(other / "P.ns", PRODUCER_NS.replace("Published", "Other"))
    project(other, "OtherTypes", "P.ns", reference(), "Library")
    expect_built(build(other, "OtherTypes", "lib", "out/OtherTypes.lib"), "segunda biblioteca")
    same = folder / "igual"
    # EN: The same types with other C functions (same_*), so that both C libraries link into one program.
    # ES: Los mismos tipos con otras funciones C (same_*), para que las dos bibliotecas C se enlacen juntas.
    c_library(same, API_H.replace("names_", "same_"), API_C.replace("names_", "same_"))
    write(same / "P.ns", PRODUCER_NS.replace("Published", "Same").replace("names_", "same_"))
    project(same, "SameTypes", "P.ns", reference(), "Library")
    expect_built(build(same, "SameTypes", "lib", "out/SameTypes.lib"), "biblioteca con la misma declaracion")
    equal = folder / "dos-iguales"
    write(equal / "C.ns", "int Main()\n{\n    NMood mood = NHappy;\n"
          "    return sizeof(NAlias) == (nuint)Same.Bridge.Probe(1) && (long)mood == Published.Bridge.Probe(3)"
          " ? 0 : 1;\n}\n")
    project(equal, "TwoEqual", "C.ns", '<LibraryReference Include="../../precompilado/out/CTypesProducer.lib" />'
            '<LibraryReference Include="../igual/out/SameTypes.lib" />')
    hide_headers(library)
    hide_headers(same)
    try:
        # EN: T08-D26: the same declaration published by two precompiled producers is one C type.
        # ES: T08-D26: la misma declaracion publicada por dos productores precompilados es un solo tipo C.
        result = build(equal, "TwoEqual", "exe", "out/C.exe")
        expect_built(result, "dos bibliotecas con la misma declaracion")
        run_exe(equal / "out" / "C.exe", "dos bibliotecas con la misma declaracion")
        print("dos bibliotecas con la misma declaracion: un solo tipo (T08-D26)", flush=True)
    finally:
        show_headers(library)
        show_headers(same)
    two = folder / "dos-bibliotecas"
    write(two / "C.ns", "int Main() { return (int)NHappy; }\n")
    project(two, "TwoLibraries", "C.ns", '<LibraryReference Include="../../precompilado/out/CTypesProducer.lib" />'
            '<LibraryReference Include="../otra/out/OtherTypes.lib" />')
    hide_headers(library)
    hide_headers(other)
    try:
        result = build(two, "TwoLibraries", "exe", "out/C.exe")
        if observe:
            print("dos bibliotecas", result.returncode,
                  [line for line in result.stdout.splitlines() if "error" in line][:3], flush=True)
        elif result.returncode == 0 or "NHappy" not in result.stdout and "NMood" not in result.stdout:
            raise AssertionError(f"dos bibliotecas con NMood distinto: se esperaba un rechazo:\n{result.stdout}")
        else:
            print("dos bibliotecas con NMood distinto: rechazado, ninguna se elige", flush=True)
    finally:
        show_headers(library)
        show_headers(other)


def check_loader():
    folder = root / "cargador"
    c_library(folder)
    write(folder / "P.ns", PRODUCER_NS)
    cases = [
        ("Valor", reference(extra=' PublicTypes="All"'), "Library", "CppLibraryReference PublicTypes must be Header"),
        ("Ejecutable", reference(), None, "PublicTypes requires a Library or SharedLibrary project"),
        ("Cxx", reference().replace('Language="C" Standard="c11"', 'Language="C++" Standard="c++20"'), "Library",
         'PublicTypes requires Language="C"'),
    ]
    for name, items, kind, text in cases:
        project(folder, name, "P.ns", items, kind)
        expect_rejected(build(folder, name, "lib", f"out/{name}.lib"), f"cargador {name}", text)


def check_cache():
    folder = root / "cache"
    c_library(folder)
    cache = root / "artifact-cache"
    markers = []
    for attempt, items in enumerate((reference(), reference(), reference(extra=""))):
        write(folder / "P.ns", PRODUCER_NS)
        project(folder, "CachedTypes", "P.ns", items, "Library")
        result = build(folder, "CachedTypes", "lib", "out/CachedTypes.lib", "0", cache)
        expect_built(result, f"caché intento {attempt}")
        match = re.search(r"compilation-bundle-cache: (\S+)", result.stdout)
        markers.append(match.group(1) if match else "?")
    if observe:
        print("caché", markers, flush=True)
        return
    if markers[1] != "hit" or markers[2] == "hit":
        raise AssertionError(f"caché: se esperaba fallo, acierto y fallo: {markers}")
    print("caché: acierto al repetir y fallo al quitar PublicTypes", markers, flush=True)


library_root = check_precompiled()
check_not_published(library_root)
check_without_attribute()
check_dll()
check_duplicates_and_conflicts()
check_loader()
check_cache()
print("observación terminada" if observe else "T08-D25 tipos C publicos verificado:", root)
