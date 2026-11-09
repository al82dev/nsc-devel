"""T08-002: C variadic arguments beyond the earlier scalar sample, with two homonymous TLS DLLs.

Each DLL (`api.dll` in two folders) exports variadic functions that read with `va_arg`:
- signed and unsigned narrow integers and `bool` (promoted to `int`);
- `long long`, `unsigned int` and a negative `double`;
- an `int*`, a null `int*` and a function pointer;
- C records by value: 8 bytes (Win64 register), 24 bytes and 3 bytes (both passed by copy);
- the same records through a variadic function-pointer typedef.
Each function returns a constant of its own DLL (3 or 7) when every argument matches, so a call that
reaches the other DLL or reads a wrong slot fails; `Access` still writes and reads each DLL's TLS.
Clang controls call the same functions with the same values. The NativeSharp calls live in precompiled
templates (one producer per DLL), are consumed without the headers in O0-O3, and the consumer is
restored from the artifact cache.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-variadicas-agregados-", dir=r"A:\nsc-interop-build"))
clang = r"A:\al82dev-clang\bin\clang.exe"
linker = r"A:\al82dev-clang\bin\lld-link.exe"
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
env = os.environ.copy()
env["PATH"] = r"A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
crt = Path(r"A:\vc2026-runtime\lib\x64\release")
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
opts = sys.argv[1:] or ["0", "1", "2", "3"]

HEADER = """typedef struct Small { int a; int b; } Small;
typedef struct Large { long long a; long long b; long long c; } Large;
typedef struct Odd { char a; char b; char c; } Odd;
typedef int (*VarFn)(int marker, ...);
extern __declspec(thread) int tls_exported;
__declspec(dllimport) int* cell_ptr(void);
__declspec(dllimport) int twice(int x);
__declspec(dllimport) Small make_small(void);
__declspec(dllimport) Large make_large(void);
__declspec(dllimport) Odd make_odd(void);
__declspec(dllimport) int var_signed(int marker, ...);
__declspec(dllimport) int var_wide(int marker, ...);
__declspec(dllimport) int var_pointer(int marker, ...);
__declspec(dllimport) int var_record(int marker, ...);
"""


def source(index, initial):
    return f"""typedef struct Small {{ int a; int b; }} Small;
typedef struct Large {{ long long a; long long b; long long c; }} Large;
typedef struct Odd {{ char a; char b; char c; }} Odd;
__declspec(thread) int tls_exported = {initial};
// Each function answers with a constant of its own DLL ({initial}); the TLS variable is still exported
// so both DLLs keep the homonymous TLS packaging of the earlier samples.
static int cell = {70 + index};
__declspec(dllexport) int* cell_ptr(void) {{ return &cell; }}
__declspec(dllexport) int twice(int x) {{ return x * 2; }}
__declspec(dllexport) Small make_small(void) {{ Small s = {{{index}, 4}}; return s; }}
__declspec(dllexport) Large make_large(void) {{ Large l = {{10, 20, {index}}}; return l; }}
__declspec(dllexport) Odd make_odd(void) {{ Odd o = {{1, 2, {index}}}; return o; }}
__declspec(dllexport) int var_signed(int marker, ...) {{
    __builtin_va_list ap; __builtin_va_start(ap, marker);
    int a = __builtin_va_arg(ap, int), b = __builtin_va_arg(ap, int), c = __builtin_va_arg(ap, int);
    int d = __builtin_va_arg(ap, int), e = __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    return marker == {index} && a == -5 && b == -300 && c == 250 && d == 65000 && e == 1 ? {initial} : -101;
}}
__declspec(dllexport) int var_wide(int marker, ...) {{
    __builtin_va_list ap; __builtin_va_start(ap, marker);
    long long a = __builtin_va_arg(ap, long long);
    unsigned b = __builtin_va_arg(ap, unsigned);
    double c = __builtin_va_arg(ap, double);
    __builtin_va_end(ap);
    return marker == {index} && a == -5000000000LL && b == 4000000000u && c == -0.5 ? {initial} : -102;
}}
__declspec(dllexport) int var_pointer(int marker, ...) {{
    __builtin_va_list ap; __builtin_va_start(ap, marker);
    int* p = __builtin_va_arg(ap, int*);
    int* z = __builtin_va_arg(ap, int*);
    int (*f)(int) = __builtin_va_arg(ap, int (*)(int));
    __builtin_va_end(ap);
    return marker == {index} && *p == {70 + index} && z == 0 && f(21) == 42 ? {initial} : -103;
}}
__declspec(dllexport) int var_record(int marker, ...) {{
    __builtin_va_list ap; __builtin_va_start(ap, marker);
    Small s = __builtin_va_arg(ap, Small);
    Large l = __builtin_va_arg(ap, Large);
    Odd o = __builtin_va_arg(ap, Odd);
    int tail = __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    return marker == {index} && s.a == {index} && s.b == 4 && l.a == 10 && l.b == 20 && l.c == {index} &&
        o.a == 1 && o.b == 2 && o.c == {index} && tail == 99 ? {initial} : -104;
}}
"""


CALLS = {
    "Signed": "var_signed({index}, (sbyte)-5, (short)-300, (byte)250, (ushort)65000, true)",
    "Wide": "var_wide({index}, (long)-5000000000, (uint)4000000000, -0.5)",
    "Pointer": "var_pointer({index}, cell_ptr(), z, f)",
    "Record": "var_record({index}, make_small(), make_large(), make_odd(), 99)",
    "RecordIndirect": "v({index}, make_small(), make_large(), make_odd(), 99)",
}
PRELUDE = {"Pointer": "int* z = null; var f = twice; ", "RecordIndirect": "VarFn v = var_record; "}
C_CALLS = {
    "Signed": "var_signed({index}, (signed char)-5, (short)-300, (unsigned char)250, (unsigned short)65000, 1)",
    "Wide": "var_wide({index}, -5000000000LL, 4000000000u, -0.5)",
    "Pointer": "var_pointer({index}, cell_ptr(), (int*)0, twice)",
    "Record": "var_record({index}, make_small(), make_large(), make_odd(), 99)",
    "RecordIndirect": "((VarFn)var_record)({index}, make_small(), make_large(), make_odd(), 99)",
}


def run(args, cwd):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise AssertionError(f"falló {args[0]} (código {result.returncode}):\n" + result.stdout)
    return result.stdout


def build_producers(opt):
    for index, initial in ((1, 3), (2, 7)):
        folder = root / f"O{opt}" / str(index)
        folder.mkdir(parents=True)
        (folder / "api.h").write_text(HEADER, encoding="utf-8")
        (folder / "api.c").write_text(source(index, initial), encoding="utf-8")
        run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
        run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/include:_tls_used",
             "/export:tls_exported,DATA", "/out:api.dll", "/implib:api.lib", "api.obj", crt / "libcmt.lib",
             crt / "libcpmt.lib", crt / "libvcruntime.lib", sdk / "ucrt/x64/libucrt.lib",
             sdk / "um/x64/kernel32.lib"], folder)
        checks = " | ".join(f"(({call.format(index=index)} != {initial}) << {bit})"
                            for bit, call in enumerate(C_CALLS.values()))
        (folder / "Control.c").write_text(
            '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n'
            "int _fltused = 0;\n"
            f"void mainCRTStartup(void) {{ ExitProcess({checks}); }}\n", encoding="utf-8")
        run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], folder)
        run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
             "Control.obj", "api.lib", sdk / "um/x64/kernel32.lib"], folder)
        run([folder / "Control.exe"], folder)
        methods = "".join(
            f"        public static int {name}<T>(T unused) {{ {PRELUDE.get(name, '')}"
            f"return {call.format(index=index)}; }}\n" for name, call in CALLS.items())
        (folder / "Producer.ns").write_text(
            f"namespace Variadic{index}\n{{\n    public class Bridge\n    {{\n"
            "        public static int Access<T>(T unused, int value) { tls_exported = value; return tls_exported; }\n"
            + methods + "    }\n}\n", encoding="utf-8")
        (folder / "Producer.nsproj").write_text(
            f"<Project><PropertyGroup><Name>Variadic{index}</Name><OutputType>Library</OutputType></PropertyGroup>"
            '<ItemGroup><Compile Include="Producer.ns" /><CppLibraryReference Header="api.h" Language="C" '
            'Standard="c11" Library="api.lib" Runtime="api.dll" /></ItemGroup></Project>', encoding="utf-8")
        run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Producer.nsproj", "-O" + opt,
             "--no-artifact-cache", "-o", f"Variadic{index}.lib"], folder)
        (folder / "api.h").unlink()


def consume(opt):
    folder = root / f"O{opt}"
    checks = "".join(
        f"    if (Variadic1.Bridge.{name}<int>(0) != 3 || Variadic2.Bridge.{name}<int>(0) != 7) return {n + 2};\n"
        for n, name in enumerate(CALLS))
    (folder / "Consumer.ns").write_text(
        "int Main()\n{\n"
        "    if (Variadic1.Bridge.Access<int>(0, 41) != 41 || Variadic2.Bridge.Access<int>(0, 51) != 51) return 1;\n"
        + checks + "    return 0;\n}\n", encoding="utf-8")
    (folder / "Consumer.nsproj").write_text(
        '<Project><PropertyGroup><Name>VariadicConsumer</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Consumer.ns" /><LibraryReference Include="1/Variadic1.lib" />'
        '<LibraryReference Include="2/Variadic2.lib" /></ItemGroup></Project>', encoding="utf-8")
    cache = folder / "cache"
    command = [nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Consumer.nsproj", "-O" + opt,
               "--time", "--artifact-cache=" + str(cache), "-o", "out/Consumer.exe"]
    for attempt, marker in enumerate(("compilation-bundle-cache: not-found", "compilation-bundle-cache: hit")):
        output = folder / "out"
        if output.exists():
            for item in output.iterdir():
                item.unlink()
        log = run(command, folder)
        if marker not in log:
            raise AssertionError(f"O{opt} intento {attempt}: estado de caché inesperado\n" + log)
        if len(list(output.glob("__nsc_tls_*.dll"))) != 2:
            raise AssertionError(f"O{opt}: no se publicaron las dos DLL TLS")
        run([output / "Consumer.exe"], Path("A:/"))


def negatives():
    # Only C records go through `...`: a NativeSharp struct, an untyped `null` and a method group without a
    # target type are rejected by overload resolution (O0, header present).
    folder = root / "negativos"
    folder.mkdir()
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    cases = {
        "struct-nativesharp":
            "struct Point { public int X; }\nint Main() { Point p; p.X = 1; return var_record(1, p); }",
        "null-sin-tipo": "int Main() { return var_pointer(1, null); }",
        "grupo-de-metodos": "int Main() { return var_pointer(1, twice); }",
    }
    for name, text in cases.items():
        (folder / "Main.ns").write_text(text + "\n", encoding="utf-8")
        (folder / "Neg.nsproj").write_text(
            '<Project><PropertyGroup><Name>Neg</Name></PropertyGroup><ItemGroup><Compile Include="Main.ns" />'
            '<CppLibraryReference Header="api.h" Language="C" Standard="c11" /></ItemGroup></Project>',
            encoding="utf-8")
        result = subprocess.run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Neg.nsproj",
                                 "-O0", "--no-artifact-cache", "-o", "out/Neg.exe"],
                                cwd=folder, env=env, text=True, encoding="utf-8", errors="replace",
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode == 0 or "internal error" in result.stdout or (
                "no viable overload" not in result.stdout and
                "does not match a delegate parameter" not in result.stdout):
            raise AssertionError(f"negativo {name} no rechazado:\n" + result.stdout)
    print("negativos: struct NativeSharp, null sin tipo y grupo de métodos rechazados", flush=True)


negatives()
for opt in opts:
    build_producers(opt)
    consume(opt)
    print(f"Variádicas con enteros con signo, anchos, punteros y records C en dos DLL TLS homónimas: correcto O{opt}",
          flush=True)
print("Promociones y agregados variádicos verificados;", root)
