"""T08-002-09m7d1: a C global declared `extern T name[];` (size unknown to the importer).

NativeSharp projects it as the zero-storage `T[*]` already used for C flexible array members: elements are
indexed in place, the name decays to `T*` keeping the element's const/volatile qualifiers, and it can never be
assigned, copied or measured as a whole. Checked:
- a Clang control that reads and writes the same globals through the same header;
- direct project (header + static C library), O0-O3: int, const int, volatile int, record, enum and pointer
  elements; writes seen by C; decay to `T*` and to an inferred `var`; passing the decayed pointer to C;
- `__declspec(dllimport)` array from a C DLL, O0-O3;
- precompiled producer whose generic templates use the scalar and pointer element globals, consumed without
  the header, O0-O3 (record and enum globals are not projected from metadata yet: T08-004-04b);
- two precompiled producers whose templates see `T a[]`/`T a[]` or `T a[]`/`T a[N]` (C composite type) and a
  consumer with a third declaration, O0-O3; joins with another qualifier, element type, alignment or two
  different bounds are rejected as ambiguous (O0);
- rejections (O0): writing a const element, decaying const or volatile elements to a plain `T*`, a `T[*]`
  local, `sizeof` of the array, assigning the array, `&name` (C's `T (*)[]` has no NativeSharp form yet) and
  `extern T name[];` under thread-local storage.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-global-arreglo-incompleto-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
linker = r"A:\al82dev-clang\bin\lld-link.exe"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

HEADER = """typedef struct Pair { int a; int b; } Pair;
typedef enum Color { Red = 1, Green = 2, Blue = 7 } Color;
extern int open_arr[];
extern const int ro_arr[];
extern volatile int vo_arr[];
extern Pair pairs[];
extern Color colors[];
extern int *ptrs[];
int sum_open(int count);
int sum_ptr(const int *p, int count);
"""
SOURCE = """#include "api.h"
int open_arr[4] = {10, 20, 30, 40};
const int ro_arr[3] = {5, 6, 7};
volatile int vo_arr[2] = {8, 9};
Pair pairs[2] = {{1, 2}, {3, 4}};
Color colors[3] = {Red, Green, Blue};
int *ptrs[2] = {&open_arr[0], &open_arr[3]};
int sum_open(int count) { int s = 0; for (int i = 0; i < count; ++i) s += open_arr[i]; return s; }
int sum_ptr(const int *p, int count) { int s = 0; for (int i = 0; i < count; ++i) s += p[i]; return s; }
"""
# Every check returns a distinct non-zero code on failure; the C control runs the same statements.
CHECKS = [
    ("open_arr[1] == 20", "open_arr[1] == 20"),
    ("ro_arr[2] == 7", "ro_arr[2] == 7"),
    ("colors[2] == Blue", "colors[2] == Blue"),
    ("*ptrs[1] == 40", "*ptrs[1] == 40"),
    ("Write()", "write_c()"),
    ("Volatile()", "volatile_c()"),
    ("Record()", "record_c()"),
    ("Decay()", "decay_c()"),
    ("sum_ptr(open_arr, 4) == 10 + 20 + 99 + 41", "sum_ptr(open_arr, 4) == 10 + 20 + 99 + 41"),
    ("sum_ptr(ro_arr, 3) == 18", "sum_ptr(ro_arr, 3) == 18"),
]
NS_HELPERS = """
    static bool Write() { open_arr[2] = 99; return sum_open(4) == 10 + 20 + 99 + 40; }
    static bool Volatile() { vo_arr[0] = vo_arr[1] + 1; return vo_arr[0] == 10; }
    static bool Record() { pairs[1].b = 40; return pairs[0].a + pairs[1].b == 41; }
    static bool Decay() { int* p = open_arr; var q = open_arr; int const* r = ro_arr; int* e = &open_arr[1];
        p[3] = 41; return q[3] == 41 && r[1] == 6 && e[0] == 20 && open_arr[3] == 41; }
"""
C_HELPERS = """
static int write_c(void) { open_arr[2] = 99; return sum_open(4) == 10 + 20 + 99 + 40; }
static int volatile_c(void) { vo_arr[0] = vo_arr[1] + 1; return vo_arr[0] == 10; }
static int record_c(void) { pairs[1].b = 40; return pairs[0].a + pairs[1].b == 41; }
static int decay_c(void) { int *p = open_arr; int *q = open_arr; const int *r = ro_arr; int *e = &open_arr[1];
    p[3] = 41; return q[3] == 41 && r[1] == 6 && e[0] == 20 && open_arr[3] == 41; }
"""


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def body(checks):
    return "".join(f"    if (!({expression})) return {index};\n" for index, expression in enumerate(checks, 1))


def build_fixture(folder, dll=False):
    folder.mkdir(parents=True, exist_ok=True)
    header = HEADER
    source = SOURCE
    if dll:
        # Same declarations as dllimport data and functions; the DLL defines them as dllexport.
        header = "\n".join(line.replace("extern ", "__declspec(dllimport) extern ") if line.startswith("extern")
                           else "__declspec(dllimport) " + line if line.startswith("int sum_") else line
                           for line in HEADER.splitlines()) + "\n"
        source = HEADER.split("extern")[0] + "\n".join(
            "__declspec(dllexport) " + line for line in SOURCE.splitlines() if line and not line.startswith("#")) + "\n"
    (folder / "api.h").write_text(header, encoding="utf-8")
    (folder / "api.c").write_text(source, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    if dll:
        run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/out:fixture.dll", "/implib:fixture.lib",
             "api.obj"], folder)
    else:
        run([lib, "/nologo", "/out:fixture.lib", "api.obj"], folder)


def control():
    folder = root / "control"
    build_fixture(folder)
    (folder / "Control.c").write_text(
        '#include "api.h"\n__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n' + C_HELPERS +
        "static int checks(void) {\n" + body([c for _, c in CHECKS]) + "    return 0;\n}\n"
        "void mainCRTStartup(void) { ExitProcess(checks()); }\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], folder)
    run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
         "Control.obj", "fixture.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "Control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control C: comprobación {code} falló")
    print("control C correcto", flush=True)


def project(folder, name, sources, references, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" +
        ("<OutputType>Library</OutputType>" if library else "") + "</PropertyGroup><ItemGroup>" +
        "".join(f'<Compile Include="{s}" />' for s in sources) + references + "</ItemGroup></Project>",
        encoding="utf-8")


def direct(opt, dll=False):
    folder = root / (("dll-" if dll else "directo-") + "O" + opt)
    build_fixture(folder, dll)
    checks = [n for n, _ in CHECKS]
    (folder / "Main.ns").write_text("class Checks {" + NS_HELPERS + "    public static int Run() {\n" +
                                    body(checks) + "    return 0; } }\nint Main() { return Checks.Run(); }\n",
                                    encoding="utf-8")
    runtime = ' Runtime="fixture.dll"' if dll else ""
    project(folder, "Direct", ["Main.ns"], '<CppLibraryReference Header="api.h" Language="C" Standard="c11" '
            f'Library="fixture.lib"{runtime} />')
    run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Direct.nsproj", "-O" + opt,
         "--no-artifact-cache", "-o", "out/Direct.exe"], folder)
    code = run([folder / "out" / "Direct.exe"], Path("A:/"), check=False).returncode
    if code:
        raise AssertionError(f"{folder.name}: comprobación {code} falló")
    print(folder.name, "correcto", flush=True)


def precompiled(opt):
    folder = root / ("precompilado-O" + opt)
    build_fixture(folder / "producer")
    # Record and enum C globals are not yet projected from metadata at all (also `Pair g;` and `Pair g[2];`):
    # that gap belongs to T08-004-04b, so this template only uses scalar and pointer elements.
    checks = [n for n, _ in CHECKS if n not in ("colors[2] == Blue", "Record()")]
    helpers = "\n".join(line for line in NS_HELPERS.splitlines() if "Record()" not in line)
    helpers = helpers.replace("static bool ", "public static bool ").replace("() {", "<T>(T unused) {")
    for helper in ("Write", "Volatile", "Decay"):
        checks = [c.replace(helper + "()", helper + "<T>(unused)") for c in checks]
    (folder / "producer" / "Producer.ns").write_text(
        "namespace OpenArrays { public class Bridge {" + helpers +
        "    public static int Run<T>(T unused) {\n" + body(checks) + "    return 0; } } }\n", encoding="utf-8")
    project(folder / "producer", "OpenArrays", ["Producer.ns"],
            '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="fixture.lib" />',
            library=True)
    run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "OpenArrays.nsproj", "-O" + opt,
         "--no-artifact-cache", "-o", "OpenArrays.lib"], folder / "producer")
    (folder / "producer" / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return OpenArrays.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/OpenArrays.lib" />')
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Consumer.nsproj",
                  "-O" + opt, "--no-artifact-cache", "-o", "out/Consumer.exe"], folder, check=False)
    if result.returncode:
        raise AssertionError(f"{folder.name}: consumidor:\n{result.stdout}")
    code = run([folder / "out" / "Consumer.exe"], Path("A:/"), check=False).returncode
    if code:
        raise AssertionError(f"{folder.name}: comprobación {code} falló")
    print(folder.name, "correcto", flush=True)


NEGATIVES = [
    ("escritura-const", "ro_arr[0] = 1; return 0;", "not assignable"),
    ("decaimiento-const", "int* p = ro_arr; return p == null ? 1 : 0;", "pointee constness"),
    ("decaimiento-volatile", "int* p = vo_arr; return p == null ? 1 : 0;", "volatile qualification"),
    ("var-const", "var p = ro_arr; p[0] = 1; return 0;", "not assignable"),
    ("local-flexible", "int[*] x = open_arr; return 0;", "cannot be a local variable"),
    ("sizeof", "return sizeof(open_arr) == 0 ? 0 : 1;", "has no value layout"),
    ("asignacion", "int* p = null; open_arr = p; return 0;", "not assignable"),
    ("direccion", "var p = &open_arr; return 0;", "cannot take the address of C array 'open_arr' of unknown size"),
]


def negatives():
    folder = root / "rechazos"
    build_fixture(folder)
    for name, statements, expected in NEGATIVES:
        case = folder / name
        case.mkdir()
        (case / "Main.ns").write_text(f"int Main() {{ {statements} }}\n", encoding="utf-8")
        project(case, "Negative", ["Main.ns"], '<CppLibraryReference Header="../api.h" Language="C" '
                'Standard="c11" Library="../fixture.lib" />')
        result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Negative.nsproj",
                      "-O0", "--no-artifact-cache", "-o", "out/N.exe"], case, check=False)
        if result.returncode == 0 or "internal error" in result.stdout or "lowering failed" in result.stdout or \
                expected not in result.stdout:
            raise AssertionError(f"rechazo {name}: se esperaba «{expected}»:\n{result.stdout}")
        print("rechazo", name, "correcto", flush=True)
    case = folder / "tls"
    case.mkdir()
    (case / "tls.h").write_text("extern _Thread_local int tls_arr[];\n", encoding="utf-8")
    (case / "Main.ns").write_text("int Main() { return tls_arr[0]; }\n", encoding="utf-8")
    project(case, "NegativeTls", ["Main.ns"], '<CppLibraryReference Header="tls.h" Language="C" Standard="c11" />')
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "NegativeTls.nsproj", "-O0",
                  "--no-artifact-cache", "-o", "out/N.exe"], case, check=False)
    if result.returncode == 0 or "thread-local C/C++ global is not yet projected" not in result.stdout:
        raise AssertionError("rechazo tls:\n" + result.stdout)
    print("rechazo tls correcto", flush=True)


# Two precompiled producers whose templates see different C declarations of the same object, consumed (and read
# by the consumer itself) with a third declaration and no producer header:
# (name, producer A, producer B, consumer, expected or None).
SHARED = [
    ("compartido-incompleto", "extern int sh[];", "extern int sh[];", "extern int sh[];", 24),
    ("compartido-incompleto-y-fijo", "extern int sh[];", "extern int sh[3];", "extern int sh[];", 24),
    ("compartido-fijo-e-incompleto", "extern int sh[3];", "extern int sh[];", "extern int sh[3];", 24),
    ("compartido-alineado", "extern _Alignas(16) int sh[];", "extern _Alignas(16) int sh[3];",
     "extern _Alignas(16) int sh[];", 24),
    ("rechazo-const", "extern int sh[];", "extern const int sh[];", "extern int sh[];", None),
    ("rechazo-volatile", "extern int sh[];", "extern volatile int sh[3];", "extern int sh[];", None),
    ("rechazo-elemento", "extern int sh[];", "extern unsigned sh[3];", "extern int sh[];", None),
    ("rechazo-dos-limites", "extern int sh[];", "extern int sh[3];", "extern int sh[4];", None),
    ("rechazo-alineacion", "extern _Alignas(16) int sh[];", "extern int sh[3];", "extern int sh[];", None),
]


def shared(name, header_a, header_b, header_consumer, expected, opt):
    folder = root / f"{name}-O{opt}"
    for side, header in (("a", header_a), ("b", header_b)):
        (folder / side).mkdir(parents=True)
        (folder / side / "same.h").write_text(header + "\n", encoding="utf-8")
        (folder / side / "Producer.ns").write_text(
            f"namespace Shared{side.upper()} {{ public class Bridge {{ public static int Read<T>(T value) "
            f"{{ return sh[1]; }} }} }}\n", encoding="utf-8")
        project(folder / side, f"SharedProducer{side.upper()}", ["Producer.ns"],
                '<CppLibraryReference Header="same.h" Language="C" Standard="c11" />', library=True)
        producer = f"SharedProducer{side.upper()}"
        run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", producer + ".nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", producer + ".lib"], folder / side)
        (folder / side / "same.h").unlink()
    # A definition may add an alignment its declarations omit, never the reverse (C11 6.7.5p7).
    (folder / "fixture.c").write_text("_Alignas(16) int sh[3] = {5, 24, 6};\n", encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "fixture.c", "-o", "fixture.obj"], folder)
    run([lib, "/nologo", "/out:fixture.lib", "fixture.obj"], folder)
    (folder / "consumer.h").write_text(header_consumer + "\n", encoding="utf-8")
    (folder / "Consumer.ns").write_text(
        "int Main() { return sh[1] * 1000000 + SharedA.Bridge.Read<int>(0) * 1000 + SharedB.Bridge.Read<int>(0); }\n",
        encoding="utf-8")
    project(folder, "SharedConsumer", ["Consumer.ns"], '<LibraryReference Include="a/SharedProducerA.lib" />'
            '<LibraryReference Include="b/SharedProducerB.lib" />'
            '<CppLibraryReference Header="consumer.h" Language="C" Standard="c11" Library="fixture.lib" />')
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "SharedConsumer.nsproj",
                  "-O" + opt, "--no-artifact-cache", "-o", "out/SharedConsumer.exe"], folder, check=False)
    if expected is None:
        if result.returncode == 0 or "ambiguous namespace state reference 'sh'" not in result.stdout:
            raise AssertionError(f"{name}: se esperaba un rechazo por ambigüedad:\n{result.stdout}")
        print(name, "rechazado", flush=True)
        return
    if result.returncode:
        raise AssertionError(f"{name} O{opt}: consumidor:\n{result.stdout}")
    code = run([folder / "out" / "SharedConsumer.exe"], Path("A:/"), check=False).returncode
    if code != expected * 1001001:
        raise AssertionError(f"{name} O{opt}: se esperaba {expected * 1001001}, se obtuvo {code}")
    print(name, f"O{opt}", "correcto", flush=True)


def ir_contract():
    folder = root / "ir"
    build_fixture(folder)
    (folder / "Main.ns").write_text("int Touch() { vo_arr[0] = vo_arr[1] + 1; return open_arr[0]; }\n"
                                    "int Main() { return Touch() == 10 ? 0 : 1; }\n", encoding="utf-8")
    project(folder, "Ir", ["Main.ns"], '<CppLibraryReference Header="api.h" Language="C" Standard="c11" '
            'Library="fixture.lib" />')
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm", "Ir.nsproj"], folder)
    (folder / "native.ll").write_text(result.stdout, encoding="utf-8")
    if not re.search(r"@open_arr = external (?:dso_local )?global \[0 x i8\]", result.stdout) or \
            not re.search(r"@vo_arr = external (?:dso_local )?global \[0 x i8\]", result.stdout):
        raise AssertionError("las globales T[*] no se declaran como almacenamiento externo de longitud cero")
    touch = re.findall(r"define [^\n]*Touch[^\n]*\{(.*?)\n\}", result.stdout, re.S)
    if not touch or len(re.findall(r"load volatile i32", touch[0])) != 1 or \
            len(re.findall(r"store volatile i32", touch[0])) != 1:
        raise AssertionError("los elementos de vo_arr no se leen y escriben con accesos volatile")
    print("IR: declaración externa [0 x i8] y accesos volatile a los elementos", flush=True)


control()
ir_contract()
negatives()
for opt in opts:
    direct(opt)
    direct(opt, dll=True)
    precompiled(opt)
    for case in SHARED:
        if case[-1] is not None or opt == "0":
            shared(*case, opt)
print("Global C `extern T name[];` verificada:", root)
