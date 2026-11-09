"""T08-D10: homonymous, incompatible C typedefs of two precompiled producers in one consumer.

Producer X and producer Y each see one header. Both headers declare `score_t` (`int` / `long long`), `Box` (an
anonymous struct with an `int` / `long long` member) and `Callback` (a pointer to a function over `Box *`). Each
producer publishes a template and a non-generic method that use them. One consumer, without the headers, uses both
producers: every transported template resolves the typedefs in its own producer's catalogue, as C enum constants
and `static` globals already do. Checked O0-O3 (producers and consumer at the same level), against a Clang control,
also with a consumer that has its own header with yet another `score_t`.

Rejections (O0): the consumer's own code still finds `score_t` and `Box` ambiguous, with and without its own
header; one live project that references both headers still rejects every use.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-typedef-productores-", dir=r"A:\nsc-interop-build"))
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

SIDES = {
    "X": {
        "header": ("typedef int score_t;\ntypedef struct { int a; } Box;\ntypedef int (*Callback)(Box *b);\n"
                   "int x_score(score_t s);\nint x_box(Box b);\nint x_call(Callback cb, Box *b);\n"
                   "int x_twice(Box *b);\nCallback x_callback(void);\n"),
        "source": ('#include "x.h"\nint x_score(score_t s) { return s + 1; }\nint x_box(Box b) { return b.a + 2; }\n'
                   "int x_call(Callback cb, Box *b) { return cb(b); }\nint x_twice(Box *b) { return b->a * 2; }\n"
                   "Callback x_callback(void) { return x_twice; }\n"),
        "body": """        score_t s = (score_t)2147483647;
        if (x_score((score_t)41) != 42 || sizeof(score_t) != 4 || s != 2147483647) return 11;
        Box b;
        b.a = 5;
        if (x_box(b) != 7 || sizeof(Box) != 4) return 12;
        Callback cb = x_callback();
        if (x_call(cb, &b) != 10) return 13;
        return 0;
""",
        "twice": "public static score_t Twice(score_t s) { return s * (score_t)2; }",
        "check": "SideX.Bridge.Twice(21) != 42",
    },
    "Y": {
        "header": ("typedef long long score_t;\ntypedef struct { long long a; } Box;\n"
                   "typedef long long (*Callback)(Box *b);\nlong long y_score(score_t s);\nlong long y_box(Box b);\n"
                   "long long y_call(Callback cb, Box *b);\nlong long y_twice(Box *b);\nCallback y_callback(void);\n"),
        "source": ('#include "y.h"\nlong long y_score(score_t s) { return s + 3; }\n'
                   "long long y_box(Box b) { return b.a + 4; }\n"
                   "long long y_call(Callback cb, Box *b) { return cb(b); }\n"
                   "long long y_twice(Box *b) { return b->a * 2; }\nCallback y_callback(void) { return y_twice; }\n"),
        "body": """        score_t s = (score_t)4294967296L;
        if (y_score(s) != 4294967299L || sizeof(score_t) != 8) return 21;
        Box b;
        b.a = s;
        if (y_box(b) != 4294967300L || sizeof(Box) != 8) return 22;
        Callback cb = y_callback();
        if (y_call(cb, &b) != 8589934592L) return 23;
        return 0;
""",
        "twice": "public static score_t Twice(score_t s) { return s * (score_t)2; }",
        "check": "SideY.Bridge.Twice(4294967296L) != 8589934592L",
    },
}
CONSUMER = ("int Main()\n{\n    if (SideX.Bridge.Run<int>(0) != 0) return 1;\n"
            "    if (SideY.Bridge.Run<int>(0) != 0) return 2;\n"
            "    if (" + SIDES["X"]["check"] + " || " + SIDES["Y"]["check"] + ") return 3;\n    return 0;\n}\n")


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def c_side(folder, side):
    name = side.lower()
    folder.mkdir(parents=True, exist_ok=True)
    (folder / f"{name}.h").write_text(SIDES[side]["header"], encoding="utf-8")
    (folder / f"c{name}.c").write_text(SIDES[side]["source"], encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", f"c{name}.c", "-o", f"c{name}.obj"], folder)
    run([lib, "/nologo", f"/out:c{name}.lib", f"c{name}.obj"], folder)


def reference(side, prefix=""):
    name = side.lower()
    return (f'<CppLibraryReference Header="{prefix}{name}.h" Language="C" Standard="c11" '
            f'Library="{prefix}c{name}.lib" />')


def project(folder, name, sources, refs, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        "</PropertyGroup><ItemGroup>" + "".join(f'<Compile Include="{s}" />' for s in sources) + refs +
        "</ItemGroup></Project>", encoding="utf-8")


def nsc_build(folder, name, opt, kind, output):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder, check=False)


def errors(result):
    return [l.split(": error: ")[-1] for l in result.stdout.splitlines() if "error" in l][:3]


def producers(base, opt):
    for side in SIDES:
        folder = base / side
        c_side(folder, side)
        (folder / "Producer.ns").write_text(
            f"namespace Side{side} {{ public class Bridge {{\n    public static int Run<T>(T unused) {{\n" +
            SIDES[side]["body"] + "    }\n    " + SIDES[side]["twice"] + "\n} }\n", encoding="utf-8")
        project(folder, f"Side{side}", ["Producer.ns"], reference(side), library=True)
        result = nsc_build(folder, f"Side{side}", opt, "lib", f"Side{side}.lib")
        if result.returncode:
            raise AssertionError(f"productor {side} O{opt}:\n{result.stdout}")
        (folder / f"{side.lower()}.h").unlink()


def consumer(base, opt, extra_refs="", source=CONSUMER, name="Consumer"):
    folder = base / name
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "Consumer.ns").write_text(source, encoding="utf-8")
    project(folder, "Consumer", ["Consumer.ns"], '<LibraryReference Include="../X/SideX.lib" />'
            '<LibraryReference Include="../Y/SideY.lib" />' + extra_refs)
    return folder, nsc_build(folder, "Consumer", opt, "exe", "out/Consumer.exe")


def expect_run(label, folder, result):
    if result.returncode:
        if observe:
            print(label, errors(result), flush=True)
            return
        raise AssertionError(f"{label}: compilación:\n{result.stdout}")
    code = run([folder / "out" / "Consumer.exe"], Path("A:/"), check=False).returncode
    if code:
        raise AssertionError(f"{label}: comprobación {code} falló")
    print(label, "correcto", flush=True)


def expect_reject(label, result, text):
    if observe:
        print(label, result.returncode, errors(result), flush=True)
        return
    if result.returncode == 0 or text not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"{label}: se esperaba «{text}»:\n{result.stdout}")
    print(label, "correcto", flush=True)


control = root / "control"
for side in SIDES:
    c_side(control, side)
(control / "Control.c").write_text(
    "typedef struct { int a; } BoxX;\ntypedef struct { long long a; } BoxY;\n"
    "int x_box(BoxX b);\nlong long y_box(BoxY b);\nint x_score(int s);\nlong long y_score(long long s);\n"
    "__declspec(dllimport) void __stdcall ExitProcess(unsigned long);\n"
    "static int checks(void) { BoxX x = {5}; BoxY y = {4294967296LL};\n"
    "    return x_box(x) == 7 && y_box(y) == 4294967300LL && x_score(41) == 42 && y_score(1) == 4 ? 0 : 1; }\n"
    "void mainCRTStartup(void) { ExitProcess(checks()); }\n", encoding="utf-8")
run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c", "-o", "Control.obj"], control)
run([linker, "/nologo", "/entry:mainCRTStartup", "/subsystem:console", "/nodefaultlib", "/out:Control.exe",
     "Control.obj", "cx.lib", "cy.lib", sdk / "um/x64/kernel32.lib"], control)
if run([control / "Control.exe"], control, check=False).returncode:
    raise AssertionError("control C falló")
print("control C correcto", flush=True)

for opt in opts:
    base = root / f"O{opt}"
    producers(base, opt)
    folder, result = consumer(base, opt)
    expect_run(f"consumidor sin encabezados O{opt}", folder, result)
    own = base / "propio"
    own.mkdir()
    (own / "own.h").write_text("typedef short score_t;\nint own_value(void);\n", encoding="utf-8")
    (own / "own.c").write_text('#include "own.h"\nint own_value(void) { return 0; }\n', encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "own.c", "-o", "own.obj"], own)
    run([lib, "/nologo", "/out:own.lib", "own.obj"], own)
    folder, result = consumer(base, opt, '<CppLibraryReference Header="../propio/own.h" Language="C" '
                              'Standard="c11" Library="../propio/own.lib" />',
                              CONSUMER.replace("return 0;\n}", "return own_value();\n}"), "ConEncabezado")
    expect_run(f"consumidor con encabezado propio O{opt}", folder, result)

base = root / "O0"
for index, (label, source, extra) in enumerate((
        ("rechazo propio sin encabezado score_t", "int Main() { score_t s = 1; return 0; }\n", ""),
        ("rechazo propio sin encabezado Box", "int Main() { Box b; return 0; }\n", ""),
        ("rechazo propio con encabezado score_t", "int Main() { score_t s = 1; return 0; }\n",
         '<CppLibraryReference Header="../propio/own.h" Language="C" Standard="c11" Library="../propio/own.lib" />'))):
    name = f"Rechazo{index}"
    _, result = consumer(base, "0", extra, source, name)
    expect_reject(label, result, "conflicting C/C++ type alias '" + ("Box" if "Box" in label else "score_t") + "'")

live = root / "vivo"
for side in SIDES:
    c_side(live, side)
(live / "Main.ns").write_text("int Main() { score_t s = 1; return 0; }\n", encoding="utf-8")
project(live, "Live", ["Main.ns"], reference("X") + reference("Y"))
expect_reject("rechazo proyecto vivo con los dos encabezados", nsc_build(live, "Live", "0", "exe", "out/Live.exe"),
              "conflicting C/C++ type alias 'score_t'")
print("observación terminada" if observe else "Typedef C homónimos de productores distintos verificados:", root)
