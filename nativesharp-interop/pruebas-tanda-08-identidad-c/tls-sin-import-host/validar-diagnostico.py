"""T08-D04: a C Runtime DLL with static TLS and no import directory is rejected with the cause.

The Windows loader initializes the static TLS of a DLL only when the image has an import directory (see
validar.py, which reproduces it without NativeSharp). NSC cannot repair a third-party DLL without changing it,
so it must not build a consumer that would silently read another module's TLS block:
1. direct CppLibraryReference to the DLL without imports: rejected, the DLL bytes are unchanged;
2. the same source linked with one Win32 import: builds and the consumer reads the initial value 7 (O0-O3);
3. precompiled path: a producer library built against the DLL without imports is rejected in the same way;
   built against the DLL with imports it is consumed without the header and reads 7 (O0 and O2). Replacing
   the transported DLL by the version without imports is caught earlier by the dependency identity check.
"""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-tls-sin-import-diag-", dir=r"A:\nsc-interop-build"))
fixtures = Path(__file__).resolve().parent.parent / "tls-dll-distintas"
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
clang = r"A:\al82dev-clang\bin\clang.exe"
lld = r"A:\al82dev-clang\bin\lld-link.exe"
crt = "A:\\vc2026-runtime\\lib\\x64\\release\\"
sdk = "A:\\vc2026-runtime\\windows-sdk\\10.0.28000.0\\lib\\"
env = os.environ.copy()
env["PATH"] = r"A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
MESSAGE = "has static thread-local storage but no import directory"


def run(command, folder):
    return subprocess.run([str(item) for item in command], cwd=folder, env=env, text=True, encoding="utf-8",
                          errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def require(command, folder, label):
    result = run(command, folder)
    if result.returncode:
        raise AssertionError(f"{label}:\n{result.stdout}")
    return result.stdout


def build_dll(folder, imported):
    folder.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(fixtures / "TlsApi2SinImport.c", folder / "api.c")
    shutil.copyfile(fixtures / "ReadApi.h", folder / "ReadApi.h")
    command = [clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", "api.c", "-o", "api.obj"]
    require(command + (["-DTEST_UNUSED_IMPORT"] if imported else []), folder, "clang api.c")
    require([lld, "/nologo", "/dll", "/noentry", "/nodefaultlib", "/include:_tls_used", "/export:tls_exported,DATA",
             "/out:api.dll", "/implib:api.lib", "api.obj", crt + "libcmt.lib", crt + "libvcruntime.lib",
             sdk + r"ucrt\x64\libucrt.lib", sdk + r"um\x64\kernel32.lib"], folder, "lld-link api.dll")
    return folder / "api.dll"


PRODUCER = "namespace TlsRead { public class Q { public static int Read() { return tls_read(); } } }\n"
REFERENCE = ('<CppLibraryReference Header="ReadApi.h" Language="C" Standard="c11" Library="{0}api.lib" '
             'Runtime="{0}api.dll" />')


def project(folder, name, source, items, library=False):
    (folder / f"{name}.nsproj").write_text(
        f"<Project><PropertyGroup><Name>{name}</Name>" + ("<OutputType>Library</OutputType>" if library else "") +
        f'</PropertyGroup><ItemGroup><Compile Include="{source}" />{items}</ItemGroup></Project>', encoding="utf-8")


def build(folder, name, kind, output, opt="0"):
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", f"--emit-project-oop-{kind}", f"{name}.nsproj",
                "-O" + opt, "--no-artifact-cache", "-o", output], folder)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def expect_rejected(result, label, artifact, dll, before):
    if result.returncode == 0 or MESSAGE not in result.stdout or "internal error" in result.stdout:
        raise AssertionError(f"{label}: se esperaba «{MESSAGE}»:\n{result.stdout}")
    if artifact.exists():
        raise AssertionError(f"{label}: se publicó {artifact.name} pese al rechazo")
    if digest(dll) != before:
        raise AssertionError(f"{label}: NSC modificó la DLL")
    print(label, "rechazado; DLL sin cambios", flush=True)


def check_direct():
    folder = root / "directo"
    without = build_dll(folder / "sin-import", False)
    with_import = build_dll(folder / "con-import", True)
    (folder / "Main.ns").write_text("int Main() { return tls_read() == 7 ? 0 : 1; }\n", encoding="utf-8")
    shutil.copyfile(fixtures / "ReadApi.h", folder / "ReadApi.h")
    project(folder, "Bad", "Main.ns", REFERENCE.format("sin-import/"))
    before = digest(without)
    expect_rejected(build(folder, "Bad", "exe", "out/Bad.exe"), "directo sin importaciones",
                    folder / "out" / "Bad.exe", without, before)
    project(folder, "Good", "Main.ns", REFERENCE.format("con-import/"))
    shutil.copyfile(with_import, folder / "api.dll")
    for opt in ("0", "1", "2", "3"):
        result = build(folder, "Good", "exe", f"out{opt}/Good.exe", opt)
        if result.returncode:
            raise AssertionError(f"directo con importación O{opt}:\n{result.stdout}")
        shutil.copyfile(with_import, folder / f"out{opt}" / "api.dll")
        code = subprocess.run([str(folder / f"out{opt}" / "Good.exe")], cwd=folder / f"out{opt}").returncode
        if code:
            raise AssertionError(f"directo con importación O{opt}: salida {code}")
    print("directo con importación: valor TLS inicial 7 en O0-O3", flush=True)


def check_precompiled():
    folder = root / "precompilado"
    rejected = folder / "productor-sin-import"
    without = build_dll(rejected, False)
    (rejected / "P.ns").write_text(PRODUCER, encoding="utf-8")
    project(rejected, "TlsRead", "P.ns", REFERENCE.format(""), library=True)
    before = digest(without)
    expect_rejected(build(rejected, "TlsRead", "lib", "TlsRead.lib"), "productor precompilado sin importaciones",
                    rejected / "TlsRead.lib", without, before)
    producer = folder / "producer"
    build_dll(producer, True)
    (producer / "P.ns").write_text(PRODUCER, encoding="utf-8")
    project(producer, "TlsRead", "P.ns", REFERENCE.format(""), library=True)
    result = build(producer, "TlsRead", "lib", "TlsRead.lib")
    if result.returncode:
        raise AssertionError(f"productor:\n{result.stdout}")
    original = (producer / "api.dll").read_bytes()
    shutil.copyfile(without, producer / "api.dll")
    (folder / "C.ns").write_text("int Main() { return TlsRead.Q.Read() == 7 ? 0 : 1; }\n", encoding="utf-8")
    project(folder, "C", "C.ns", '<LibraryReference Include="producer/TlsRead.lib" />')
    result = build(folder, "C", "exe", "out/C.exe")
    if result.returncode == 0 or "binary dependency identity mismatch: api.dll" not in result.stdout:
        raise AssertionError(f"sustitución de la DLL transportada:\n{result.stdout}")
    print("DLL transportada sustituida por la versión sin importaciones: rechazada por identidad", flush=True)
    (producer / "api.dll").write_bytes(original)
    for opt in ("0", "2"):
        result = build(folder, "C", "exe", f"out{opt}/C.exe", opt)
        if result.returncode:
            raise AssertionError(f"consumidor precompilado O{opt}:\n{result.stdout}")
        shutil.copyfile(producer / "api.dll", folder / f"out{opt}" / "api.dll")
        code = subprocess.run([str(folder / f"out{opt}" / "C.exe")], cwd=folder / f"out{opt}").returncode
        if code:
            raise AssertionError(f"consumidor precompilado O{opt}: salida {code}")
    print("consumidor precompilado con la DLL con importación: valor TLS inicial 7 en O0 y O2", flush=True)


check_direct()
check_precompiled()
print("Runtime C con TLS y sin importaciones: diagnóstico verificado;", root)
