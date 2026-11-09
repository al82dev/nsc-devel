"""T08-010-04d: storing C bit fields keeps the preserve mask inside its allocation unit.

NSC built the preserve mask of a bit-field store as a 64-bit complement and handed it to LLVM with the narrow
type of the allocation unit; the stray high bits made a non-canonical constant and the backend stopped at -O1
on `b.c = 127; b.d = 0;` (a `MOV8mi` with an `i8` constant operand). The probe writes the extreme values of
fields in 8, 16, 32 and 64-bit allocation units next to zeroed and saturated neighbours, and C checks every
bit pattern. It runs directly with the header and through a precompiled template consumed without the header,
O0-O3.
The same probe covers fields whose smallest window starting at their first byte would cross the end of the
record (`unsigned hi : 24` after one byte of bit fields, `unsigned long long c : 50` at bit 13): they were not
imported ("has no compatible member named") and are now read through their naturally aligned allocation unit.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-mascara-bits-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

HEADER = """typedef struct B8 { unsigned char lo : 1; unsigned char c : 7; } B8;
typedef struct B16 { unsigned short lo : 3; unsigned short c : 12; short d : 1; } B16;
typedef struct B32 { unsigned lo : 5; unsigned : 0; unsigned c : 7; int d : 1; unsigned hi : 24; } B32;
typedef struct B64 { unsigned long long lo : 13; unsigned long long c : 50; long long d : 1; } B64;
int check8(const B8 *b, int lo, int c);
int check16(const B16 *b, int lo, int c, int d);
int check32(const B32 *b, unsigned lo, unsigned c, int d, unsigned hi);
int check64(const B64 *b, unsigned long long lo, unsigned long long c, long long d);
"""
SOURCE = """#include "api.h"
int check8(const B8 *b, int lo, int c) { return b->lo == lo && b->c == c; }
int check16(const B16 *b, int lo, int c, int d) { return b->lo == lo && b->c == c && b->d == d; }
int check32(const B32 *b, unsigned lo, unsigned c, int d, unsigned hi)
{ return b->lo == lo && b->c == c && b->d == d && b->hi == hi; }
int check64(const B64 *b, unsigned long long lo, unsigned long long c, long long d)
{ return b->lo == lo && b->c == c && b->d == d; }
"""
BODY = """    B8 a;
    a.c = 127; a.lo = 0;
    if (check8(&a, 0, 127) != 1) return 1;
    a.lo = 1; a.c = 0;
    if (check8(&a, 1, 0) != 1) return 2;
    B16 b;
    b.c = 4095; b.d = 0; b.lo = 0;
    if (check16(&b, 0, 4095, 0) != 1) return 3;
    b.d = -1; b.c = 0; b.lo = 7;
    if (check16(&b, 7, 0, -1) != 1) return 4;
    B32 c;
    c.c = 127; c.d = 0; c.lo = 0; c.hi = 0;
    if (check32(&c, 0, 127, 0, 0) != 1) return 5;
    c.hi = 16777215; c.c = 0; c.d = -1; c.lo = 31;
    if (check32(&c, 31, 0, -1, 16777215) != 1) return 6;
    B64 d;
    d.c = 1125899906842623UL; d.d = 0; d.lo = 0;
    if (check64(&d, 0, 1125899906842623UL, 0) != 1) return 7;
    d.d = -1; d.c = 0; d.lo = 8191;
    if (check64(&d, 8191, 0, -1) != 1) return 8;
    return 0;
"""


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder):
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "bits.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "bits.c", "-o", "bits.obj"], folder)
    run([lib, "/nologo", "/out:bits.lib", "bits.obj"], folder)


def project(folder, name, source, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + refs + "</ItemGroup></Project>",
        encoding="utf-8")


def build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def expect_run(label, folder, name, opt):
    result = build(folder, name, opt, "exe", f"out/{name}.exe")
    code = run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode \
        if result.returncode == 0 else None
    if result.returncode or code:
        raise AssertionError(f"{label}: build {result.returncode}, salida {code}:\n{result.stdout}")
    print(label, "correcto", flush=True)


REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="bits.lib" />'
for opt in opts:
    direct = root / f"directo-O{opt}"
    direct.mkdir()
    c_library(direct)
    (direct / "Main.ns").write_text("int Main()\n{\n" + BODY + "}\n", encoding="utf-8")
    project(direct, "Direct", "Main.ns", REF)
    expect_run(f"directo O{opt}", direct, "Direct", opt)
    producer = root / f"precompilado-O{opt}" / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "P.ns").write_text("namespace Bits { public class Store { public static int Run<T>(T unused)\n{\n" +
                                   BODY + "} } }\n", encoding="utf-8")
    project(producer, "BitsPub", "P.ns", REF, library=True)
    result = build(producer, "BitsPub", opt, "lib", "BitsPub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (producer / "api.h").unlink()
    consumer = producer.parent
    (consumer / "C.ns").write_text("int Main() { return Bits.Store.Run<int>(0); }\n", encoding="utf-8")
    project(consumer, "C", "C.ns", '<LibraryReference Include="producer/BitsPub.lib" />')
    expect_run(f"precompilado sin encabezado O{opt}", consumer, "C", opt)
print("Máscaras de campos de bits verificadas:", root)
