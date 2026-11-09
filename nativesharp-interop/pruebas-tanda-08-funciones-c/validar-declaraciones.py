"""T08-003-04: declarations and redeclarations of C functions.

a) A forward declaration and the definition with compatible prototypes: repeated declarations, a typedef'd
   parameter, top-level `const`, array and function parameters adjusted to pointers, a declaration without
   prototype completed by a later one, and a `static inline` forward declaration followed by its body.
b) Incompatible redeclarations: in one header (Clang's own error), across two headers of one project, and
   across two precompiled producers whose templates call the function.
c) Variadic functions and calls without prototype with default argument promotions.
d) `static inline` functions of different headers: each keeps its body and its own static helper, directly
   and through templates of two precompiled producers consumed without headers.
Positive cases run in O0-O3 against a Clang control; rejections run in O0. `--observar` prints outcomes.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-funciones-declaraciones-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef int MyInt;
typedef struct Pair { int a; int b; } Pair;
int fwd_plain(int value);
int fwd_plain(int value);
int fwd_typedef(MyInt value);
int fwd_typedef(int value);
int fwd_const(const int value);
int fwd_const(int value);
int fwd_array(int values[]);
int fwd_array(int *values);
int fwd_array3(int values[3]);
int fwd_array3(int *values);
int fwd_func(int (*f)(int));
int fwd_func(int f(int));
int fwd_composite();
int fwd_composite(int a, long long b);
static inline int inline_fwd(int value);
static inline int inline_fwd(int value) { return value * 3; }
int twice(int value);
int np_promote();
int va_sum(int count, ...);
int va_mixed(int count, ...);
"""
SOURCE = """#include "api.h"
int fwd_plain(int value) { return value + 1; }
int fwd_typedef(int value) { return value + 2; }
int fwd_const(const int value) { return value + 3; }
int fwd_array(int *values) { return values[0] + values[1]; }
int fwd_array3(int values[3]) { return values[2]; }
int fwd_func(int (*f)(int)) { return f(10); }
int fwd_composite(int a, long long b) { return a + (int)(b / 1000000000LL); }
int twice(int value) { return value * 2; }
int np_promote(a, b, c, d, e) int a; double b; int c; int d; int *e;
{ return a == -3 && b == 1.5 && c == -7 && d == 65000 && *e == 9; }
int va_sum(int count, ...)
{
    __builtin_va_list ap; __builtin_va_start(ap, count);
    int sum = 0;
    for (int i = 0; i < count; ++i) sum += __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    return sum;
}
int va_mixed(int count, ...)
{
    __builtin_va_list ap; __builtin_va_start(ap, count);
    int a = __builtin_va_arg(ap, int); double b = __builtin_va_arg(ap, double);
    long long c = __builtin_va_arg(ap, long long); int *d = __builtin_va_arg(ap, int *);
    Pair e = __builtin_va_arg(ap, Pair);
    __builtin_va_end(ap);
    return count == 5 && a == -2 && b == 2.25 && c == -5000000000LL && *d == 9 && e.a == 4 && e.b == 5;
}
"""
# (NativeSharp expression, C expression): both must be true.
CHECKS = [
    ("fwd_plain(4) == 5", "fwd_plain(4) == 5"),
    ("fwd_typedef(4) == 6", "fwd_typedef(4) == 6"),
    ("fwd_const(4) == 7", "fwd_const(4) == 7"),
    ("fwd_array(&values[0]) == 3", "fwd_array(values) == 3"),
    ("fwd_array3(&values[0]) == 7", "fwd_array3(values) == 7"),
    ("fwd_func(twice) == 20", "fwd_func(twice) == 20"),
    ("fwd_composite(5, 3000000000L) == 8", "fwd_composite(5, 3000000000LL) == 8"),
    ("inline_fwd(5) == 15", "inline_fwd(5) == 15"),
    ("np_promote((sbyte)-3, 1.5f, (short)-7, (ushort)65000, &nine) == 1",
     "np_promote((signed char)-3, 1.5f, (short)-7, (unsigned short)65000, &nine) == 1"),
    ("va_sum(5, (sbyte)1, (short)2, true, (ushort)3, (byte)4) == 11",
     "va_sum(5, (signed char)1, (short)2, (_Bool)1, (unsigned short)3, (unsigned char)4) == 11"),
    ("va_mixed(5, (short)-2, 2.25f, -5000000000L, &nine, pair) == 1",
     "va_mixed(5, (short)-2, 2.25f, -5000000000LL, &nine, pair) == 1"),
]
NS_SETUP = ("int[3] values; values[0] = 1; values[1] = 2; values[2] = 7; int nine = 9; "
            "Pair pair; pair.a = 4; pair.b = 5;")
C_SETUP = "int values[3] = {1, 2, 7}; int nine = 9; Pair pair = {4, 5};"


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def body(checks):
    return "".join(f"    if (!({expression})) return {index};\n" for index, expression in enumerate(checks, 1))


def fixture(folder):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "-Wno-deprecated-non-prototype", "api.c",
         "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:api.lib", "api.obj"], folder)


def project(folder, name, sources, references, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + references +
        "</ItemGroup></Project>", encoding="utf-8")


def compile_exe(folder, name, opt):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", f"{name}.nsproj", "-O" + opt,
                "--no-artifact-cache", "-o", f"out/{name}.exe"], folder, check=False)


def execute(folder, name):
    return run([folder / "out" / f"{name}.exe"], Path("A:/"), check=False).returncode


def control():
    folder = root / "control"
    fixture(folder)
    (folder / "Control.c").write_text(
        '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n'
        "int _fltused = 0;\nstatic int checks(void) {\n    " + C_SETUP + "\n" + body([c for _, c in CHECKS]) +
        "    return 0;\n}\nvoid mainCRTStartup(void) { ExitProcess(checks()); }\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "-Wno-deprecated-non-prototype",
         "Control.c", "-o", "Control.obj"], folder)
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
         "Control.obj", "api.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "Control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control C: comprobación {code} falló")
    print("control C correcto", flush=True)


def report(name, opt, result, code=None):
    lines = [line.split(": error: ")[-1][:170] for line in result.stdout.splitlines() if "error" in line][:3]
    print(name, f"O{opt}", "build", result.returncode, "run", code, lines, flush=True)


def direct(opt):
    folder = root / f"directo-O{opt}"
    fixture(folder)
    (folder / "Main.ns").write_text("int Main() {\n    " + NS_SETUP + "\n" + body([n for n, _ in CHECKS]) +
                                    "    return 0;\n}\n", encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], '<CppLibraryReference Header="api.h" Language="C" Standard="c11" '
            'Library="api.lib" />')
    result = compile_exe(folder, "Direct", opt)
    code = execute(folder, "Direct") if result.returncode == 0 else None
    if observe:
        return report("directo", opt, result, code)
    if result.returncode or code:
        raise AssertionError(f"directo O{opt}: build {result.returncode}, comprobación {code}:\n{result.stdout}")
    print("directo", f"O{opt}", "correcto", flush=True)


def precompiled(opt):
    folder = root / f"precompilado-O{opt}"
    fixture(folder / "producer")
    (folder / "producer" / "Producer.ns").write_text(
        "namespace Declarations { public class Bridge { public static int Run<T>(T unused) {\n    " + NS_SETUP +
        "\n" + body([n for n, _ in CHECKS]) + "    return 0; } } }\n", encoding="utf-8")
    project(folder / "producer", "Declarations", ["Producer.ns"],
            '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="api.lib" />', library=True)
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Declarations.nsproj",
                  "-O" + opt, "--no-artifact-cache", "-o", "Declarations.lib"], folder / "producer", check=False)
    if result.returncode:
        if observe:
            return report("precompilado-productor", opt, result)
        raise AssertionError(f"precompilado O{opt}: productor:\n{result.stdout}")
    (folder / "producer" / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Declarations.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/Declarations.lib" />')
    result = compile_exe(folder, "Consumer", opt)
    code = execute(folder, "Consumer") if result.returncode == 0 else None
    if observe:
        return report("precompilado", opt, result, code)
    if result.returncode or code:
        raise AssertionError(f"precompilado O{opt}: build {result.returncode}, comprobación {code}:\n{result.stdout}")
    print("precompilado", f"O{opt}", "correcto", flush=True)


# b) Incompatible redeclarations: (name, header X, header Y or None, NativeSharp body, expected text).
REJECTIONS = [
    ("mismo-encabezado-retorno", "int bad(int value);\nlong bad(int value);\n", None, "return bad(1);",
     "conflicting types for 'bad'"),
    ("mismo-encabezado-parametro", "int bad(int value);\nint bad(double value);\n", None, "return bad(1);",
     "conflicting types for 'bad'"),
    ("mismo-encabezado-aridad", "int bad(int value);\nint bad(int value, int other);\n", None, "return bad(1);",
     "conflicting types for 'bad'"),
    ("dos-encabezados-retorno", "int split(int value);\n", "long split(int value);\n", "return split(1);",
     "conflicting declarations for one Clang USR"),
    ("dos-encabezados-parametro", "int split(int value);\n", "int split(double value);\n", "return split(1);",
     "conflicting declarations for one Clang USR"),
    ("dos-encabezados-variadica", "int split(int value, ...);\n", "int split(int value);\n", "return split(1);",
     "conflicting declarations for one Clang USR"),
    ("aridad-compuesta", "int composite();\nint composite(int a, int b);\n", None, "return composite(1);",
     "no overload of namespace function 'composite' accepts 1 argument(s)"),
    # c) A NativeSharp struct has no C type: neither a variadic nor an unprototyped slot accepts it.
    ("variadica-struct-nativesharp", "int va(int count, ...);\n", None,
     "Local l = new Local(); return va(1, l);", "no viable overload of namespace function 'va'"),
    ("sin-prototipo-struct-nativesharp", "int np();\n", None, "Local l = new Local(); return np(l);",
     "no viable overload of namespace function 'np'"),
]


def rejection(name, header_x, header_y, statements, expected):
    folder = root / "rechazos" / name
    folder.mkdir(parents=True)
    (folder / "x.h").write_text(header_x, encoding="utf-8")
    references = '<CppLibraryReference Header="x.h" Language="C" Standard="c11" />'
    if header_y:
        (folder / "y.h").write_text(header_y, encoding="utf-8")
        references += '<CppLibraryReference Header="y.h" Language="C" Standard="c11" />'
    (folder / "Main.ns").write_text("struct Local { int value; }\nint Main() { " + statements + " }\n",
                                    encoding="utf-8")
    project(folder, "Negative", ["Main.ns"], references)
    result = compile_exe(folder, "Negative", "0")
    if observe:
        return report("rechazo-" + name, "0", result)
    if result.returncode == 0 or expected not in result.stdout or "internal error" in result.stdout or \
            "lowering failed" in result.stdout:
        raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
    print("rechazo", name, "correcto", flush=True)


def two_producers(name, header_a, header_b, opt, expected):
    """b) and d) through templates of two precompiled producers consumed without headers."""
    folder = root / f"{name}-O{opt}"
    sources = {"a": header_a, "b": header_b}
    for side, header in sources.items():
        producer = folder / side
        producer.mkdir(parents=True)
        (producer / "same.h").write_text(header["h"], encoding="utf-8")
        (producer / "same.c").write_text('#include "same.h"\n' + header["c"], encoding="utf-8")
        run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "same.c", "-o", "same.obj"], producer)
        run([lib, "/nologo", f"/out:same{side}.lib", "same.obj"], producer)
        (producer / "Producer.ns").write_text(
            f"namespace Inline{side.upper()} {{ public class Bridge {{ public static int Read<T>(T unused) "
            f"{{ return {header['call']}; }} }} }}\n", encoding="utf-8")
        project(producer, f"InlineProducer{side.upper()}", ["Producer.ns"],
                f'<CppLibraryReference Header="same.h" Language="C" Standard="c11" Library="same{side}.lib" />',
                library=True)
        run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib",
             f"InlineProducer{side.upper()}.nsproj", "-O" + opt, "--no-artifact-cache", "-o",
             f"InlineProducer{side.upper()}.lib"], producer)
        (producer / "same.h").unlink()
    (folder / "Consumer.ns").write_text(
        "int Main() { return InlineA.Bridge.Read<int>(0) * 1000 + InlineB.Bridge.Read<int>(0); }\n",
        encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="a/InlineProducerA.lib" />'
            '<LibraryReference Include="b/InlineProducerB.lib" />')
    result = compile_exe(folder, "Consumer", opt)
    code = execute(folder, "Consumer") if result.returncode == 0 else None
    if observe:
        return report(name, opt, result, code)
    if expected is None:
        if result.returncode == 0 or "conflicting C function declarations" not in result.stdout:
            raise AssertionError(f"{name}: se esperaba un rechazo:\n{result.stdout}")
        print(name, "rechazado", flush=True)
    elif result.returncode or code != expected:
        raise AssertionError(f"{name} O{opt}: se esperaba {expected}, build {result.returncode}, salida {code}:\n"
                             f"{result.stdout}")
    else:
        print(name, f"O{opt}", "correcto", flush=True)


INLINE_A = {"h": "static inline int helper(int v) { return v + 1; }\n"
                 "static inline int outer(int v) { return helper(v) * 10; }\nint plain_a(int v);\n",
            "c": "int plain_a(int v) { return v; }\n", "call": "outer(2) + plain_a(0)"}
INLINE_B = {"h": "static inline int helper(int v) { return v + 2; }\n"
                 "static inline int outer(int v) { return helper(v) * 10; }\nint plain_b(int v);\n",
            "c": "int plain_b(int v) { return v; }\n", "call": "outer(2) + plain_b(0)"}
SPLIT_A = {"h": "int split(int v);\n", "c": "int split(int v) { return v; }\n", "call": "split(3)"}
SPLIT_B = {"h": "long split(int v);\n", "c": "long split(int v) { return v; }\n", "call": "(int)split(3)"}


def inline_direct(opt):
    """d) Two headers of one project: each static inline keeps its body and its own static helper."""
    folder = root / f"inline-directo-O{opt}"
    folder.mkdir(parents=True)
    (folder / "x.h").write_text("static inline int helper_x(int v) { return v + 1; }\n"
                                "static inline int outer_x(int v) { return helper_x(v) * 10; }\n"
                                "static inline int counter_x(void) { static int calls; return ++calls; }\n",
                                encoding="utf-8")
    (folder / "y.h").write_text("static inline int helper_y(int v) { return v + 2; }\n"
                                "static inline int outer_y(int v) { return helper_y(v) * 10; }\n"
                                "static inline int counter_y(void) { static int calls = 100; return ++calls; }\n",
                                encoding="utf-8")
    (folder / "Main.ns").write_text(
        "int Main() {\n    if (outer_x(2) != 30 || outer_y(2) != 40) return 1;\n"
        "    if (counter_x() != 1 || counter_x() != 2 || counter_y() != 101) return 2;\n    return 0;\n}\n",
        encoding="utf-8")
    project(folder, "InlineDirect", ["Main.ns"], '<CppLibraryReference Header="x.h" Language="C" Standard="c11" />'
            '<CppLibraryReference Header="y.h" Language="C" Standard="c11" />')
    result = compile_exe(folder, "InlineDirect", opt)
    code = execute(folder, "InlineDirect") if result.returncode == 0 else None
    if observe:
        return report("inline-directo", opt, result, code)
    if result.returncode or code:
        raise AssertionError(f"inline-directo O{opt}: build {result.returncode}, comprobación {code}:\n"
                             f"{result.stdout}")
    print("inline-directo", f"O{opt}", "correcto", flush=True)


control()
for opt in opts:
    direct(opt)
    precompiled(opt)
    inline_direct(opt)
    # Each producer's template keeps its own `outer` and its own `helper`: 30 * 1000 + 40.
    two_producers("inline-dos-productores", INLINE_A, INLINE_B, opt, 30040)
    if opt == "0":
        two_producers("rechazo-dos-productores-retorno", SPLIT_A, SPLIT_B, opt, None)
        for case in REJECTIONS:
            rejection(*case)
print("observación terminada" if observe else "Declaraciones de funciones C verificadas:", root)
