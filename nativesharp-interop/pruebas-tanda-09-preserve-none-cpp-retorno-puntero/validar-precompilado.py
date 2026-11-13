"""T09: llamada C++ preserve_none desde plantilla precompilada, con y sin encabezado."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--nsc", required=True, help="Compilador de desarrollo seleccionado")
parser.add_argument("--clang-root", required=True, help="Directorio de recursos de Clang 23.1.0")
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
source_c = fixture
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix="nsc-t09-preserve-cpp-retorno-puntero-precompilado-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)

def run(label, argv):
    result = subprocess.run(argv, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    log = root / (label + ".log")
    log.write_text("COMMAND: " + repr(argv) + "\nEXIT: " + str(result.returncode)
                   + "\n" + result.stdout, encoding="utf-8")
    if result.returncode != 0:
        print(result.stdout)
        raise SystemExit(label + ": fallo; evidencia: " + str(log))
    return result.stdout

version = run("clang-version", ["clang.exe", "--version"])
if "clang version 23.1.0" not in version:
    raise SystemExit("Se requiere Clang 23.1.0; evidencia: " + str(root))

for mode in ("estatica", "dll"):
    for opt in range(4):
        case = root / (mode + "-O" + str(opt))
        producer = case / "producer"
        producer.mkdir(parents=True)
        for name in ("api.h", "api-dll.h", "api.cpp"):
            shutil.copyfile(source_c / name, producer / name)
        shutil.copyfile(fixture / "Producer.ns", producer / "Producer.ns")
        prefix = mode + "-O" + str(opt)
        run(prefix + "-c", ["clang.exe", "-target", "x86_64-pc-windows-msvc",
            "-std=c++20", "-O" + str(opt), "-c", str(producer / "api.cpp"),
            "-o", str(producer / "api.obj")])
        if mode == "estatica":
            run(prefix + "-archivo", ["llvm-lib.exe", "/nologo",
                "/out:" + str(producer / "api.lib"), str(producer / "api.obj")])
            header, library, runtime = "api.h", "api.lib", ""
        else:
            run(prefix + "-dll", ["lld-link.exe", "/nologo", "/dll", "/noentry",
                "/nodefaultlib", "/export:?scalar@T09Cpp@@YVPEAHMPEAH@Z", "/export:?fetchCallback@T09Cpp@@YAP6VPEAHMPEAH@ZXZ", "/out:" + str(producer / "api.dll"),
                "/implib:" + str(producer / "api-dll.lib"), str(producer / "api.obj")])
            header, library, runtime = "api-dll.h", "api-dll.lib", ' Runtime="api.dll"'
        cpp_ref = ('<CppLibraryReference Header="' + header + '" Language="C++" Standard="c++20" '
                   'Library="' + library + '"' + runtime + ' />')
        producer_xml = ('<Project><PropertyGroup><Name>T09Producer</Name>'
                        '<OutputType>Library</OutputType></PropertyGroup><ItemGroup>'
                        '<Compile Include="Producer.ns" />' + cpp_ref + '</ItemGroup></Project>')
        (producer / "Producer.nsproj").write_text(producer_xml, encoding="utf-8")
        run(prefix + "-producer-ns", [args.nsc, "--emit-project-oop-lib",
            str(producer / "Producer.nsproj"), "--cpp-clang-root=" + args.clang_root,
            "--no-artifact-cache", "-O" + str(opt), "-o", str(producer / "Producer.lib")])
        for visibility in ("con-encabezado", "sin-encabezado"):
            consumer = case / visibility
            consumer.mkdir()
            shutil.copyfile(fixture / "Consumer.ns", consumer / "Consumer.ns")
            refs = '<LibraryReference Include="../producer/Producer.lib" />'
            if visibility == "con-encabezado":
                refs += ('<CppLibraryReference Header="../producer/' + header +
                         '" Language="C++" Standard="c++20" Library="../producer/' + library +
                         '"' + (' Runtime="../producer/api.dll"' if runtime else '') + ' />')
            else:
                for name in ("api.h", "api-dll.h"):
                    path = producer / name
                    if path.exists():
                        path.rename(producer / (name + ".oculto"))
            consumer_xml = ('<Project><PropertyGroup><Name>T09Consumer</Name></PropertyGroup><ItemGroup>'
                            '<Compile Include="Consumer.ns" />' + refs + '</ItemGroup></Project>')
            (consumer / "Consumer.nsproj").write_text(consumer_xml, encoding="utf-8")
            output = consumer / "out" / "Consumer.exe"
            output.parent.mkdir()
            label = prefix + "-" + visibility
            if opt == 0:
                hir = run(label + "-hir", [args.nsc, "--dump-project-oop-hir",
                    str(consumer / "Consumer.nsproj"), "--cpp-clang-root=" + args.clang_root])
                if "cpp-pointer-plan=__nsc_pointer_abi_probe" not in hir:
                    raise SystemExit(label + ": no se restauro el plan ABI; evidencia: " + str(root))
            run(label + "-build", [args.nsc, "--emit-project-oop-exe",
                str(consumer / "Consumer.nsproj"), "--cpp-clang-root=" + args.clang_root,
                "--no-artifact-cache", "-O" + str(opt), "-o", str(output)])
            run(label + "-run", [str(output)])
            print(label + ": codigo 0", flush=True)
print("OK: 16 consumidores precompilados; evidencia:", root)
