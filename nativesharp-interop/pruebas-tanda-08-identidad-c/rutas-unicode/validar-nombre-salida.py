"""T08-002-09m7c0g2b: product names and directories the active code page cannot represent.

1. A static library emitted under a precomposed accent, a decomposed accent (combining U+0301), a CJK
   character and the Kelvin sign is consumed by an executable with an ASCII name (O0). Before the fix the
   decomposed names stopped nsc with "No mapping for the Unicode character exists in the target
   multi-byte code page" (bundle identity, then the library stem in ProjectGraphLoader).
2. An executable emitted under those names builds and runs in O0-O3, and with -g its PDB is published
   under the requested name and the RSDS record of the PE names that same file in UTF-8. Before the fix
   NSLink received the response file through the ANSI code page and stopped with 0xC0000409, or wrote the
   product under a mangled name.
3. A temporary directory and an artifact cache whose names have a CJK character (a user profile the code
   page cannot spell): the build succeeds, the second build restores from the cache, both run.
4. DLLs (O0 and O2, -g; the export directory and the RSDS record must carry the requested names and an
   importing executable must run). With NSLink: an ASCII DLL linked from a CJK temporary directory works,
   and a non-ASCII DLL name is rejected with a diagnostic (negative): NSLink cannot spell it and a DLL keeps
   the name it was linked under. With lld-link, which reads UTF-8, the non-ASCII names link with the right
   exported name and LoadLibraryW loads them (an ASCII lld-link DLL is also imported by an executable).
"""
from pathlib import Path
import ctypes
import os
import subprocess
import tempfile
import unicodedata

root = Path(tempfile.mkdtemp(prefix="t08-nombre-salida-", dir=r"A:\nsc-interop-build"))
compiler = Path(r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe")
env = os.environ.copy()
env["PATH"] = str(compiler.parent) + r";A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"

NAMES = [
    ("nfc", unicodedata.normalize("NFC", "Lib\u00e9")),
    ("nfd", unicodedata.normalize("NFD", "Lib\u00e9")),
    ("combinante", "Liba\u0301"),
    ("cjk", "Lib\u6f22"),
    ("kelvin", "Lib\u212a"),
]


def run(arguments, folder, environment=None):
    return subprocess.run([str(compiler), *arguments], cwd=folder, env=environment or env, text=True,
                          encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def rsds_name(executable):
    data = executable.read_bytes()
    at = data.find(b"RSDS")
    if at < 0:
        raise AssertionError(f"{executable.name}: sin registro RSDS")
    return data[at + 24:data.index(b"\0", at + 24)].decode("utf-8")


def program(folder):
    (folder / "Main.ns").write_text("int Main() { return 0; }\n", encoding="utf-8")
    (folder / "P.nsproj").write_text('<Project><PropertyGroup><Name>P</Name></PropertyGroup><ItemGroup>'
                                     '<Compile Include="Main.ns" /></ItemGroup></Project>', encoding="utf-8")


def check_libraries():
    for label, name in NAMES:
        folder = root / ("lib-" + label)
        folder.mkdir()
        (folder / "Producer.ns").write_text("namespace NamedOutput { public class Value { public static int Get() "
                                            "{ return 12; } } }\n", encoding="utf-8")
        (folder / "Producer.nsproj").write_text(
            "<Project><PropertyGroup><Name>NamedOutput</Name><OutputType>Library</OutputType></PropertyGroup>"
            '<ItemGroup><Compile Include="Producer.ns" /></ItemGroup></Project>', encoding="utf-8")
        result = run(["--emit-project-oop-lib", "Producer.nsproj", "-O0", "--no-artifact-cache", "-o",
                      name + ".lib"], folder)
        if result.returncode or not (folder / (name + ".lib")).is_file():
            raise AssertionError(f"biblioteca {label}:\n" + result.stdout)
        (folder / "Consumer.ns").write_text("int Main() { return NamedOutput.Value.Get() == 12 ? 0 : 1; }\n",
                                            encoding="utf-8")
        (folder / "Consumer.nsproj").write_text(
            '<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup>'
            f'<Compile Include="Consumer.ns" /><LibraryReference Include="{name}.lib" /></ItemGroup></Project>',
            encoding="utf-8")
        result = run(["--emit-project-oop-exe", "Consumer.nsproj", "-O0", "--no-artifact-cache", "-o",
                      "out/Consumer.exe"], folder)
        if result.returncode or subprocess.run([str(folder / "out" / "Consumer.exe")], cwd=folder).returncode:
            raise AssertionError(f"consumidor {label}:\n" + result.stdout)
        print("biblioteca", label, "emitida y consumida", flush=True)


def check_executables():
    folder = root / "exe"
    folder.mkdir()
    program(folder)
    for label, name in NAMES:
        for level in ("-O0", "-O1", "-O2", "-O3"):
            output = folder / f"{label}{level}" / (name.replace("Lib", "Prog") + ".exe")
            result = run(["--emit-project-oop-exe", "P.nsproj", level, "--no-artifact-cache", "-g", "-o",
                          str(output)], folder)
            if result.returncode or subprocess.run([str(output)], cwd=folder).returncode:
                raise AssertionError(f"ejecutable {label} {level}:\n" + result.stdout)
            pdb = output.with_suffix(".pdb")
            if not pdb.is_file() or rsds_name(output) != pdb.name:
                raise AssertionError(f"PDB {label} {level}: {pdb.is_file()} {rsds_name(output)!r}")
            if sorted(path.name for path in output.parent.iterdir()) != sorted([output.name, pdb.name]):
                raise AssertionError(f"restos en {output.parent}: {list(output.parent.iterdir())}")
        print("ejecutable", label, "O0-O3 con PDB", flush=True)


def check_temporary_and_cache():
    folder = root / "temporal"
    folder.mkdir()
    program(folder)
    temporary = folder / "temp-\u6f22"
    temporary.mkdir()
    environment = dict(env, TMP=str(temporary), TEMP=str(temporary))
    for level in ("-O0", "-O2"):
        cache = folder / f"cache-\u6f22{level}"
        output = folder / f"out{level}" / "Prog.exe"
        command = ["--emit-project-oop-exe", "P.nsproj", level, "-g", "--time", "--artifact-cache=" + str(cache),
                   "-o", str(output)]
        for attempt, marker in enumerate(("compilation-bundle-cache: not-found", "compilation-bundle-cache: hit")):
            if attempt:
                output.unlink()
            result = run(command, folder, environment)
            if result.returncode or marker not in result.stdout:
                raise AssertionError(f"temporal/caché CJK {level} intento {attempt}:\n" + result.stdout)
            if subprocess.run([str(output)], cwd=folder).returncode:
                raise AssertionError(f"ejecución temporal/caché CJK {level} intento {attempt}")
    print("directorio temporal y caché CJK: construcción y restauración", flush=True)


def export_name(dll):
    data = dll.read_bytes()
    header = int.from_bytes(data[0x3C:0x40], "little")
    sections = int.from_bytes(data[header + 6:header + 8], "little")
    optional = header + 24
    optional_size = int.from_bytes(data[header + 20:header + 22], "little")
    export_rva = int.from_bytes(data[optional + 112:optional + 116], "little")

    def offset(rva):
        for index in range(sections):
            entry = optional + optional_size + 40 * index
            virtual = int.from_bytes(data[entry + 12:entry + 16], "little")
            size = int.from_bytes(data[entry + 16:entry + 20], "little")
            raw = int.from_bytes(data[entry + 20:entry + 24], "little")
            if virtual <= rva < virtual + size:
                return rva - virtual + raw
        raise AssertionError(f"{dll.name}: RVA {rva:#x} fuera de secciones")

    name = offset(int.from_bytes(data[offset(export_rva) + 12:offset(export_rva) + 16], "little"))
    return data[name:data.index(b"\0", name)].decode("utf-8")


def build_dll_and_consumer(folder, label, name, level, environment, consume=True):
    dll = folder / f"{label}{level}" / (name + ".dll")
    result = run(["--emit-project-oop-dll", "L.nsproj", level, "--no-artifact-cache", "-g", "-o", str(dll)], folder,
                 environment)
    if result.returncode or not dll.is_file():
        raise AssertionError(f"DLL {label} {level}:\n" + result.stdout)
    if export_name(dll) != dll.name or rsds_name(dll) != dll.with_suffix(".pdb").name:
        raise AssertionError(f"DLL {label} {level}: {export_name(dll)!r} {rsds_name(dll)!r}")
    if not dll.with_suffix(".lib").is_file() or not dll.with_suffix(".pdb").is_file():
        raise AssertionError(f"DLL {label} {level}: falta la biblioteca de importación o el PDB")
    if not consume:
        ctypes.WinDLL(str(dll))
        return
    relative = f"{label}{level}/{name}"
    (folder / "C.nsproj").write_text(
        '<Project><PropertyGroup><Name>C</Name></PropertyGroup><ItemGroup><Compile Include="C.ns" />'
        f'<LibraryReference Include="{relative}.lib" Runtime="{relative}.dll" /></ItemGroup></Project>',
        encoding="utf-8")
    consumer = folder / f"consumidor-{label}{level}" / "C.exe"
    result = run(["--emit-project-oop-exe", "C.nsproj", level, "--no-artifact-cache", "-o", str(consumer)], folder,
                 environment)
    if result.returncode or subprocess.run([str(consumer)], cwd=Path("A:/")).returncode:
        raise AssertionError(f"consumidor de DLL {label} {level}:\n" + result.stdout)


def check_dynamic_libraries():
    folder = root / "dll"
    folder.mkdir()
    (folder / "L.ns").write_text("namespace Dl { public class V { public static int Get() { return 3; } } }\n",
                                 encoding="utf-8")
    (folder / "L.nsproj").write_text('<Project><PropertyGroup><Name>Dl</Name><OutputType>SharedLibrary'
                                     '</OutputType></PropertyGroup><ItemGroup><Compile Include="L.ns" />'
                                     '</ItemGroup></Project>', encoding="utf-8")
    (folder / "C.ns").write_text("int Main() { return Dl.V.Get() == 3 ? 0 : 1; }\n", encoding="utf-8")
    # NSLink: an ASCII DLL linked from a CJK temporary directory works; a non-ASCII DLL name is rejected
    # with a diagnostic, because NSLink cannot spell it and a DLL keeps the name it was linked under.
    temporary = folder / "temp-\u6f22"
    temporary.mkdir()
    for level in ("-O0", "-O2"):
        build_dll_and_consumer(folder, "nslink-ascii", "Dl", level, dict(env, TMP=str(temporary), TEMP=str(temporary)))
    for label, name in NAMES:
        result = run(["--emit-project-oop-dll", "L.nsproj", "-O0", "--no-artifact-cache", "-o",
                      f"nslink-{label}/{name}.dll"], folder)
        if result.returncode == 0 or "NSLink cannot link a DLL whose file name has characters outside ASCII" \
                not in result.stdout or "internal error" in result.stdout:
            raise AssertionError(f"DLL {label} con NSLink no rechazada con el diagnóstico:\n" + result.stdout)
        if (folder / f"nslink-{label}" / (name + ".dll")).exists():
            raise AssertionError(f"DLL {label} con NSLink: quedó un producto")
    # lld-link reads UTF-8: the same names link and keep their exported name, and LoadLibraryW loads them.
    # An importing executable is not checked: the Windows loader does not resolve a non-ASCII import name
    # and the program stops with STATUS_DLL_NOT_FOUND (pending decision T08-002-09m7c0g2d).
    lld = dict(env, NSC_LINKER=r"A:\al82dev-clang\bin\lld-link.exe")
    build_dll_and_consumer(folder, "lld-ascii", "Dl", "-O0", lld)
    for label, name in NAMES:
        for level in ("-O0", "-O2"):
            build_dll_and_consumer(folder, "lld-" + label, name, level, lld, consume=False)
    print("DLL: ASCII con NSLink y temporal CJK, rechazo no ASCII con NSLink, nombres Unicode con lld-link",
          flush=True)


check_libraries()
check_executables()
check_temporary_and_cache()
check_dynamic_libraries()
print("Nombres de producto y directorios Unicode verificados;", root)
