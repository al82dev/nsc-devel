"""T08-006-03 and T08-006-04: C integer widths, signedness, `_Bool`, conversions and ABI.

Every C integer type of the fixed target (x86_64-pc-windows-msvc) is checked as a parameter, a return value, a
struct field, a global and an array element, against Clang:
- 03a-03d: `char` (signed on this target), `signed char`, `unsigned char`, `short`, `unsigned short`, `int`,
  `unsigned int`, `long` and `unsigned long` (32 bits on this target), `long long`, `unsigned long long`;
  sizes, offsets and limits come from Clang and are compared with what NativeSharp sees.
- 03e: `_Bool` in memory (one byte, 0 or 1 written by each side) and in signatures.
- 04a: sign or zero extension of narrow parameters and returns (C widens what it receives and returns
  narrow values with the high bit set).
- 04b: promotions of narrow integers in binary arithmetic and bitwise operations, compared with the values
  that Clang computes for the same expressions; unary `-` and `~` keep the operand type in NativeSharp
  (decision T08-006-04b1), so the shared checks cast to `int` first and the current behaviour is pinned apart.
- 04c: cross storage: NativeSharp writes globals, fields and arrays and C reads them, and the reverse.
- 04d: limits and conversions between signed and unsigned types.
Positive cases run in a Clang control, directly with the header and through a template of a precompiled
producer consumed without the header, O0-O3. Rejections (O0): implicit narrowing and sign-changing
conversions that NativeSharp requires to be explicit.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-enteros-", dir=r"A:\nsc-interop-build"))
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

# (suffix, C type, NativeSharp type, size, minimum, maximum)
TYPES = [
    ("c", "char", "sbyte", 1, -128, 127),
    ("sc", "signed char", "sbyte", 1, -128, 127),
    ("uc", "unsigned char", "byte", 1, 0, 255),
    ("s", "short", "short", 2, -32768, 32767),
    ("us", "unsigned short", "ushort", 2, 0, 65535),
    ("i", "int", "int", 4, -2147483648, 2147483647),
    ("ui", "unsigned int", "uint", 4, 0, 4294967295),
    ("l", "long", "int", 4, -2147483648, 2147483647),
    ("ul", "unsigned long", "uint", 4, 0, 4294967295),
    ("ll", "long long", "long", 8, -9223372036854775808, 9223372036854775807),
    ("ull", "unsigned long long", "ulong", 8, 0, 18446744073709551615),
]


def wide(ctype):
    return "unsigned long long" if ctype.startswith("unsigned") else "long long"


def header():
    text = "typedef struct Scalars {\n" + "".join(f"    {c} f_{n};\n" for n, c, *_ in TYPES) + \
        "    _Bool f_b;\n} Scalars;\n"
    for n, c, *_ in TYPES:
        text += (f"extern {c} g_{n};\nextern {c} a_{n}[3];\n{c} echo_{n}({c} v);\n{c} next_{n}({c} v);\n"
                 f"{wide(c)} widen_{n}({c} v);\n{c} min_{n}(void);\n{c} max_{n}(void);\nint size_{n}(void);\n"
                 f"int offset_{n}(void);\nint is_max_{n}({c} v);\nint is_min_{n}({c} v);\n")
    text += ("extern _Bool g_b;\n_Bool not_b(_Bool v);\nint widen_b(_Bool v);\nint size_b(void);\n"
             "int offset_b(void);\nint size_scalars(void);\nvoid fill(Scalars *s);\nint check(const Scalars *s);\n"
             "void fill_globals(void);\nint check_globals(void);\nunsigned char raw_b(const _Bool *p);\n")
    return text


def source():
    text = '#include "api.h"\n#include <stddef.h>\n#include <limits.h>\n'
    limits = {"c": ("CHAR_MIN", "CHAR_MAX"), "sc": ("SCHAR_MIN", "SCHAR_MAX"), "uc": ("0", "UCHAR_MAX"),
              "s": ("SHRT_MIN", "SHRT_MAX"), "us": ("0", "USHRT_MAX"), "i": ("INT_MIN", "INT_MAX"),
              "ui": ("0", "UINT_MAX"), "l": ("LONG_MIN", "LONG_MAX"), "ul": ("0", "ULONG_MAX"),
              "ll": ("LLONG_MIN", "LLONG_MAX"), "ull": ("0", "ULLONG_MAX")}
    for n, c, *_ in TYPES:
        low, high = limits[n]
        text += (f"{c} g_{n};\n{c} a_{n}[3];\n{c} echo_{n}({c} v) {{ return v; }}\n"
                 f"{c} next_{n}({c} v) {{ return ({c})((unsigned long long)v + 1); }}\n"
                 f"{wide(c)} widen_{n}({c} v) {{ return ({wide(c)})v; }}\n"
                 f"{c} min_{n}(void) {{ return ({c}){low}; }}\n{c} max_{n}(void) {{ return ({c}){high}; }}\n"
                 f"int size_{n}(void) {{ return (int)sizeof({c}); }}\n"
                 f"int offset_{n}(void) {{ return (int)offsetof(Scalars, f_{n}); }}\n"
                 f"int is_max_{n}({c} v) {{ return v == ({c}){high}; }}\n"
                 f"int is_min_{n}({c} v) {{ return v == ({c}){low}; }}\n")
    text += ("_Bool g_b;\n_Bool not_b(_Bool v) { return !v; }\nint widen_b(_Bool v) { return v; }\n"
             "int size_b(void) { return (int)sizeof(_Bool); }\n"
             "int offset_b(void) { return (int)offsetof(Scalars, f_b); }\n"
             "int size_scalars(void) { return (int)sizeof(Scalars); }\n"
             "unsigned char raw_b(const _Bool *p) { return *(const unsigned char *)p; }\n")
    # fill/check use the maximum of each type, minus the index so neighbouring fields differ.
    fill = "".join(f"    s->f_{n} = ({c})(max_{n}() - {k});\n" for k, (n, c, *_) in enumerate(TYPES))
    check = " && ".join(f"s->f_{n} == ({c})(min_{n}() + {k})" for k, (n, c, *_) in enumerate(TYPES))
    text += f"void fill(Scalars *s)\n{{\n{fill}    s->f_b = 1;\n}}\n"
    text += f"int check(const Scalars *s) {{ return {check} && *(const unsigned char *)&s->f_b == 1; }}\n"
    fill_g = "".join(f"    g_{n} = max_{n}(); a_{n}[2] = min_{n}();\n" for n, *_ in TYPES)
    check_g = " && ".join(f"g_{n} == min_{n}() && a_{n}[0] == max_{n}() && a_{n}[1] == ({c})(min_{n}() + 1)"
                          for n, c, *_ in TYPES)
    text += f"void fill_globals(void)\n{{\n{fill_g}    g_b = 1;\n}}\n"
    text += f"int check_globals(void) {{ return {check_g} && *(const unsigned char *)&g_b == 1; }}\n"
    return text


def body():
    """NativeSharp body; the C control compiles it with C_SPELLING applied."""
    lines = []
    step = 0

    def check(condition):
        nonlocal step
        step += 1
        lines.append(f"    if (!({condition})) return {step};")

    check("size_scalars() == (int)sizeof(Scalars)")
    for k, (n, c, ns, size, low, high) in enumerate(TYPES):
        # 03a-03d: sizes, offsets, limits, parameters and returns.
        check(f"size_{n}() == {size} && (int)sizeof({ns}) == {size}")
        check(f"is_max_{n}(max_{n}()) == 1 && is_min_{n}(min_{n}()) == 1")
        check(f"echo_{n}(max_{n}()) == max_{n}() && echo_{n}(min_{n}()) == min_{n}()")
        # wrap-around in C after the conversion back to the narrow type
        check(f"next_{n}(max_{n}()) == min_{n}()")
        # 04a: extension of the narrow value seen by C and by NativeSharp
        signed_word = "long" if not c.startswith("unsigned") else "ulong"
        check(f"widen_{n}(min_{n}()) == ({signed_word})min_{n}() && widen_{n}(max_{n}()) == ({signed_word})max_{n}()")
        if low < 0:
            check(f"min_{n}() < 0 && ({signed_word})min_{n}() < 0")
        else:
            check(f"min_{n}() == 0 && ({signed_word})max_{n}() > 0")
    check("size_b() == 1 && (int)sizeof(bool) == 1")
    check("not_b(true) == false && not_b(false) == true && widen_b(true) == 1 && widen_b(false) == 0")
    # 03e and 04c: struct written by C and read by NativeSharp, and the reverse.
    lines.append("    Scalars s;")
    lines.append("    fill(&s);")
    for k, (n, c, ns, *_) in enumerate(TYPES):
        check(f"s.f_{n} == ({ns})(max_{n}() - ({ns}){k})")
    check("s.f_b == true")
    for k, (n, c, ns, *_) in enumerate(TYPES):
        lines.append(f"    s.f_{n} = ({ns})(min_{n}() + ({ns}){k});")
    lines.append("    s.f_b = true;")
    check("check(&s) == 1 && raw_b(&s.f_b) == 1")
    lines.append("    s.f_b = false;")
    check("raw_b(&s.f_b) == 0")
    # globals and arrays
    lines.append("    fill_globals();")
    for n, c, ns, *_ in TYPES:
        check(f"g_{n} == max_{n}() && a_{n}[2] == min_{n}()")
    check("g_b == true")
    for n, c, ns, *_ in TYPES:
        lines.append(f"    g_{n} = min_{n}(); a_{n}[0] = max_{n}(); a_{n}[1] = ({ns})(min_{n}() + ({ns})1);")
    lines.append("    g_b = true;")
    check("check_globals() == 1")
    # 04b: promotions; the expected values are what C computes for the same expressions.
    lines.append("    byte ub = max_uc();")
    lines.append("    sbyte sb = min_sc();")
    lines.append("    ushort uw = max_us();")
    lines.append("    short sw = min_s();")
    check("ub + ub == 510 && ub * ub == 65025 && ~(int)ub == -256 && (ub << 4) == 4080")
    check("sb + sb == -256 && -(int)sb == 128 && (sb >> 1) == -64 && (sb & 255) == 128")
    # NativeSharp keeps the operand type in unary - and ~ (decision T08-006-04b1); C promotes to int.
    check("NSONLY(~ub == 0 && -sb == -128 && (int)~uw == 0)")
    check("uw + 1 == 65536 && uw * 2 == 131070 && (uw >> 8) == 255")
    check("sw - 1 == -32769 && (sw ^ -1) == 32767")
    check("(ub | sb) == -1 && (ub & sw) == 0")
    # 04d: conversions between signed and unsigned types and limits
    check("(uint)min_i() == 2147483648 && (int)max_ui() == -1 && (ulong)min_ll() == 9223372036854775808")
    check("(byte)min_sc() == 128 && (sbyte)max_uc() == -1 && (ushort)min_s() == 32768 && (short)max_us() == -1")
    check("(long)max_ui() == 4294967295 && (int)max_ull() == -1 && (long)max_ull() == -1")
    check("max_ull() == 18446744073709551615 && min_ll() == -9223372036854775807 - 1")
    lines.append("    return 0;")
    return "\n".join(lines) + "\n"


def ns_spelling(text):
    return text.replace("NSONLY(", "(")


def c_spelling(text):
    text = text.replace("NSONLY(", "1 || (")
    mapping = [("(sbyte)", "(signed char)"), ("(byte)", "(unsigned char)"), ("(ushort)", "(unsigned short)"),
               ("(short)", "(short)"), ("(uint)", "(unsigned int)"), ("(ulong)", "(unsigned long long)"),
               ("(long)", "(long long)"), ("sizeof(sbyte)", "sizeof(signed char)"), ("sizeof(byte)",
               "sizeof(unsigned char)"), ("sizeof(ushort)", "sizeof(unsigned short)"), ("sizeof(uint)",
               "sizeof(unsigned int)"), ("sizeof(ulong)", "sizeof(unsigned long long)"), ("sizeof(long)",
               "sizeof(long long)"), ("sizeof(bool)", "sizeof(_Bool)"), ("    byte ", "    unsigned char "),
               ("    sbyte ", "    signed char "), ("    ushort ", "    unsigned short "), ("true", "1"),
               ("false", "0")]
    for a, b in mapping:
        text = text.replace(a, b)
    text = text.replace("18446744073709551615", "18446744073709551615ULL")
    text = text.replace("9223372036854775808", "9223372036854775808ULL")
    text = text.replace("9223372036854775807 ", "9223372036854775807LL ")
    text = text.replace("4294967295", "4294967295LL").replace("2147483648", "2147483648U")
    return text


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_library(folder):
    (folder / "api.h").write_text(header(), encoding="utf-8")
    (folder / "api.c").write_text(source(), encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:centeros.lib", "api.obj"], folder)


HEADER_REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="centeros.lib" />'


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
        f"static int run(void)\n{{\n{c_spelling(body())}}}\n"
        "void mainCRTStartup(void) { ExitProcess(run()); }\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"], folder)
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:control.exe",
         "control.obj", "centeros.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control Clang: paso {code}")
    print("control Clang correcto", flush=True)


def direct(opt):
    folder = root / f"directo-O{opt}"
    folder.mkdir()
    c_library(folder)
    (folder / "Main.ns").write_text("int Main()\n{\n" + ns_spelling(body()) + "}\n", encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], HEADER_REF)
    build_and_run(f"directo O{opt}", folder, "Direct", opt)


def precompiled(opt):
    folder = root / f"precompilado-O{opt}"
    producer = folder / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "Producer.ns").write_text(
        "namespace Integers { public class Bridge { public static int Run<T>(T unused)\n{\n" + ns_spelling(body()) +
        "} } }\n",
        encoding="utf-8")
    project(producer, "Integers", ["Producer.ns"], HEADER_REF, library=True)
    result = nsc_build(producer, "Integers", opt, "lib", "Integers.lib")
    if result.returncode:
        report(f"productor O{opt}", result, None)
        return
    (producer / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Integers.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Integers.lib" />')
    build_and_run(f"precompilado O{opt}", folder, "Consumer", opt)


REJECTIONS = [
    ("estrechar-int-a-char", "sbyte v = echo_i(1); return v;",
     "cannot initialize local 'v' of type 'builtin:sbyte' from 'builtin:int'"),
    ("signo-int-a-uint", "uint v = echo_i(1); return (int)v;",
     "cannot initialize local 'v' of type 'builtin:uint' from 'builtin:int'"),
    ("bool-desde-int", "bool v = widen_b(true); return v ? 0 : 1;",
     "cannot initialize local 'v' of type 'builtin:bool' from 'builtin:int'"),
]


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
print("observación terminada" if observe else "Enteros y _Bool C verificados:", root)
