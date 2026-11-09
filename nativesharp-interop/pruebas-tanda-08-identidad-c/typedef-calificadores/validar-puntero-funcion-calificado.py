"""T08-002: C function-pointer types written without a typedef whose parameters or result point to
`const` or `volatile` data: function parameters and results, struct fields and globals.

Before, `CppSemanticFunction` rejected these types, so the functions, fields and globals were not
imported at all. Positive cases compile and run with the header (direct path) and through a precompiled
template library consumed without the header, in O0-O3. Negative cases must be rejected in both paths
(frontend checks, O0). A C control built with Clang documents the same contract.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-puntero-funcion-calificado-", dir=r"A:\nsc-interop-build"))
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
typedef struct Holder { int (*cb)(const Payload*); int (*vcb)(volatile Payload*); } Holder;
__declspec(dllimport) const Payload* get_const(void);
__declspec(dllimport) volatile Payload* get_volatile(void);
__declspec(dllimport) int take_const(const Payload* p);
__declspec(dllimport) int take_volatile(volatile Payload* p);
__declspec(dllimport) int apply_const(int (*cb)(const Payload*), const Payload* p);
__declspec(dllimport) int apply_volatile(int (*cb)(volatile Payload*), volatile Payload* p);
__declspec(dllimport) int (*const_callback(void))(const Payload*);
__declspec(dllimport) int (*volatile_callback(void))(volatile Payload*);
__declspec(dllimport) const Payload* (*const_getter(void))(void);
__declspec(dllimport) volatile Payload* (*volatile_getter(void))(void);
__declspec(dllimport) extern int (*global_const)(const Payload*);
__declspec(dllimport) extern int (*global_volatile)(volatile Payload*);
"""

SOURCE = """typedef struct Payload { int value; } Payload;
static Payload cell = {40};
__declspec(dllexport) const Payload* get_const(void) { return &cell; }
__declspec(dllexport) volatile Payload* get_volatile(void) { return &cell; }
__declspec(dllexport) int take_const(const Payload* p) { return p->value + 1; }
__declspec(dllexport) int take_volatile(volatile Payload* p) { return p->value + 2; }
__declspec(dllexport) int apply_const(int (*cb)(const Payload*), const Payload* p) { return cb(p); }
__declspec(dllexport) int apply_volatile(int (*cb)(volatile Payload*), volatile Payload* p) { return cb(p); }
__declspec(dllexport) int (*const_callback(void))(const Payload*) { return take_const; }
__declspec(dllexport) int (*volatile_callback(void))(volatile Payload*) { return take_volatile; }
__declspec(dllexport) const Payload* (*const_getter(void))(void) { return get_const; }
__declspec(dllexport) volatile Payload* (*volatile_getter(void))(void) { return get_volatile; }
__declspec(dllexport) int (*global_const)(const Payload*) = take_const;
__declspec(dllexport) int (*global_volatile)(volatile Payload*) = take_volatile;
"""

# Positive cases: (name, NativeSharp body, expected value).
POSITIVE = [
    ("CaseParamConst", "return apply_const(take_const, get_const());", 41),
    ("CaseParamVolatile", "return apply_volatile(take_volatile, get_volatile());", 42),
    ("CaseResultConst", "var f = const_callback(); return f(get_const());", 41),
    ("CaseResultVolatile", "var f = volatile_callback(); return f(get_volatile());", 42),
    ("CaseGetterConst", "var g = const_getter(); var p = g(); return p->value;", 40),
    ("CaseGetterVolatile", "var g = volatile_getter(); var p = g(); return p->value;", 40),
    ("CaseField", "Holder h; h.cb = take_const; h.vcb = take_volatile; "
                  "return h.cb(get_const()) + h.vcb(get_volatile());", 83),
    ("CaseGlobal", "return global_const(get_const()) + global_volatile(get_volatile());", 83),
]

NO_MATCH = "method group does not match a delegate parameter of namespace function"
NOT_ASSIGNABLE = "left side of assignment is not assignable"
DISCARD_VOLATILE = "indirect call argument would discard pointer pointee volatile qualification"
NO_FIELD_MATCH = "no method in the method group exactly matches delegate signature"

# Negative cases: (name, NativeSharp body, expected diagnostic).
NEGATIVE = [
    ("RejectParamConst", "return apply_const(take_volatile, get_const());", NO_MATCH),
    ("RejectParamVolatile", "return apply_volatile(take_const, get_volatile());", NO_MATCH),
    ("RejectGetterWrite", "var g = const_getter(); var p = g(); p->value = 1; return 0;", NOT_ASSIGNABLE),
    ("RejectResultArgument", "var f = const_callback(); return f(get_volatile());", DISCARD_VOLATILE),
    ("RejectFieldAssign", "Holder h; h.cb = take_volatile; return 0;", NO_FIELD_MATCH),
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
    checks = " || ".join(f"case_{name}() != {value}" for name, _, value in POSITIVE)
    bodies = ""
    for name, body, _ in POSITIVE:
        c_body = body
        c_body = c_body.replace("var f = const_callback();", "int (*f)(const Payload*) = const_callback();")
        c_body = c_body.replace("var f = volatile_callback();",
                                "int (*f)(volatile Payload*) = volatile_callback();")
        c_body = c_body.replace("var g = const_getter(); var p = g();",
                                "const Payload* (*g)(void) = const_getter(); const Payload* p = g();")
        c_body = c_body.replace("var g = volatile_getter(); var p = g();",
                                "volatile Payload* (*g)(void) = volatile_getter(); volatile Payload* p = g();")
        bodies += f"static int case_{name}(void) {{ {c_body} }}\n"
    (folder / "control.c").write_text(
        '#include "api.h"\n' + bodies +
        "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
        f"void mainCRTStartup(void) {{ ExitProcess({checks}); }}\n", encoding="utf-8")
    require([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "control.c", "-o", "control.obj"],
            folder, "clang control.c")
    require([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib",
             "/out:control.exe", "control.obj", "api.lib", sdk / "um/x64/kernel32.lib"], folder, "control link")
    require([folder / "control.exe"], folder, "control.exe")
    for name, body, needle in (("ParamConst", "return apply_const(take_volatile, get_const());", "incompatible"),
                               ("ParamVolatile", "return apply_volatile(take_const, get_volatile());",
                                "incompatible"),
                               ("FieldAssign", "Holder h; h.cb = take_volatile; return 0;", "incompatible"),
                               ("GetterWrite", "const Payload* (*g)(void) = const_getter(); const Payload* p = g(); "
                                               "p->value = 1; return 0;", "const-qualified")):
        (folder / f"reject_{name}.c").write_text(
            '#include "api.h"\n' + f"int probe(void) {{ {body} }}\n", encoding="utf-8")
        result = execute([clang, "--target=x86_64-pc-windows-msvc", "-std=c11", "-fsyntax-only",
                          f"reject_{name}.c"], folder)
        if result.returncode == 0 or needle not in result.stdout:
            raise AssertionError("Clang aceptó el control " + name + ":\n" + result.stdout)
    print("control C: valores correctos; conversiones incompatibles rechazadas", flush=True)


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
    methods = "".join(f"        public static int {name}<T>(T unused) {{ {body} }}\n"
                      for name, body, _ in cases)
    return "namespace QualifiedPointers\n{\n    public class Bridge\n    {\n" + methods + "    }\n}\n"


def direct_source(cases):
    functions = "".join(f"int {name}() {{ {body} }}\n" for name, body, _ in cases)
    checks = "".join(f"    if ({name}() != {value}) return {index + 1};\n"
                     for index, (name, _, value) in enumerate(cases))
    return functions + "int Main()\n{\n" + checks + "    return 0;\n}\n"


def consumer_source(cases):
    checks = "".join(f"    if (QualifiedPointers.Bridge.{name}<int>(0) != {value}) return {index + 1};\n"
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
    (producer / "Producer.ns").write_text(template_source(POSITIVE), encoding="utf-8")
    write_project(producer, "ProducerQualified", ["Producer.ns"], True)
    require([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "ProducerQualified.nsproj",
             "-O" + opt, "--no-artifact-cache", "-o", "ProducerQualified.lib"], producer, f"productor O{opt}")
    (producer / "api.h").unlink()
    (folder / "Consumer.ns").write_text(consumer_source(POSITIVE), encoding="utf-8")
    (folder / "Consumer.nsproj").write_text(
        '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />'
        '<LibraryReference Include="producer/ProducerQualified.lib" /></ItemGroup></Project>', encoding="utf-8")
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
        write_project(producer, "ProducerQualified", ["Producer.ns"], True)
        result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib",
                          "ProducerQualified.nsproj", "-O0", "--no-artifact-cache", "-o", "ProducerQualified.lib"],
                         producer)
        stage = "productor"
        if result.returncode == 0:
            (producer / "api.h").unlink()
            (folder / "Consumer.ns").write_text(consumer_source(case), encoding="utf-8")
            (folder / "Consumer.nsproj").write_text(
                '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup>'
                '<Compile Include="Consumer.ns" /><LibraryReference Include="producer/ProducerQualified.lib" />'
                '</ItemGroup></Project>', encoding="utf-8")
            result = execute([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe",
                              "Consumer.nsproj", "-O0", "--no-artifact-cache", "-o", "out/Consumer.exe"], folder)
            stage = "consumidor sin encabezado"
        expect_rejection(result, message, "precompilado " + name, folder / "out" / "Consumer.exe")
        print(name, "rechazado (directo y precompilado; etapa:", stage + ")", flush=True)


control = root / "control"
control.mkdir()
build_dll(control)
c_control(control)
negative_cases()
for opt in opts:
    positive_direct(opt)
    positive_precompiled(opt)
    print("casos positivos correctos (directo y precompilado sin encabezado) en O" + opt, flush=True)
print("Punteros a función C sin typedef con datos apuntados calificados verificados;", root)
