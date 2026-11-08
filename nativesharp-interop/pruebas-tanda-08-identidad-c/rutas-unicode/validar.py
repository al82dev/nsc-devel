from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(tempfile.mkdtemp(prefix="t08-rutas-unicode-", dir=r"A:\nsc-interop-build"))
reference = Path(r"A:\nsc-interop-build\t08-tls-dos-dll-repro")
compiler = Path(r"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe")
env = os.environ.copy()
env["PATH"] = str(compiler.parent) + r";A:\al82dev-clang\bin;" + env["PATH"]
env["NSC_LINKER"] = r"A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
env["NSC_LLVM_LIB"] = r"A:\al82dev-clang\bin\llvm-lib.exe"
env["NSC_WINDOWS_RUNTIME_LIBRARY"] = r"A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
env["TMP"] = env["TEMP"] = r"A:\nsc-interop-build\temp"
shutil.copyfile(reference / "TlsProducer.nsmeta", root / "TlsProducer.nsmeta")
(root / "Main.ns").write_text("int Main() { return 0; }\n", encoding="utf-8")

def check(project):
    result = subprocess.run([str(compiler), r"--cpp-clang-root=A:\al82dev-clang", "--check-project", str(project)], cwd=root, env=env,
                            text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    return result

for upper, lower in [("\u0394", "\u03b4"), ("\u0416", "\u0436"), ("\u0100", "\u0101")]:
    library = root / ("lib-" + upper + ".lib")
    lower_library = root / ("lib-" + lower + ".lib")
    shutil.copyfile(reference / "TlsProducer.lib", library)
    if not os.path.samefile(library, lower_library):
        raise AssertionError("Windows no confirma equivalencia: " + str(library))
    project = root / "Libraries.nsproj"
    project.write_text('<Project><PropertyGroup><Name>UnicodeLibraries</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" />'
        '<LibraryReference Include="lib-' + upper + '.lib" Metadata="TlsProducer.nsmeta" />'
        '<LibraryReference Include="lib-' + lower + '.lib" Metadata="TlsProducer.nsmeta" />'
        '</ItemGroup></Project>', encoding="utf-8")
    result = check(project)
    if result.returncode == 0 or "duplicate LibraryReference" not in result.stdout:
        raise AssertionError("No se detectó biblioteca duplicada " + upper + "/" + lower + ":\n" + result.stdout)

    source = root / ("source-" + upper + ".ns")
    source.write_text("int Main() { return 0; }\n", encoding="utf-8")
    if not os.path.samefile(source, root / ("source-" + lower + ".ns")):
        raise AssertionError("Windows no confirma equivalencia de fuente")
    project = root / "Sources.nsproj"
    project.write_text('<Project><PropertyGroup><Name>UnicodeSources</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="source-' + upper + '.ns" />'
        '<Compile Include="source-' + lower + '.ns" />'
        '</ItemGroup></Project>', encoding="utf-8")
    result = check(project)
    if result.returncode != 0:
        raise AssertionError("Fuente equivalente no se compiló una sola vez " + upper + "/" + lower + ":\n" + result.stdout)

    child = root / ("child-" + upper + ".nsproj")
    child.write_text('<Project><PropertyGroup><Name>ChildModule</Name></PropertyGroup><ItemGroup>'
                     '<Compile Include="Child.ns" /></ItemGroup></Project>', encoding="utf-8")
    (root / "Child.ns").write_text("int Child() { return 5; }\n", encoding="utf-8")
    if not os.path.samefile(child, root / ("child-" + lower + ".nsproj")):
        raise AssertionError("Windows no confirma equivalencia de proyecto")
    project = root / "Projects.nsproj"
    project.write_text('<Project><PropertyGroup><Name>UnicodeProjects</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" />'
        '<ProjectReference Include="child-' + upper + '.nsproj" />'
        '<ProjectReference Include="child-' + lower + '.nsproj" />'
        '</ItemGroup></Project>', encoding="utf-8")
    result = check(project)
    if result.returncode == 0 or "duplicate ProjectReference" not in result.stdout:
        raise AssertionError("No se detectó proyecto duplicado " + upper + "/" + lower + ":\n" + result.stdout)
    project.write_text('<Project><PropertyGroup><Name>UnicodeProjects</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="Main.ns" />'
        '<ProjectReference Include="child-' + lower + '.nsproj" />'
        '</ItemGroup></Project>', encoding="utf-8")
    result = check(project)
    if result.returncode != 0:
        raise AssertionError("No se cargó el proyecto Unicode " + upper + "/" + lower + ":\n" + result.stdout)

    include_dir = root / ("include-" + upper)
    include_dir.mkdir()
    (include_dir / "dep.h").write_text("#define ANSWER 5\n", encoding="utf-8")
    (root / ("header-" + upper + ".h")).write_text(
        '#include <dep.h>\nstatic inline int answer(void) { return ANSWER; }\n', encoding="utf-8")
    (root / "UseC.ns").write_text("int Main() { return answer(); }\n", encoding="utf-8")
    project = root / "Headers.nsproj"
    project.write_text('<Project><PropertyGroup><Name>UnicodeHeaders</Name></PropertyGroup><ItemGroup>'
        '<Compile Include="UseC.ns" />'
        '<CppLibraryReference Header="header-' + lower + '.h" Language="C" Standard="c11">'
        '<IncludeDirectory Path="include-' + lower + '" />'
        '</CppLibraryReference></ItemGroup></Project>', encoding="utf-8")
    result = check(project)
    if result.returncode != 0:
        raise AssertionError("Encabezado o directorio Unicode no accesible " + upper + "/" + lower + ":\n" + result.stdout)
    exe = root / "Headers.exe"
    built = subprocess.run([str(compiler), r"--cpp-clang-root=A:\al82dev-clang",
                            "--emit-project-oop-exe", str(project), "-O0", "--no-artifact-cache",
                            "-o", str(exe)], cwd=root, env=env, text=True, encoding="utf-8",
                           errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if built.returncode != 0 or subprocess.run([str(exe)], cwd=root, env=env).returncode != 5:
        raise AssertionError("No se ejecutó la función del encabezado Unicode:\n" + built.stdout)

print("Bibliotecas, fuentes, proyectos y encabezados Unicode: correctos;", root)
