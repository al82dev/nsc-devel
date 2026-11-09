"""T08-002-09m7d2: a template that names a C global its producer could not import.

`extern const int **pp;` has no NativeSharp projection yet (nested pointee qualifier). The producer used to
publish its template without any record of that declaration, so a consumer joined the name in silence to
another producer's `extern int **pp;`. The producer now records the declaration it saw and the consumer keeps
it as an unimportable state. Checked (consumers without producer headers):
- Clang control: `int **pp` and `const int **pp` are conflicting declarations of one object;
- the two producers above: the join is rejected as ambiguous (O0);
- only the `const int **` producer: the use is rejected with the import reason (O0);
- the unimportable global shadowed by a public static field or by a local of the template, and a template of the
  same producer that only uses another, importable global: they build and run, O0-O3.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-global-no-importable-", dir=r"A:\nsc-interop-build"))
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lib = r"A:\al82dev-clang\bin\llvm-lib.exe"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = lib
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
opts = [a for a in sys.argv[1:] if not a.startswith("--")] or ["0", "1", "2", "3"]
FIXTURE = "int v = 22; int *p = &v; int **pp = &p; int other = 31;\n"
PLAIN = "extern int **pp;\nextern int other;\n"
QUALIFIED = "extern const int **pp;\nextern int other;\n"


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args}:\n{result.stdout}")
    return result


def producer(folder, name, header, body):
    folder.mkdir()
    (folder / "same.h").write_text(header, encoding="utf-8")
    (folder / "Producer.ns").write_text(f"namespace {name} {{ public class Bridge {{ {body} }} }}\n",
                                        encoding="utf-8")
    (folder / "Producer.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name><OutputType>Library</OutputType></PropertyGroup><ItemGroup>"
        '<Compile Include="Producer.ns" /><CppLibraryReference Header="same.h" Language="C" Standard="c11" '
        'Library="../fixture.lib" />'
        "</ItemGroup></Project>", encoding="utf-8")
    run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Producer.nsproj", "-O" + opt,
         "--no-artifact-cache", "-o", f"{name}.lib"], folder)
    (folder / "same.h").unlink()


def fixture(folder):
    folder.mkdir(parents=True)
    (folder / "fixture.c").write_text(FIXTURE, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "fixture.c", "-o", "fixture.obj"], folder)
    run([lib, "/nologo", "/out:fixture.lib", "fixture.obj"], folder)


def consume(folder, producers, expression):
    (folder / "Consumer.ns").write_text(f"int Main() {{ return {expression}; }}\n", encoding="utf-8")
    references = "".join(f'<LibraryReference Include="{p}/{p}.lib" />' for p in producers)
    (folder / "Consumer.nsproj").write_text(
        f"<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup>"
        f'<Compile Include="Consumer.ns" />{references}'
        "</ItemGroup></Project>", encoding="utf-8")
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Consumer.nsproj", "-O" + opt,
                "--no-artifact-cache", "-o", "out/Consumer.exe"], folder, check=False)


def expect_rejection(name, producers, expression, message):
    folder = root / f"{name}-O{opt}"
    fixture(folder)
    for producer_name, header, body in producers:
        producer(folder / producer_name, producer_name, header, body)
    result = consume(folder, [p[0] for p in producers], expression)
    if result.returncode == 0 or message not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"{name}: se esperaba «{message}»:\n{result.stdout}")
    print(name, "rechazado", flush=True)


def expect_value(name, producers, expression, expected):
    folder = root / f"{name}-O{opt}"
    fixture(folder)
    for producer_name, header, body in producers:
        producer(folder / producer_name, producer_name, header, body)
    result = consume(folder, [p[0] for p in producers], expression)
    if result.returncode:
        raise AssertionError(f"{name} O{opt}: consumidor:\n{result.stdout}")
    code = run([folder / "out" / "Consumer.exe"], Path("A:/"), check=False).returncode
    if code != expected:
        raise AssertionError(f"{name} O{opt}: se esperaba {expected}, se obtuvo {code}")
    print(name, f"O{opt}", "correcto", flush=True)


control = root / "control"
control.mkdir()
(control / "Control.c").write_text(PLAIN + QUALIFIED, encoding="utf-8")
result = run([clang, "-fsyntax-only", "--target=x86_64-pc-windows-msvc", "-std=c11", "Control.c"], control, False)
if result.returncode == 0 or "redeclaration of 'pp' with a different type" not in result.stdout:
    raise AssertionError("control C: se esperaba «redeclaration of 'pp' with a different type»:\n" + result.stdout)
print("control C: declaraciones incompatibles", flush=True)

READ_PP = "public static int Read<T>(T unused) { return pp == null ? 1 : **pp; }"
for opt in opts:
    if opt == "0":
        # Before the fix the second join bound QualifiedB's template to PlainA's `int **pp` (22022).
        expect_rejection("rechazo-dos-productores", [("PlainA", PLAIN, READ_PP), ("QualifiedB", QUALIFIED, READ_PP)],
                         "PlainA.Bridge.Read<int>(0) * 1000 + QualifiedB.Bridge.Read<int>(0)",
                         "ambiguous namespace state reference 'pp'")
        expect_rejection("rechazo-un-productor", [("QualifiedB", QUALIFIED, READ_PP)], "QualifiedB.Bridge.Read<int>(0)",
                         "cannot import C/C++ global 'pp'")
    expect_value("campo-estatico", [("FieldB", QUALIFIED,
                 "public static int pp = 7; public static int Read<T>(T unused) { return pp; }")],
                 "FieldB.Bridge.Read<int>(0)", 7)
    expect_value("local", [("LocalB", QUALIFIED,
                 "public static int Read<T>(T unused) { int pp = 5; return pp; }")],
                 "LocalB.Bridge.Read<int>(0)", 5)
    expect_value("otra-global", [("OtherB", QUALIFIED,
                 "public static int Read<T>(T unused) { return other; }")],
                 "OtherB.Bridge.Read<int>(0)", 31)
print("Global C no importable en plantillas verificada:", root)
