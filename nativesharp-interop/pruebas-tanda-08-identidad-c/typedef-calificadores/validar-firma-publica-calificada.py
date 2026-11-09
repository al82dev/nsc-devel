"""T08-D02 and T08-D03: pointee qualifiers of public signatures across NSMETA.

A precompiled producer publishes, with `T const*` and with C aliases (`typedef const Payload *CPayload`,
`typedef volatile Payload *VPayload`, `typedef const volatile Payload *CVPayload`):
- methods whose result and parameters are qualified pointers, a static field and a namespace function;
- delegates `VHandler(VPayload)` and `CHandler(CPayload)`;
- a callback type with a volatile pointee (`TakeV`), as a result and as a parameter (T08-D03);
- a C callback type with a const pointee (`TakeC`) built from a published `CPayload` method.
A consumer without the header must keep the same contract as the producer:
- positives (O0-O3): reads, implicit addition of qualifiers, delegates bound to the published methods, the
  callbacks returned and passed back, and the published `ReadC`/`ReadV` converted to the C callback types
  `TakeC`/`TakeV` (before, the consumer saw `ReadC` with a plain pointer and could not convert it); the
  reads through a `VPayload` emit `load volatile`;
- negatives (O0): writing through a `const` pointee, discarding `const` or `volatile`, binding a method with a
  plain pointer to `VHandler` or to the C callback types, writing through a `CHandler` lambda parameter.
The metadata-source spelling `T volatile*` is not NativeSharp source: user code that writes it is rejected.
The same bodies run first in the producer's own project (control with the header).
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-firma-calificada-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
observe = "--observar" in sys.argv
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]

HEADER = """typedef struct Payload { int value; } Payload;
typedef const Payload *CPayload;
typedef volatile Payload *VPayload;
typedef const volatile Payload *CVPayload;
typedef int (*TakeV)(volatile Payload *);
typedef int (*TakeC)(const Payload *);
CPayload get_c(void);
VPayload get_v(void);
Payload *get_p(void);
int take_v(volatile Payload *p);
"""
SOURCE = """#include "api.h"
static Payload storage = {40};
CPayload get_c(void) { return &storage; }
VPayload get_v(void) { return &storage; }
Payload *get_p(void) { return &storage; }
int take_v(volatile Payload *p) { return p->value + 2; }
"""
PRODUCER = """namespace Pub
{
    public class Q
    {
        public static int const* IntPtr;
        public static int const* GetInt() { return IntPtr; }
        public static CPayload GetC() { return get_c(); }
        public static VPayload GetV() { return get_v(); }
        public static CVPayload GetCV() { return get_v(); }
        public static int ReadC(CPayload p) { return p->value; }
        public static int ReadV(VPayload p) { return p->value; }
        public static int ReadCV(CVPayload p) { return p->value; }
        public static int ReadPlain(Payload* p) { return p->value; }
        public static Payload* GetP() { return get_p(); }
        public static TakeV Callback() { return take_v; }
        public static TakeC CallbackC() { return ReadC; }
        public static int Apply(TakeV f, VPayload p) { return f(p); }
    }
    public delegate int VHandler(VPayload p);
    public delegate int CHandler(CPayload p);
    public int ReadBoth(CPayload c, VPayload v) { return c->value + v->value; }
}
"""
POSITIVE = """    int local = 7;
    Pub.Q.IntPtr = &local;
    if (*Pub.Q.GetInt() != 7) return 1;
    if (Pub.Q.ReadC(Pub.Q.GetC()) != 40 || Pub.Q.ReadV(Pub.Q.GetV()) != 40) return 2;
    if (Pub.Q.ReadCV(Pub.Q.GetCV()) != 40 || Pub.Q.ReadCV(Pub.Q.GetV()) != 40) return 3;
    if (Pub.Q.ReadC(Pub.Q.GetP()) != 40 || Pub.Q.ReadV(Pub.Q.GetP()) != 40) return 4;
    var v = Pub.Q.GetV();
    v->value = 41;
    if (v->value != 41) return 5;
    v->value = 40;
    Pub.VHandler h = Pub.Q.ReadV;
    Pub.CHandler c = Pub.Q.ReadC;
    Pub.CHandler lambda = (p) => p->value + 1;
    if (h(Pub.Q.GetV()) != 40 || c(Pub.Q.GetC()) != 40 || lambda(Pub.Q.GetC()) != 41) return 6;
    var f = Pub.Q.Callback();
    if (f(Pub.Q.GetV()) != 42 || Pub.Q.Apply(f, Pub.Q.GetV()) != 42) return 7;
    if (Pub.ReadBoth(Pub.Q.GetC(), Pub.Q.GetV()) != 80) return 8;
    var g = Pub.Q.CallbackC();
    if (g(Pub.Q.GetC()) != 40) return 9;
    g = Pub.Q.ReadC;
    f = Pub.Q.ReadV;
    if (g(Pub.Q.GetC()) != 40 || f(Pub.Q.GetV()) != 40) return 10;
    return 0;
"""
NEGATIVES = [
    ("escritura-int-const", "*Pub.Q.GetInt() = 1; return 0;", "not assignable"),
    ("escritura-alias-const", "Pub.Q.GetC()->value = 1; return 0;", "not assignable"),
    ("escritura-campo-const", "*Pub.Q.IntPtr = 1; return 0;", "not assignable"),
    ("descarta-const", "Payload* p = Pub.Q.GetC(); return 0;", "would discard pointee const"),
    ("descarta-volatile", "Payload* p = Pub.Q.GetV(); return 0;", "would discard pointer pointee volatile"),
    ("descarta-const-volatile", "Payload* p = Pub.Q.GetCV(); return 0;", "would discard"),
    ("argumento-descarta-volatile", "return Pub.Q.ReadPlain(Pub.Q.GetV());", "no viable overload"),
    ("delegate-metodo-plano", "Pub.VHandler h = Pub.Q.ReadPlain; return 0;", "no method in the method group"),
    ("callback-c-metodo-plano", "var g = Pub.Q.CallbackC(); g = Pub.Q.ReadPlain; return 0;",
     "no method in the method group"),
    ("callback-v-metodo-plano", "var f = Pub.Q.Callback(); f = Pub.Q.ReadPlain; return 0;",
     "no method in the method group"),
    ("delegate-lambda-escribe", "Pub.CHandler c = (p) => { p->value = 1; return 0; }; return c(Pub.Q.GetC());",
     "not assignable"),
]
USER_SPELLING = "int Main() { int x = 1; int volatile* p = &x; return 0; }"


def run(args, cwd):
    return subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                          errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def c_library(folder):
    (folder / "api.h").write_text(HEADER, encoding="utf-8")
    (folder / "api.c").write_text(SOURCE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"], folder)
    run([lib, "/nologo", "/out:capi.lib", "api.obj"], folder)


REF = '<CppLibraryReference Header="api.h" Language="C" Standard="c11" Library="capi.lib" />'


def project(folder, name, source, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />' + refs + "</ItemGroup></Project>",
        encoding="utf-8")


def build(folder, name, opt, kind, output, extra=()):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", *extra, "-o", output], folder)


def errors(result):
    return [l.split(": error: ")[-1][:150] for l in result.stdout.splitlines() if "error" in l][:2]


def expect_run(label, folder, name, opt):
    result = build(folder, name, opt, "exe", f"out/{name}.exe")
    code = run([folder / "out" / f"{name}.exe"], Path("A:/")).returncode if result.returncode == 0 else None
    if observe:
        print(label, result.returncode, code, errors(result), flush=True)
        return
    if result.returncode or code:
        raise AssertionError(f"{label}: build {result.returncode}, salida {code}:\n{result.stdout}")
    print(label, "correcto", flush=True)


def expect_reject(label, result, text):
    if observe:
        print(label, result.returncode, errors(result), flush=True)
        return
    if result.returncode == 0 or (text and text not in result.stdout) or "internal error" in result.stdout:
        raise AssertionError(f"{label}: se esperaba «{text}»:\n{result.stdout}")
    print(label, "rechazado", flush=True)


def producer(opt):
    folder = root / f"O{opt}" / "producer"
    folder.mkdir(parents=True)
    c_library(folder)
    (folder / "P.ns").write_text(PRODUCER, encoding="utf-8")
    project(folder, "Pub", "P.ns", REF, library=True)
    result = build(folder, "Pub", opt, "lib", "Pub.lib")
    if result.returncode:
        raise AssertionError(f"productor O{opt}:\n{result.stdout}")
    (folder / "api.h").unlink()
    return folder


def direct(opt):
    folder = root / f"O{opt}" / "directo"
    folder.mkdir(parents=True)
    c_library(folder)
    (folder / "Main.ns").write_text(PRODUCER + "int Main()\n{\n" + POSITIVE + "}\n", encoding="utf-8")
    project(folder, "D", "Main.ns", REF)
    expect_run(f"directo O{opt}", folder, "D", opt)


def consumer(opt, producer_folder, name, body):
    folder = producer_folder.parent / name
    folder.mkdir()
    (folder / "C.ns").write_text("int Main()\n{\n" + body + "\n}\n", encoding="utf-8")
    project(folder, "C", "C.ns", '<LibraryReference Include="../producer/Pub.lib" />')
    return folder


def check_volatile_ir(producer_folder):
    folder = consumer("1", producer_folder, "ir",
                      "    var v = Pub.Q.GetV();\n    v->value = 3;\n    return v->value == 3 ? 0 : 1;")
    result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--dump-project-oop-llvm-opt", "C.nsproj", "-O1"],
                 folder)
    loads = [l for l in result.stdout.splitlines() if "load volatile i32" in l]
    stores = [l for l in result.stdout.splitlines() if "store volatile i32" in l]
    if observe:
        print("IR volatile", result.returncode, len(loads), len(stores), flush=True)
        return
    if result.returncode or not loads or not stores:
        raise AssertionError(f"IR sin accesos volatile por VPayload:\n{result.stdout[-3000:]}")
    print("IR volatile por VPayload correcto:", len(loads), "lecturas,", len(stores), "escrituras", flush=True)


def check_user_spelling():
    folder = root / "escritura-usuario"
    folder.mkdir()
    (folder / "U.ns").write_text(USER_SPELLING + "\n", encoding="utf-8")
    project(folder, "U", "U.ns", "")
    expect_reject("rechazo T volatile* en codigo de usuario", build(folder, "U", "0", "exe", "out/U.exe"), None)


for opt in opts:
    direct(opt)
    producer_folder = producer(opt)
    expect_run(f"consumidor sin encabezado O{opt}", consumer(opt, producer_folder, "positivo", POSITIVE), "C", opt)
    if opt == "0":
        for name, body, text in NEGATIVES:
            folder = consumer(opt, producer_folder, name, "    " + body)
            expect_reject(f"rechazo {name}", build(folder, "C", "0", "exe", "out/C.exe"), text)
        check_volatile_ir(producer_folder)
check_user_spelling()
print("observación terminada" if observe else "Firmas públicas calificadas verificadas:", root)
