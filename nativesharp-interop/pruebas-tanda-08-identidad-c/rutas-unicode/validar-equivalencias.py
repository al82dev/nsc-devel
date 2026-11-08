"""T08-002-09m7c0g2b: Unicode spellings that Windows may or may not treat as the same file.

For every pair, os.path.samefile on the real file system is the oracle. NSC must follow it for
sources, project references and C headers: one file is loaded once, two files are both loaded.
Output executables and artifact caches whose paths use those spellings must build, run and
restore from the cache. Every build runs in O0; the content of the paths does not reach codegen.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import unicodedata

root = Path(tempfile.mkdtemp(prefix="t08-rutas-equivalencias-", dir=r"A:\nsc-interop-build"))
compiler = Path(r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe")
env = os.environ.copy()
env["PATH"] = str(compiler.parent) + r";A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"

NFC_E = unicodedata.normalize("NFC", "\u00e9")
NFD_E = unicodedata.normalize("NFD", "\u00e9")
PAIRS = [
    ("nfc-nfd", NFC_E, NFD_E),
    ("i-punto", "\u0130", "i"),
    ("i-sin-punto", "\u0131", "I"),
    ("eszett", "\u00df", "\u1e9e"),
    ("titulo-dz", "\u01c5", "\u01c6"),
    ("sigma-final", "\u03a3", "\u03c2"),
    ("kelvin", "\u212a", "k"),
    # Control: Windows folds this case pair, so the "same file" branch is exercised too.
    ("mayuscula-e", NFC_E, "\u00c9"),
]


def run(arguments, folder):
    return subprocess.run([str(compiler), r"--cpp-clang-root=A:\al82dev-clang", *arguments], cwd=folder,
                          env=env, text=True, encoding="utf-8", errors="replace",
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def same_file(folder, first, second):
    probe = folder / ("probe-" + first)
    probe.write_text("x", encoding="utf-8")
    other = folder / ("probe-" + second)
    return other.exists() and os.path.samefile(probe, other)


def project(path, name, items):
    path.write_text(f"<Project><PropertyGroup><Name>{name}</Name></PropertyGroup><ItemGroup>{items}"
                    "</ItemGroup></Project>", encoding="utf-8")


def check_sources(folder, first, second, same):
    (folder / f"src-{first}.ns").write_text("int First() { return 3; }\n", encoding="utf-8")
    if same:
        main = "int Main() { return First() == 3 ? 0 : 1; }\n"
    else:
        (folder / f"src-{second}.ns").write_text("int Second() { return 4; }\n", encoding="utf-8")
        main = "int Main() { return First() + Second() == 7 ? 0 : 1; }\n"
    (folder / "Main.ns").write_text(main, encoding="utf-8")
    project(folder / "Sources.nsproj", "UnicodeSources",
            f'<Compile Include="Main.ns" /><Compile Include="src-{first}.ns" /><Compile Include="src-{second}.ns" />')
    result = run(["--check-project", "Sources.nsproj"], folder)
    if result.returncode:
        raise AssertionError(f"fuentes {first}/{second} (mismo archivo={same}):\n" + result.stdout)


def check_projects(folder, first, second, same):
    (folder / "ChildA.ns").write_text("int ChildA() { return 5; }\n", encoding="utf-8")
    (folder / "ChildB.ns").write_text("int ChildB() { return 6; }\n", encoding="utf-8")
    project(folder / f"child-{first}.nsproj", "ChildModuleA", '<Compile Include="ChildA.ns" />')
    if not same:
        project(folder / f"child-{second}.nsproj", "ChildModuleB", '<Compile Include="ChildB.ns" />')
    (folder / "UseChildren.ns").write_text("int Main() { return 0; }\n", encoding="utf-8")
    project(folder / "Projects.nsproj", "UnicodeProjects",
            f'<Compile Include="UseChildren.ns" /><ProjectReference Include="child-{first}.nsproj" />'
            f'<ProjectReference Include="child-{second}.nsproj" />')
    result = run(["--check-project", "Projects.nsproj"], folder)
    if same and (result.returncode == 0 or "duplicate ProjectReference" not in result.stdout):
        raise AssertionError(f"proyectos {first}/{second}: no se detectó el duplicado:\n" + result.stdout)
    if not same and result.returncode:
        raise AssertionError(f"proyectos {first}/{second}: dos archivos distintos rechazados:\n" + result.stdout)


def check_headers_output_cache(folder, label, first, second, same):
    (folder / f"head-{first}.h").write_text("static inline int first_value(void) { return 8; }\n", encoding="utf-8")
    items = '<Compile Include="UseC.ns" />'
    items += f'<CppLibraryReference Header="head-{first}.h" Language="C" Standard="c11" />'
    expected = "first_value() == 8"
    if not same:
        (folder / f"head-{second}.h").write_text("static inline int second_value(void) { return 9; }\n",
                                                 encoding="utf-8")
        items += f'<CppLibraryReference Header="head-{second}.h" Language="C" Standard="c11" />'
        expected = "first_value() + second_value() == 17"
    (folder / "UseC.ns").write_text(f"int Main() {{ return {expected} ? 0 : 1; }}\n", encoding="utf-8")
    project(folder / "Headers.nsproj", "UnicodeHeaders", items)
    output = folder / f"salida {label}-{second}" / f"Prog-{first}.exe"
    cache = folder / f"cache-{label}-{first}"
    command = ["--emit-project-oop-exe", "Headers.nsproj", "-O0", "--time", "--artifact-cache=" + str(cache),
               "-o", str(output)]
    for attempt, marker in enumerate(("compilation-bundle-cache: not-found", "compilation-bundle-cache: hit")):
        if attempt:
            output.unlink()
        result = run(command, folder)
        if result.returncode or marker not in result.stdout:
            raise AssertionError(f"encabezados/salida/caché {label} intento {attempt}:\n" + result.stdout)
        if subprocess.run([str(output)], cwd=Path("A:/")).returncode:
            raise AssertionError(f"ejecución {label} intento {attempt}")


for label, first, second in PAIRS:
    folder = root / label
    folder.mkdir()
    same = same_file(folder, first, second)
    check_sources(folder, first, second, same)
    check_projects(folder, first, second, same)
    check_headers_output_cache(folder, label, first, second, same)
    print(f"{label}: {'mismo archivo' if same else 'archivos distintos'} para Windows; NSC coincide", flush=True)
print("Equivalencias Unicode de rutas verificadas;", root)
