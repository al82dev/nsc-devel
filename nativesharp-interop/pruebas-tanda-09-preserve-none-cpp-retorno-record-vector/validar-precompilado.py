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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-preserve-cpp-retorno-record-vector-precompilado-", dir=r"A:\temp"))
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
                "/nodefaultlib", "/export:t09_init", "/export:t09_check", "/export:?scalar@T09Cpp@@YV?AUSmall@1@MU21@@Z", "/export:?fetchCallback@T09Cpp@@YAP6V?AUSmall@1@MU21@@ZXZ", "/out:" + str(producer / "api.dll"),
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
            if mode == "estatica" and opt == 0 and visibility == "sin-encabezado":
                negative = case / "campo-no-proyectado"
                negative.mkdir()
                padded = negative / "producer"
                padded.mkdir()
                header_text = (fixture / "api.h").read_text(encoding="utf-8")
                lane = "float" if "using Float4 = float " in header_text else "double"
                declaration = "using Float4 = " + lane + " "
                if declaration not in header_text:
                    raise SystemExit("falta tipo vectorial para el control negativo")
                negative_header = header_text.replace(declaration, "using Float4 = _Float16 ")
                negative_header = negative_header.replace("Float4 values[2];", "Float4 values;")
                for assertion in ("static_assert(alignof(Float4) == 32);",
                                  "static_assert(alignof(T09Cpp::Small) == 32);",
                                  "static_assert(alignof(T09Cpp::Small) == 64);",
                                  "static_assert(sizeof(Float4)==16 && alignof(Float4)==16);",
                                  "static_assert(sizeof(Float4)==32 && alignof(Float4)==32);",
                                  "static_assert(sizeof(Float4)==16 && sizeof(T09Cpp::Small)==48);"):
                    negative_header = negative_header.replace(assertion, "")
                (padded / "api.h").write_text(negative_header, encoding="utf-8")
                (padded / "api.cpp").write_text('#include "api.h"\nextern "C" void t09_init(T09Cpp::Small*) {}\n', encoding="utf-8")
                (padded / "Producer.ns").write_text(
                    "namespace T09 { public class Keep { public static int Run<T>(T x) { T09Cpp.Small v; t09_init(&v); return 0; } } }\n", encoding="utf-8")
                (padded / "Producer.nsproj").write_text(
                    '<Project><PropertyGroup><Name>T09Producer</Name><OutputType>Library</OutputType></PropertyGroup>'
                    '<ItemGroup><Compile Include="Producer.ns" /><CppLibraryReference Header="api.h" Language="C++" '
                    'Standard="c++20" Library="api.lib" /></ItemGroup></Project>', encoding="utf-8")
                run("campo-no-proyectado-clang", ["clang.exe", "-target", "x86_64-pc-windows-msvc", "-std=c++20",
                    "-c", str(padded / "api.cpp"), "-o", str(padded / "api.obj")])
                run("campo-no-proyectado-lib", ["llvm-lib.exe", "/nologo", "/out:" + str(padded / "api.lib"), str(padded / "api.obj")])
                run("campo-no-proyectado-producer", [args.nsc, "--emit-project-oop-lib", str(padded / "Producer.nsproj"),
                    "--cpp-clang-root=" + args.clang_root, "--no-artifact-cache", "-o", str(padded / "Producer.lib")])
                (padded / "api.h").rename(padded / "api.h.oculto")
                (negative / "Consumer.ns").write_text(
                    "int Main() { T09Cpp.Small v; return (int)v.values[0]; }\n", encoding="utf-8")
                (negative / "Consumer.nsproj").write_text(
                    '<Project><PropertyGroup><Name>T09Consumer</Name></PropertyGroup><ItemGroup>'
                    '<Compile Include="Consumer.ns" /><LibraryReference Include="producer/Producer.lib" />'
                    '</ItemGroup></Project>', encoding="utf-8")
                command = [args.nsc, "--dump-project-oop-hir", str(negative / "Consumer.nsproj"),
                           "--cpp-clang-root=" + args.clang_root]
                result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, text=True, errors="replace")
                log = root / "campo-no-proyectado-precompilado.log"
                log.write_text("COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) +
                               "\n" + result.stdout, encoding="utf-8")
                if result.returncode == 0 or "has no compatible member named 'values'" not in result.stdout:
                    raise SystemExit("el campo vectorial precompilado no se rechazo; evidencia: " + str(log))
            print(label + ": codigo 0", flush=True)
print("OK: 16 consumidores precompilados; evidencia:", root)
