"""T08-D05: homonymous C TLS DLLs whose transitive dependencies are also homonymous and different.

Two folders each hold an `api.dll` with exported TLS and an `api_dep()` that returns `dep_value()` of its own
`dep.dll` (101 in one folder, 102 in the other). One precompiled producer per folder; the consumer reads both
TLS variables and both `api_dep()` without the headers. The loader reuses a module already loaded by base
name, so before T08-D05 NSC rejected the package ("runtime package contains different DLLs named 'dep.dll'").
Now each TLS alias receives a private assembly (`NativeSharp.Isolation.*`) with its own `dep.dll` and an
activation context manifest that the alias loader activates around LoadLibraryExW. No DLL is modified.
Checks:
1. O0-O3: the consumer runs and observes 101 and 102 plus each TLS initial value;
   the package holds two assemblies whose `dep.dll` are byte-identical to their sources and no root `dep.dll`;
2. artifact cache: the second build restores the executable, the aliases, the assemblies and the manifests;
3. control: with equal `dep.dll` contents there is a single root `dep.dll` and no isolation artifact;
4. negative: when a DLL that is not a TLS alias (loaded by the import table) also reaches a different
   `dep.dll`, the package is still rejected with the existing diagnostic;
5. negative: removing an isolation manifest after publication makes the alias loader report a failure instead
   of silently binding the other `dep.dll` (the consumer must not return 0);
6. a NativeSharp DLL product consuming both producers packages the same isolation next to itself, and a host
   that loads it (Python ctypes) observes 101 and 102 (O0 and O2);
7. T08-D05b: a function exported by only one of the homonymous DLLs (`only_one`) is bound at run time by the
   alias loader when that alias needs isolated dependencies; before, the import table loaded the alias before any
   activation context existed and the package was rejected ("is imported directly"). O0-O3 executable and DLL
   product: the product import table does not name the alias, both dependencies stay isolated, and removing the
   alias manifest makes the first call fail instead of binding the other `dep.dll`;
8. T08-D05b control: with equal `dep.dll` contents no isolation is needed and the exclusive function keeps its
   direct import (the alias stays in the import table).
"""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-transitivas-homonimas-", dir=r"A:\nsc-interop-build"))
clang = r"A:\al82dev-clang\bin\clang.exe"
linker = r"A:\al82dev-clang\bin\lld-link.exe"
nsc = r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe"
readobj = r"A:\al82dev-clang\bin\llvm-readobj.exe"
crt = Path(r"A:\vc2026-runtime\lib\x64\release")
sdk = Path(r"A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib")
env = os.environ.copy()
env["PATH"] = r"A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
opts = [a for a in sys.argv[1:]] or ["0", "1", "2", "3"]


def run(args, cwd, check=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and result.returncode:
        raise AssertionError(f"{args[0]} (código {result.returncode}):\n{result.stdout}")
    return result


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def c_dll(folder, name, source, libs=()):
    (folder / f"{name}.c").write_text(source, encoding="utf-8")
    run([clang, "-c", "--target=x86_64-pc-windows-msvc", "-std=c11", f"{name}.c", "-o", f"{name}.obj"], folder)
    extra = ["/include:_tls_used", "/export:tls_exported,DATA"] if "tls_exported" in source else []
    run([linker, "/nologo", "/dll", "/noentry", "/nodefaultlib", *extra, f"/out:{name}.dll", f"/implib:{name}.lib",
         f"{name}.obj", *libs, crt / "libcmt.lib", crt / "libvcruntime.lib", sdk / "ucrt/x64/libucrt.lib",
         sdk / "um/x64/kernel32.lib"], folder)


def producer(folder, index, dep_value, unique=False, dep_from=None):
    folder.mkdir(parents=True)
    if dep_from is None:
        c_dll(folder, "dep", f"__declspec(dllexport) int dep_value(void) {{ return {dep_value}; }}\n"
                             "__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);\n"
                             "__declspec(dllexport) unsigned long dep_thread(void) { return GetCurrentThreadId(); }\n")
    else:
        for name in ("dep.dll", "dep.lib"):
            shutil.copyfile(dep_from / name, folder / name)
    (folder / "dep.h").write_text("__declspec(dllimport) int dep_value(void);\n", encoding="utf-8")
    (folder / "api.h").write_text("extern __declspec(thread) int tls_exported;\n"
                                  "__declspec(dllimport) int api_dep(void);\n" +
                                  ("__declspec(dllimport) int only_one(void);\n" if unique else ""), encoding="utf-8")
    c_dll(folder, "api", f"__declspec(thread) int tls_exported = {index};\n"
                         "__declspec(dllimport) int dep_value(void);\n"
                         "__declspec(dllexport) int api_dep(void) { return dep_value(); }\n" +
                         ("__declspec(dllexport) int only_one(void) { return dep_value(); }\n" if unique else ""),
          ["dep.lib"])
    (folder / "Producer.ns").write_text(
        f"namespace Dep{index} {{ public class Bridge {{\n"
        "  public static int Tls<T>(T unused) { return tls_exported; }\n"
        "  public static int Dep<T>(T unused) { return api_dep(); }\n" +
        ("  public static int Only<T>(T unused) { return only_one(); }\n" if unique else "") + "} }\n",
        encoding="utf-8")
    (folder / "Producer.nsproj").write_text(
        f"<Project><PropertyGroup><Name>Dep{index}</Name><OutputType>Library</OutputType></PropertyGroup>"
        '<ItemGroup><Compile Include="Producer.ns" /><CppLibraryReference Header="api.h" Language="C" '
        'Standard="c11" Library="api.lib" Runtime="api.dll" /><CppLibraryReference Header="dep.h" Language="C" '
        'Standard="c11" Library="dep.lib" Runtime="dep.dll" /></ItemGroup></Project>', encoding="utf-8")
    run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-lib", "Producer.nsproj", "-O0",
         "--no-artifact-cache", "-o", f"Dep{index}.lib"], folder)


CONSUMER = """int Check()
{
    if (Dep1.Bridge.Tls<int>(0) != 1 || Dep2.Bridge.Tls<int>(0) != 2) return 7;
    if (Dep1.Bridge.Dep<int>(0) != EXPECTED1 || Dep2.Bridge.Dep<int>(0) != EXPECTED2) return 8;
    return 0;
}
int Main()
{
    return Check();
}
"""


def consumer(folder, expected1, expected2, extra=""):
    (folder / "Consumer.ns").write_text(CONSUMER.replace("EXPECTED1", str(expected1))
                                        .replace("EXPECTED2", str(expected2)), encoding="utf-8")
    (folder / "Consumer.nsproj").write_text(
        '<Project><PropertyGroup><Name>C</Name></PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />'
        '<LibraryReference Include="1/Dep1.lib" /><LibraryReference Include="2/Dep2.lib" />' + extra +
        "</ItemGroup></Project>", encoding="utf-8")


def build(folder, opt, output, cache=None):
    caching = ["--time", "--artifact-cache=" + str(cache)] if cache else ["--no-artifact-cache"]
    return run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-exe", "Consumer.nsproj", "-O" + opt,
                *caching, "-o", output], folder, check=False)


def isolation_layout(out, sources):
    assemblies = sorted(p for p in out.iterdir() if p.is_dir() and p.name.startswith("NativeSharp.Isolation."))
    manifests = sorted(p.name for p in out.glob("*.nsc-isolation.manifest"))
    if len(assemblies) != 2 or len(manifests) != 2 or (out / "dep.dll").exists():
        raise AssertionError(f"disposición inesperada: {sorted(p.name for p in out.iterdir())}")
    found = sorted(digest(a / "dep.dll") for a in assemblies)
    if found != sorted(digest(s) for s in sources):
        raise AssertionError("las dep.dll aisladas no son copias exactas de sus orígenes")
    for assembly in assemblies:
        if not (assembly / f"{assembly.name}.manifest").is_file():
            raise AssertionError(f"falta el manifiesto del ensamblado {assembly.name}")
    return assemblies, manifests


def check_isolated():
    folder = root / "aislado"
    folder.mkdir()
    producer(folder / "1", 1, 101)
    producer(folder / "2", 2, 102)
    sources = [folder / "1" / "dep.dll", folder / "2" / "dep.dll"]
    before = [digest(s) for s in sources]
    consumer(folder, 101, 102)
    for opt in opts:
        out = folder / f"out{opt}"
        result = build(folder, opt, f"out{opt}/C.exe")
        if result.returncode:
            raise AssertionError(f"consumidor O{opt}:\n{result.stdout}")
        isolation_layout(out, sources)
        code = subprocess.run([str(out / "C.exe")], cwd=Path("A:/")).returncode
        if code:
            raise AssertionError(f"ejecución O{opt}: salida {code}")
        print(f"dep.dll homónimas aisladas por alias TLS: correcto O{opt}", flush=True)
    if [digest(s) for s in sources] != before:
        raise AssertionError("NSC modificó una dep.dll de origen")
    cache = folder / "cache"
    for attempt, marker in enumerate(("compilation-bundle-cache: not-found", "compilation-bundle-cache: hit")):
        out = folder / "out-cache"
        if out.exists():
            shutil.rmtree(out)
        result = build(folder, "0", "out-cache/C.exe", cache)
        if result.returncode or marker not in result.stdout:
            raise AssertionError(f"caché intento {attempt}:\n{result.stdout}")
        isolation_layout(out, sources)
        code = subprocess.run([str(out / "C.exe")], cwd=Path("A:/")).returncode
        if code:
            raise AssertionError(f"ejecución restaurada de caché intento {attempt}: salida {code}")
    print("caché: ensamblados y manifiestos restaurados; ejecución correcta", flush=True)
    out = folder / "out0"
    (out / sorted(out.glob("*.nsc-isolation.manifest"))[0].name).unlink()
    code = subprocess.run([str(out / "C.exe")], cwd=Path("A:/")).returncode
    if code == 0:
        raise AssertionError("sin manifiesto de aislamiento el consumidor no debía terminar correctamente")
    print(f"manifiesto de aislamiento retirado: el consumidor falla (código {code:#x})", flush=True)


def check_equal_control():
    folder = root / "control-igual"
    folder.mkdir()
    producer(folder / "1", 1, 100)
    producer(folder / "2", 2, 100, dep_from=folder / "1")
    consumer(folder, 100, 100)
    result = build(folder, "0", "out/C.exe")
    if result.returncode:
        raise AssertionError(f"control con dep.dll iguales:\n{result.stdout}")
    out = folder / "out"
    if not (out / "dep.dll").is_file() or any(out.glob("*.nsc-isolation.manifest")) or \
            any(p.name.startswith("NativeSharp.Isolation.") for p in out.iterdir()):
        raise AssertionError(f"control: disposición inesperada {sorted(p.name for p in out.iterdir())}")
    if subprocess.run([str(out / "C.exe")], cwd=Path("A:/")).returncode:
        raise AssertionError("control con dep.dll iguales: ejecución")
    print("control con dep.dll iguales: una sola dep.dll y sin aislamiento", flush=True)


def check_plain_root_rejected():
    folder = root / "raiz-sin-alias"
    folder.mkdir()
    producer(folder / "1", 1, 101)
    producer(folder / "2", 2, 102)
    other = folder / "3"
    other.mkdir()
    c_dll(other, "dep", "__declspec(dllexport) int dep_value(void) { return 103; }\n")
    (other / "other.h").write_text("__declspec(dllimport) int other_value(void);\n", encoding="utf-8")
    c_dll(other, "other", "__declspec(dllimport) int dep_value(void);\n"
                          "__declspec(dllexport) int other_value(void) { return dep_value(); }\n", ["dep.lib"])
    consumer(folder, 101, 102, '<CppLibraryReference Header="3/other.h" Language="C" Standard="c11" '
                               'Library="3/other.lib" Runtime="3/other.dll" />')
    text = (folder / "Consumer.ns").read_text(encoding="utf-8")
    (folder / "Consumer.ns").write_text(text.replace("    return Check();", "    return Check() + other_value();"),
                                        encoding="utf-8")
    result = build(folder, "0", "out/C.exe")
    if result.returncode == 0 or "runtime package contains different DLLs named 'dep.dll'" not in result.stdout:
        raise AssertionError(f"raíz sin alias con dep.dll distinta: se esperaba el rechazo\n{result.stdout}")
    if (folder / "out" / "C.exe").exists():
        raise AssertionError("raíz sin alias: se publicó el ejecutable pese al rechazo")
    print("dep.dll distinta alcanzada por una DLL sin alias: rechazo conservado", flush=True)


def check_dll_product():
    folder = root / "producto-dll"
    folder.mkdir()
    producer(folder / "1", 1, 101)
    producer(folder / "2", 2, 102)
    (folder / "Product.ns").write_text(
        "namespace Product\n{\n    public class Api\n    {\n"
        "        static int Check()\n        {\n"
        "            if (Dep1.Bridge.Tls<int>(0) != 1 || Dep2.Bridge.Tls<int>(0) != 2) return 7;\n"
        "            if (Dep1.Bridge.Dep<int>(0) != 101 || Dep2.Bridge.Dep<int>(0) != 102) return 8;\n"
        "            return 0;\n        }\n"
        "        public static export extern C int product_check() { return Check(); }\n    }\n}\n",
        encoding="utf-8")
    (folder / "Product.nsproj").write_text(
        '<Project><PropertyGroup><Name>Product</Name><OutputType>SharedLibrary</OutputType></PropertyGroup>'
        '<ItemGroup><Compile Include="Product.ns" /><LibraryReference Include="1/Dep1.lib" />'
        '<LibraryReference Include="2/Dep2.lib" /></ItemGroup></Project>', encoding="utf-8")
    for opt in ("0", "2"):
        out = folder / f"out{opt}"
        result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-dll", "Product.nsproj",
                      "-O" + opt, "--no-artifact-cache", "-o", f"out{opt}/Product.dll"], folder, check=False)
        if result.returncode:
            raise AssertionError(f"producto DLL O{opt}:\n{result.stdout}")
        isolation_layout(out, [folder / "1" / "dep.dll", folder / "2" / "dep.dll"])
        probe = ("import ctypes, sys; d = ctypes.WinDLL(sys.argv[1]); sys.exit(d.product_check())")
        code = subprocess.run([sys.executable, "-c", probe, str(out / "Product.dll")], cwd=Path("A:/")).returncode
        if code:
            raise AssertionError(f"producto DLL O{opt}: product_check devolvió {code}")
        print(f"producto DLL NativeSharp con dep.dll aisladas: correcto O{opt}", flush=True)


def imported_dlls(image):
    """EN: Names in the import table of a PE image. ES: nombres de la tabla de importacion de una imagen PE."""
    text = run([readobj, "--coff-imports", image], image.parent).stdout
    return {line.split(":", 1)[1].strip().lower() for line in text.splitlines() if line.strip().startswith("Name:")}


def exclusive_consumer(folder):
    consumer(folder, 101, 102)
    text = (folder / "Consumer.ns").read_text(encoding="utf-8")
    call = "    return Check() + Dep1.Bridge.Only<int>(0) - EXCLUSIVE;"
    (folder / "Consumer.ns").write_text(text.replace("    return Check();", call), encoding="utf-8")


def check_exclusive_alias():
    folder = root / "alias-exclusivo"
    folder.mkdir()
    producer(folder / "1", 1, 101, unique=True)
    producer(folder / "2", 2, 102)
    exclusive_consumer(folder)
    text = (folder / "Consumer.ns").read_text(encoding="utf-8")
    (folder / "Consumer.ns").write_text(text.replace("EXCLUSIVE", "101"), encoding="utf-8")
    sources = [folder / "1" / "dep.dll", folder / "2" / "dep.dll"]
    for opt in opts:
        out = folder / f"out{opt}"
        result = build(folder, opt, f"out{opt}/C.exe")
        if result.returncode:
            raise AssertionError(f"alias exclusivo O{opt}:\n{result.stdout}")
        isolation_layout(out, sources)
        aliases = {name for name in imported_dlls(out / "C.exe") if name.startswith("__nsc_tls_")}
        if aliases:
            raise AssertionError(f"alias exclusivo O{opt}: la tabla de importacion nombra {sorted(aliases)}")
        code = subprocess.run([str(out / "C.exe")], cwd=Path("A:/")).returncode
        if code:
            raise AssertionError(f"alias exclusivo O{opt}: salida {code}")
        print(f"funcion exclusiva de un alias aislado, enlazada en ejecucion: correcto O{opt}", flush=True)
    out = folder / f"out{opts[0]}"
    (out / sorted(out.glob("*.nsc-isolation.manifest"))[0].name).unlink()
    code = subprocess.run([str(out / "C.exe")], cwd=Path("A:/")).returncode
    if code == 0:
        raise AssertionError("alias exclusivo sin manifiesto: el consumidor no debia terminar correctamente")
    print(f"alias exclusivo sin manifiesto de aislamiento: el consumidor falla (codigo {code:#x})", flush=True)
    product = folder / "producto"
    product.mkdir()
    for index in ("1", "2"):
        shutil.copytree(folder / index, product / index)
    (product / "Product.ns").write_text(
        "namespace Product\n{\n    public class Api\n    {\n"
        "        public static export extern C int product_check()\n        {\n"
        "            if (Dep1.Bridge.Dep<int>(0) != 101 || Dep2.Bridge.Dep<int>(0) != 102) return 8;\n"
        "            if (Dep1.Bridge.Tls<int>(0) != 1 || Dep2.Bridge.Tls<int>(0) != 2) return 7;\n"
        "            return Dep1.Bridge.Only<int>(0) == 101 ? 0 : 9;\n        }\n    }\n}\n", encoding="utf-8")
    (product / "Product.nsproj").write_text(
        '<Project><PropertyGroup><Name>Product</Name><OutputType>SharedLibrary</OutputType></PropertyGroup>'
        '<ItemGroup><Compile Include="Product.ns" /><LibraryReference Include="1/Dep1.lib" />'
        '<LibraryReference Include="2/Dep2.lib" /></ItemGroup></Project>', encoding="utf-8")
    for opt in ("0", "2"):
        out = product / f"out{opt}"
        result = run([nsc, r"--cpp-clang-root=A:\al82dev-clang", "--emit-project-oop-dll", "Product.nsproj",
                      "-O" + opt, "--no-artifact-cache", "-o", f"out{opt}/Product.dll"], product, check=False)
        if result.returncode:
            raise AssertionError(f"producto DLL con alias exclusivo O{opt}:\n{result.stdout}")
        isolation_layout(out, [product / "1" / "dep.dll", product / "2" / "dep.dll"])
        if any(name.startswith("__nsc_tls_") for name in imported_dlls(out / "Product.dll")):
            raise AssertionError(f"producto DLL O{opt}: la tabla de importacion nombra un alias")
        probe = "import ctypes, sys; d = ctypes.WinDLL(sys.argv[1]); sys.exit(d.product_check())"
        code = subprocess.run([sys.executable, "-c", probe, str(out / "Product.dll")], cwd=Path("A:/")).returncode
        if code:
            raise AssertionError(f"producto DLL con alias exclusivo O{opt}: product_check devolvio {code}")
        print(f"producto DLL con funcion exclusiva de un alias aislado: correcto O{opt}", flush=True)


def check_exclusive_without_isolation():
    folder = root / "alias-exclusivo-sin-aislamiento"
    folder.mkdir()
    producer(folder / "1", 1, 100, unique=True)
    producer(folder / "2", 2, 100, dep_from=folder / "1")
    consumer(folder, 100, 100)
    text = (folder / "Consumer.ns").read_text(encoding="utf-8")
    call = "    return Check() + Dep1.Bridge.Only<int>(0) - 100;"
    (folder / "Consumer.ns").write_text(text.replace("    return Check();", call), encoding="utf-8")
    result = build(folder, "0", "out/C.exe")
    if result.returncode:
        raise AssertionError(f"alias exclusivo sin aislamiento:\n{result.stdout}")
    out = folder / "out"
    aliases = {name for name in imported_dlls(out / "C.exe") if name.startswith("__nsc_tls_")}
    if len(aliases) != 1 or any(out.glob("*.nsc-isolation.manifest")):
        raise AssertionError(f"sin aislamiento la funcion exclusiva debia conservar su importacion directa: {aliases}")
    if subprocess.run([str(out / "C.exe")], cwd=Path("A:/")).returncode:
        raise AssertionError("alias exclusivo sin aislamiento: ejecucion")
    print("funcion exclusiva sin dependencias que aislar: conserva la importacion directa", flush=True)


check_isolated()
check_dll_product()
check_exclusive_alias()
check_exclusive_without_isolation()
check_equal_control()
check_plain_root_rejected()
print("DLL TLS homónimas con dependencias transitivas homónimas verificadas;", root)
