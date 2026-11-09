"""T08-008-02: C `void` in signatures, `void*` and its qualifiers.

a) `void` return and `f(void)` prototypes: calls for effect, a counter read back, and rejection of extra
   arguments and of using a `void` result as a value.
b) `void*` as parameter, return, struct field and global, with round trips through C and a cast back to
   `int*`. A `const void*` field is included; a `volatile void*` field is not imported yet (recorded under
   T08-010-04b), so `volatile void*` is checked as parameter, return and local.
c) `const void*` and `volatile void*` keep their qualifiers: adding them is implicit, dropping them is rejected
   (Clang only warns), and the values reach C unchanged.
d) The same body as a template of a precompiled producer consumed without the header (signatures restored
   from metadata).
Positive cases run in a Clang control, directly with the header and through the precompiled template, O0-O3.
Rejections run in O0, each with the Clang verdict for the same statements.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-void-", dir=r"A:\nsc-interop-build"))
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

HEADER = """typedef struct Holder { void *p; const void *cp; } Holder;
extern void *g_p;
extern const void *g_cp;
void reset(void);
int counter(void);
void bump(int by);
void *identity(void *p);
const void *cidentity(const void *p);
volatile void *videntity(volatile void *p);
int read_int(const void *p);
int read_vint(volatile void *p);
void write_int(void *p, int v);
void *get_buffer(void);
void store(Holder *h, void *p);
int holder_ok(const Holder *h, const void *expected);
"""

SOURCE = """#include "api.h"
void *g_p;
const void *g_cp;
static int count;
static int buffer[2] = {11, 22};
void reset(void) { count = 0; }
int counter(void) { return count; }
void bump(int by) { count += by; }
void *identity(void *p) { return p; }
const void *cidentity(const void *p) { return p; }
volatile void *videntity(volatile void *p) { return p; }
int read_int(const void *p) { return *(const int *)p; }
int read_vint(volatile void *p) { return *(volatile int *)p; }
void write_int(void *p, int v) { *(int *)p = v; }
void *get_buffer(void) { return buffer; }
void store(Holder *h, void *p) { h->p = p; h->cp = p; }
int holder_ok(const Holder *h, const void *expected)
{ return h->p == expected && h->cp == expected; }
"""

BODY = """    reset();
    bump(3);
    bump(4);
    if (counter() != 7) return 1;
    int x = 5;
    void* p = &x;
    if (identity(p) != p || read_int(p) != 5) return 2;
    write_int(p, 9);
    if (x != 9) return 3;
    const void* cp = p;
    if (cidentity(cp) != cp || read_int(cp) != 9) return 4;
    volatile void* vp = p;
    if (videntity(vp) != vp || read_vint(vp) != 9) return 5;
    int* back = (int*)identity(p);
    if (*back != 9) return 6;
    int* buffer = (int*)get_buffer();
    if (buffer[0] != 11 || buffer[1] != 22 || read_int(get_buffer()) != 11) return 7;
    g_p = p;
    g_cp = get_buffer();
    if (g_p != p || read_int(g_p) != 9 || read_int(g_cp) != 11) return 8;
    Holder h;
    store(&h, p);
    if (h.p != p || h.cp != cp || holder_ok(&h, cp) != 1) return 9;
    h.p = get_buffer();
    h.cp = h.p;
    vp = h.p;
    if (holder_ok(&h, get_buffer()) != 1 || read_vint(vp) != 11) return 10;
    g_p = null;
    if (g_p != null || identity(null) != null) return 11;
    return 0;
"""

# (name, statements, NativeSharp diagnostic, Clang rejects the same statements)
REJECTIONS = [
    ("argumento-en-f-void", "return counter(1);", "no overload of namespace function 'counter' accepts 1 argument(s)",
     True),
    ("resultado-void", "int v = reset(); return v;",
     "cannot initialize local 'v' of type 'builtin:int' from 'builtin:void'", True),
    ("aridad-bump", "bump(); return 0;", "no overload of namespace function 'bump' accepts 0 argument(s)", True),
    ("quitar-const", "int x = 1; const void* cp = &x; void* p = cidentity(cp); return read_int(p);",
     "would discard pointee constness", False),
    ("quitar-volatile", "int x = 1; volatile void* vp = &x; void* p = videntity(vp); return read_int(p);",
     "initialization would discard pointer pointee volatile qualification", False),
    ("desreferenciar-void", "int x = 1; void* p = &x; return *p;", "operand of * must be a non-void pointer", True),
]


def c_spelling(text):
    return text.replace("null", "0")


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
    run([lib, "/nologo", "/out:cvoid.lib", "api.obj"], folder)


HEADER_REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="cvoid.lib" />'


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
         "control.obj", "cvoid.lib", sdk / "um/x64/kernel32.lib"], folder)
    code = run([folder / "control.exe"], folder, check=False).returncode
    if code:
        raise AssertionError(f"control Clang: paso {code}")
    for name, statements, _, c_rejects in REJECTIONS:
        probe = folder / f"rechazo-{name}.c"
        probe.write_text(f'#include "api.h"\nint probe(void) {{ {statements} }}\n', encoding="utf-8")
        result = run([clang, "--target=x86_64-pc-windows-msvc", "-std=c11", "-fsyntax-only", probe.name], folder,
                     check=False)
        if observe:
            print("Clang", name, result.returncode, flush=True)
        elif (result.returncode != 0) != c_rejects:
            raise AssertionError(f"Clang {name}: código {result.returncode}:\n{result.stdout}")
    print("control Clang correcto", flush=True)


def direct(opt):
    folder = root / f"directo-O{opt}"
    folder.mkdir()
    c_library(folder)
    (folder / "Main.ns").write_text("int Main()\n{\n" + BODY + "}\n", encoding="utf-8")
    project(folder, "Direct", ["Main.ns"], HEADER_REF)
    build_and_run(f"directo O{opt}", folder, "Direct", opt)


def precompiled(opt):
    folder = root / f"precompilado-O{opt}"
    producer = folder / "producer"
    producer.mkdir(parents=True)
    c_library(producer)
    (producer / "Producer.ns").write_text(
        "namespace Voids { public class Bridge { public static int Run<T>(T unused)\n{\n" + BODY + "} } }\n",
        encoding="utf-8")
    project(producer, "VoidBridge", ["Producer.ns"], HEADER_REF, library=True)
    result = nsc_build(producer, "VoidBridge", opt, "lib", "VoidBridge.lib")
    if result.returncode:
        report(f"productor O{opt}", result, None)
        return
    (producer / "api.h").unlink()
    (folder / "Consumer.ns").write_text("int Main() { return Voids.Bridge.Run<int>(0); }\n", encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="producer/VoidBridge.lib" />')
    build_and_run(f"precompilado O{opt}", folder, "Consumer", opt)


def rejections():
    for name, statements, text, _ in REJECTIONS:
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
print("observación terminada" if observe else "void C verificado:", root)
