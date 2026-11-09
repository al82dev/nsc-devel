"""T08-009-03: values and operations of C enums against Clang.

a) Negative values and the limits of the underlying type: an `int` enum with INT_MIN/-1/INT_MAX, enums with a
   fixed underlying type (`unsigned int`, `unsigned char`, `long long`), and an enum with 0xFFFFFFFF and no fixed
   type, which stays `int` (-1) on the MSVC target exactly as in Clang.
b) Conversions between enums, `int`, `unsigned int` and wider integers, explicit and implicit.
c) Mixed comparisons with signed and unsigned operands: an operand of `int` enum type against `unsigned int`
   follows C's usual arithmetic conversions (`unsigned int`), like the reverse case already did; enumerator
   constants are `int`, and `int` against `unsigned int` keeps NativeSharp's rule (decision T08-009-03c1).
d) Arithmetic, shifts and bitwise operations, including compound assignment to an enum object.
The same body runs in a Clang control, directly with the header and through a template of a precompiled
producer consumed without the header, O0-O3. Rejections run in O0.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-enum-valores-", dir=r"A:\nsc-interop-build"))
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

HEADER = """#include <limits.h>
typedef enum Range { RMin = INT_MIN, RNeg = -1, RZero = 0, RMax = INT_MAX } Range;
typedef enum UEnum : unsigned int { UZero = 0, UBig = 0xFFFFFFFFu } UEnum;
typedef enum MEnum { MZero = 0, MBig = 0xFFFFFFFFu } MEnum;
typedef enum Small : unsigned char { S0 = 0, S200 = 200, S255 = 255 } Small;
typedef enum Wide : long long { WNeg = -1, WBig = 0x100000000LL } Wide;
typedef enum Flags { FA = 1, FB = 2, FC = 4, FAll = 7 } Flags;
int range_value(Range r);
Range next_range(Range r);
unsigned int ubig_value(UEnum u);
int small_value(Small s);
Small small_make(int v);
long long wide_value(Wide w);
int flags_value(Flags f);
Flags flags_make(int v);
int sizes(void);
"""

SOURCE = """#include "api.h"
int range_value(Range r) { return (int)r; }
Range next_range(Range r) { return (Range)((unsigned int)r + 1u); }
unsigned int ubig_value(UEnum u) { return (unsigned int)u; }
int small_value(Small s) { return s; }
Small small_make(int v) { return (Small)v; }
long long wide_value(Wide w) { return w; }
int flags_value(Flags f) { return (int)f; }
Flags flags_make(int v) { return (Flags)v; }
int sizes(void) { return (int)(sizeof(Range) * 1000 + sizeof(UEnum) * 100 + sizeof(Small) * 10 + sizeof(Wide)); }
"""

BODY = """    if ((int)RMin != -2147483647 - 1 || (int)RMax != 2147483647 || (int)RNeg != -1) return 1;
    if (range_value(RMin) != -2147483647 - 1 || range_value(RMax) != 2147483647 || range_value(RNeg) != -1) return 2;
    if (next_range(RNeg) != RZero || next_range(RMax) != RMin) return 3;
    if (sizes() != 4418 || (int)sizeof(Range) != 4 || (int)sizeof(Small) != 1 || (int)sizeof(Wide) != 8) return 4;
    if ((uint)UBig != 4294967295 || ubig_value(UBig) != 4294967295 || small_value(S255) != 255) return 5;
    if (wide_value(WBig) != 4294967296 || (long)WNeg != -1 || (long)WBig != 4294967296) return 6;
    Range r = (Range)(-1);
    int i = RNeg;
    if (r != RNeg || i != -1 || (uint)RNeg != 4294967295 || (long)RNeg != -1) return 7;
    if ((ulong)RNeg != 18446744073709551615 || (Range)(long)RMax != RMax || (int)(Range)5 != 5) return 8;
    Small s = small_make(300);
    if ((int)s != 44 || s != (Small)44 || small_value((Small)S200) != 200) return 9;
    if (r < 1u || !(r < 1) || !(r == 4294967295u) || !(r + 0u > 5u) || 1u > r) return 10;
    UEnum u = UBig;
    if (r / 2u != 2147483647u || !(u > 0) || u > -1 || -1 < u) return 11;
    if (!(s < 256) || !(S255 > S200) || !(WNeg < 0) || WNeg >= 0) return 12;
    if ((FA | FC) != 5 || (FAll & ~FB) != 5 || (FAll ^ FA) != 6 || (FB << 2) != 8 || (FC >> 1) != FB) return 13;
    if (FB * 3 != 6 || FAll - FC != 3 || FAll % FC != 3 || -FA != -1 || ~FA != -2 || FA + FB + FC != FAll) return 14;
    if (flags_value(flags_make(FA | FB)) != 3 || flags_value(FAll) != 7) return 15;
    Flags f = FA;
    f |= FB;
    f &= ~FA;
    f ^= FC;
    f <<= 1;
    f += 1;
    if ((int)f != 13 || flags_value(f) != 13) return 16;
    f -= FA;
    f >>= 2;
    if (f != FC - 1) return 17;
    // Without a fixed type the MSVC target keeps `int`: 0xFFFFFFFF becomes -1, as Clang computes.
    if ((int)MBig != -1 || MBig > 0 || (uint)MBig != 4294967295) return 18;
    // Enumerators are `int` constants, as in C; `int` against `unsigned int` keeps NativeSharp's rule
    // (decision T08-009-03c1), so only an enum-typed operand compares as `unsigned int`.
    if (!(NSONLY(RNeg < 1u && UBig > -1))) return 19;
    return 0;
"""

REJECTIONS = [
    ("enum-a-bool", "bool b = FA; return b ? 0 : 1;", "cannot initialize local 'b' of type 'builtin:bool'"),
    ("enum-a-float-compuesto", "Flags f = FA; f += 1.5; return (int)f;",
     "compound assignment requires compatible numeric operands"),
]


def ns_spelling(text):
    return text.replace("NSONLY(", "(")


def c_spelling(text):
    text = text.replace("NSONLY(", "1 || (")
    for a, b in (("(uint)", "(unsigned int)"), ("(ulong)", "(unsigned long long)"), ("(long)", "(long long)"),
                 ("4294967296", "4294967296LL"), ("18446744073709551615", "18446744073709551615ULL"),
                 (" 4294967295)", " 4294967295u)"), (" 4294967295 ", " 4294967295u ")):
        text = text.replace(a, b)
    return text


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder):
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:cenum.lib", "api.obj"], folder)


HEADER_REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="cenum.lib" />'


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
        "</ItemGroup></Project>", encoding="utf-8")


def nsc_build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def report(label, result, code):
    if observe:
        print(label, result.returncode, code, [l.split(": error: ")[-1] for l in result.stdout.splitlines()
                                               if "error" in l][:4], flush=True)
        return
    if result.returncode or code:
        raise AssertionError(f"{label}: build {result.returncode}, salida {code}:\n{result.stdout}")
    print(label, "correcto", flush=True)


def build_and_run(label, folder, name, opt):
    result = nsc_build(folder, name, opt, "exe", f"out/{name}.exe")
    code = run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode \
        if result.returncode == 0 else None
    report(label, result, code)


def control():
    folder = root / "control"
    folder.mkdir()
    c_library(folder)
    (folder / "control.c").write_text(
        '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n'
        f"static int run(void)\n{{\n{c_spelling(BODY)}}}\n"
        "void mainCRTStartup(void) { ExitProcess(run()); }\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"], folder)
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:control.exe",
         "control.obj", "cenum.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control Clang: paso {code}")
    print("control Clang correcto", flush=True)


def direct(opt):
    folder = root / f"directo-O{opt}"
    folder.mkdir()
    c_library(folder)
    (folder / "Main.ns").write_text("int Main()\n{\n" + ns_spelling(BODY) + "}\n", encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], HEADER_REF)
    build_and_run(f"directo O{opt}", folder, "Direct", opt)


def precompiled(opt):
    folder = root / f"precompilado-O{opt}"
    producer = folder / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "Producer.ns").write_text(
        "namespace Enums { public class Bridge { public static int Run<T>(T unused)\n{\n" + ns_spelling(BODY) +
        "} } }\n",
        encoding="utf-8")
    project(producer, "EnumBridge", ["Producer.ns"], HEADER_REF, library=True)
    result = nsc_build(producer, "EnumBridge", opt, "lib", "EnumBridge.lib")
    if result.returncode:
        report(f"productor O{opt}", result, None)
        return
    (producer / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Enums.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/EnumBridge.lib" />')
    build_and_run(f"precompilado O{opt}", folder, "Consumer", opt)


def rejections():
    for name, statements, text in REJECTIONS:
        folder = root / "rechazos" / name
        folder.mkdir(parents=True)
        c_library(folder)
        (folder / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
        project(folder, "Negative", ["Main.ns"], HEADER_REF)
        result = nsc_build(folder, "Negative", "0", "exe", "out/Negative.exe")
        if observe:
            print("rechazo", name, result.returncode, [l.split(": error: ")[-1] for l in result.stdout.splitlines()
                                                        if "error" in l][:2], flush=True)
            continue
        if result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
            raise AssertionError(f"rechazo {name}: se esperaba «{text}»:\n{result.stdout}")
        print("rechazo", name, "correcto", flush=True)


control()
for opt in opts:
    direct(opt)
    precompiled(opt)
rejections()
print("observación terminada" if observe else "Valores y operaciones de enum C verificados:", root)
