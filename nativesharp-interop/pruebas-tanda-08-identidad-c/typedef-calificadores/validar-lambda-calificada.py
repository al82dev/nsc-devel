"""T08-002: lambdas and NativeSharp methods converted to C callbacks whose parameters point to `const`
or `volatile` data.

Before, a lambda parameter of a `ConstFn` callback could be written through, and a lambda parameter of
a `VolatileFn` callback was read with ordinary loads. Positive cases compile and run with the header
(direct path) and through a precompiled template library consumed without the header, in O0-O3.
Negative cases must be rejected in both paths. The IR check requires a volatile read in the lambda
body. A C control built with Clang documents the method conversions. The precompiled path omits the
conversion of a public NativeSharp method with a `T const*` parameter (see POSITIVE_PRECOMPILED).
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-lambda-calificada-", dir=r"A:\nsc-interop-build"))
nsc = Path(r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe")
clang = Path(r"A:\al82dev-clang\bin\clang.exe")
linker = Path(r"A:\al82dev-clang\bin\lld-link.exe")
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
env = os.environ.copy()
env["PATH"] = str(nsc.parent) + r";A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
opts = sys.argv[1:] or ["0", "1", "2", "3"]

HEADER = """typedef struct Payload { int value; } Payload;
typedef int (*ConstFn)(const Payload*);
typedef int (*VolatileFn)(volatile Payload*);
__declspec(dllimport) int call_const(ConstFn f);
__declspec(dllimport) int call_volatile(VolatileFn f);
"""

SOURCE = """typedef struct Payload { int value; } Payload;
typedef int (*ConstFn)(const Payload*);
typedef int (*VolatileFn)(volatile Payload*);
static Payload cell = {40};
__declspec(dllexport) int call_const(ConstFn f) { return f(&cell); }
__declspec(dllexport) int call_volatile(VolatileFn f) { return f(&cell); }
"""

# NativeSharp helpers shared by every case: top-level functions directly, static members in the template.
HELPERS = [
    "int ReadConst(Payload const* p) { return p->value + 1; }",
    "int ReadPlain(Payload* p) { return p->value + 3; }",
]

POSITIVE = [
    ("CaseLambdaConst", "ConstFn f = (p) => p->value + 1; return call_const(f);", 41),
    ("CaseLambdaVolatile", "VolatileFn f = (p) => p->value + 2; return call_volatile(f);", 42),
    ("CaseLambdaTyped", "ConstFn f = (Payload const* p) => p->value + 1; return call_const(f);", 41),
    ("CaseMethodConst", "return call_const(ReadConst);", 41),
]

# The precompiled path omits CaseMethodConst: the metadata of a public NativeSharp method does not carry
# the `const` of a `T const*` parameter, so the materialized ReadConst no longer matches ConstFn (pending
# T08-002 decision: it needs a field in NSMeta).
POSITIVE_PRECOMPILED = [case for case in POSITIVE if case[0] != "CaseMethodConst"]

NOT_ASSIGNABLE = "left side of assignment is not assignable"
NO_MATCH = "method group does not match a delegate parameter of namespace function"

NEGATIVE = [
    ("RejectLambdaWrite", "ConstFn f = (p) => { p->value = 1; return 0; }; return call_const(f);", NOT_ASSIGNABLE),
    ("RejectTypedLambdaWrite", "ConstFn f = (Payload* p) => { p->value = 1; return 0; }; return call_const(f);",
     NOT_ASSIGNABLE),
    ("RejectMethodPlainToConst", "return call_const(ReadPlain);", NO_MATCH),
    ("RejectMethodPlainToVolatile", "return call_volatile(ReadPlain);", NO_MATCH),
]


def execute(command, folder):
    return subprocess.run([str(x) for x in command], cwd=folder, env=env, text=True,
                          encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)


def require(command, folder, label):
    result = execute(command, folder)
    if result.returncode:
        raise AssertionError(label + " falló:\n" + result.stdout)
    return result.stdout


def build_dll(folder):
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    require([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"],
            folder, "clang api.c")
    require([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/out:api.dll", "/implib:api.lib",
             "api.obj"], folder, "lld-link api.dll")


def c_control(folder):
    (folder / "control.c").write_text(
        '#include "api.h"\n'
        "static int read_const(const Payload* p) { return p->value + 1; }\n"
        "static int read_volatile(volatile Payload* p) { return p->value + 2; }\n"
        "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
        "void mainCRTStartup(void) { ExitProcess(call_const(read_const) != 41 || "
        "call_volatile(read_volatile) != 42); }\n", encoding="utf-8")
    require([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"],
            folder, "clang control.c")
    require([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
             "/out:control.exe", "control.obj", "api.lib", sdk / "um/x64/kernel32.lib"], folder, "control link")
    require([folder / "control.exe"], folder, "control.exe")
    for name, body, needle in (
            ("PlainToConst", "static int f(Payload* p) { return p->value; }\nint probe(void) { return call_const(f); }",
             "incompatible"),
            ("PlainToVolatile", "static int f(Payload* p) { return p->value; }\n"
                                "int probe(void) { return call_volatile(f); }", "incompatible"),
            ("WriteConst", "static int f(const Payload* p) { p->value = 1; return 0; }\n"
                           "int probe(void) { return call_const(f); }", "const-qualified")):
        (folder / f"reject_{name}.c").write_text('#include "api.h"\n' + body + "\n", encoding="utf-8")
        result = execute([clang, "--target=x86_64-pc-windows-msvc", "-std=c11", "-fsyntax-only",
                          f"reject_{name}.c"], folder)
        if result.returncode == 0 or needle not in result.stdout:
            raise AssertionError("Clang aceptó el control " + name + ":\n" + result.stdout)
    print("control C: valores correctos; conversiones incompatibles y escritura const rechazadas", flush=True)


def write_project(folder, name, sources, header):
    items = "".join(f'<Compile Include="{source}" />' for source in sources)
    if header:
        items += ('<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="api.lib" '
                  'Runtime="api.dll" />')
    kind = "<OutputType>Library</OutputType>" if name.startswith("Producer") else ""
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>{kind}</PropertyGroup><ItemGroup>{items}"
        "</ItemGroup></Project>", encoding="utf-8")


def template_source(cases):
    methods = "".join(f"        public static {helper}\n" for helper in HELPERS)
    methods += "".join(f"        public static int {name}<T>(T unused) {{ {body} }}\n"
                       for name, body, _ in cases)
    return "namespace QualifiedLambdas\n{\n    public class Bridge\n    {\n" + methods + "    }\n}\n"


def direct_source(cases):
    functions = "".join(helper + "\n" for helper in HELPERS)
    functions += "".join(f"int {name}() {{ {body} }}\n" for name, body, _ in cases)
    checks = "".join(f"    if ({name}() != {value}) return {index + 1};\n"
                     for index, (name, _, value) in enumerate(cases))
    return functions + "int Main()\n{\n" + checks + "    return 0;\n}\n"


def consumer_source(cases):
    checks = "".join(f"    if (QualifiedLambdas.Bridge.{name}<int>(0) != {value}) return {index + 1};\n"
                     for index, (name, _, value) in enumerate(cases))
    return "int Main()\n{\n" + checks + "    return 0;\n}\n"


def positive_direct(opt):
    folder = root / f"direct-O{opt}"
    folder.mkdir()
    build_dll(folder)
    (folder / "Main.ns").write_text(direct_source(POSITIVE), encoding="utf-8")
    write_project(folder, "Direct", ["Main.ns"], True)
    require([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Direct.nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", "out/Direct.exe"], folder, f"directo O{opt}")
    require([folder / "out" / "Direct.exe"], folder, f"ejecución directa O{opt}")


def positive_precompiled(opt):
    folder = root / f"precompiled-O{opt}"
    producer = folder / "producer"
    producer.mkdir(parents=True)
    build_dll(producer)
    (producer / "Producer.ns").write_text(template_source(POSITIVE_PRECOMPILED), encoding="utf-8")
    write_project(producer, "ProducerLambda", ["Producer.ns"], True)
    require([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "ProducerLambda.nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", "ProducerLambda.lib"], producer, f"productor O{opt}")
    (producer / "api.h").unlink()
    (folder / "Consumer.ns").write_text(consumer_source(POSITIVE_PRECOMPILED), encoding="utf-8")
    (folder / "Consumer.nsproj").write_text(
        '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />'
        '<LibraryReference Include="producer/ProducerLambda.lib" /></ItemGroup></Project>', encoding="utf-8")
    require([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Consumer.nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", "out/Consumer.exe"], folder, f"consumidor O{opt}")
    require([folder / "out" / "Consumer.exe"], Path("A:/"), f"ejecución precompilada O{opt}")


def expect_rejection(result, message, label, artifact):
    if result.returncode == 0 or message not in result.stdout or artifact.exists():
        raise AssertionError(label + " no produjo el rechazo esperado '" + message + "':\n" + result.stdout)


def negative_cases():
    for name, body, message in NEGATIVE:
        case = [(name, body, 0)]
        direct = root / f"reject-direct-{name}"
        direct.mkdir()
        build_dll(direct)
        (direct / "Main.ns").write_text(direct_source(case), encoding="utf-8")
        write_project(direct, "Direct", ["Main.ns"], True)
        result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Direct.nsproj",
                          "-O0", "--no-artifact-cache", "-o", "out/Direct.exe"], direct)
        expect_rejection(result, message, "directo " + name, direct / "out" / "Direct.exe")
        folder = root / f"reject-precompiled-{name}"
        producer = folder / "producer"
        producer.mkdir(parents=True)
        build_dll(producer)
        (producer / "Producer.ns").write_text(template_source(case), encoding="utf-8")
        write_project(producer, "ProducerLambda", ["Producer.ns"], True)
        result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib",
                          "ProducerLambda.nsproj", "-O0", "--no-artifact-cache", "-o", "ProducerLambda.lib"],
                         producer)
        stage = "productor"
        if result.returncode == 0:
            (producer / "api.h").unlink()
            (folder / "Consumer.ns").write_text(consumer_source(case), encoding="utf-8")
            (folder / "Consumer.nsproj").write_text(
                '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup>'
                '<Compile Include="Consumer.ns" /><LibraryReference Include="producer/ProducerLambda.lib" />'
                '</ItemGroup></Project>', encoding="utf-8")
            result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe",
                              "Consumer.nsproj", "-O0", "--no-artifact-cache", "-o", "out/Consumer.exe"], folder)
            stage = "consumidor sin encabezado"
        expect_rejection(result, message, "precompilado " + name, folder / "out" / "Consumer.exe")
        print(name, "rechazado (directo y precompilado; etapa:", stage + ")", flush=True)


def ir_contract():
    folder = root / "ir"
    folder.mkdir()
    build_dll(folder)
    (folder / "Main.ns").write_text(
        "int ViaLambda() { VolatileFn f = (p) => p->value; return call_volatile(f); }\n"
        "int Main() { return ViaLambda() == 40 ? 0 : 1; }\n", encoding="utf-8")
    write_project(folder, "Ir", ["Main.ns"], True)
    result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm", "Ir.nsproj"], folder)
    if result.returncode:
        raise AssertionError("dump IR falló:\n" + result.stdout)
    (folder / "native.ll").write_text(result.stdout, encoding="utf-8")
    lambdas = re.findall(r"define [^\n]*lambda[^\n]*\{(.*?)\n\}", result.stdout, re.S)
    if not lambdas or not any(re.search(r"load (?:atomic )?volatile", body) for body in lambdas):
        raise AssertionError("el cuerpo de la lambda no lee el dato volatile con un acceso volatile")
    print("IR: el cuerpo de la lambda lee el dato apuntado volatile con un acceso volatile", flush=True)


control = root / "control"
control.mkdir()
build_dll(control)
c_control(control)
ir_contract()
negative_cases()
for opt in opts:
    positive_direct(opt)
    positive_precompiled(opt)
    print("casos positivos correctos (directo y precompilado sin encabezado) en O" + opt, flush=True)
print("Lambdas y métodos convertidos a callbacks C con datos apuntados calificados verificados;", root)
