"""T09: compara llamadas C++ sysv_abi desde C++ y NativeSharp."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--nsc", help="Compilador de desarrollo seleccionado")
parser.add_argument("--clang-root", help="Directorio de recursos de Clang 23.1.0")
parser.add_argument("--solo-c", action="store_true", help="Ejecutar solo el control C")
args = parser.parse_args()
if not args.solo_c and (not args.nsc or not args.clang_root):
    parser.error("--nsc y --clang-root son obligatorios para la comparacion completa")

fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix="nsc-t09-sysv-cpp-retorno-mixto-argumentos-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)

def run(label, argv, cwd, expected=0):
    result = subprocess.run(argv, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    log = root / (label + ".log")
    log.write_text("COMMAND: " + repr(argv) + "\nEXIT: " + str(result.returncode)
                   + "\n" + result.stdout, encoding="utf-8")
    if result.returncode != expected:
        print(result.stdout)
        raise SystemExit(label + ": codigo inesperado; evidencia: " + str(log))
    return result.stdout

version = run("clang-version", ["clang.exe", "--version"], repo)
if "clang version 23.1.0" not in version:
    raise SystemExit("Se requiere Clang 23.1.0; evidencia: " + str(root))

for mode in ("estatica", "dll"):
    for opt in range(4):
        case = root / (mode + "-O" + str(opt))
        case.mkdir()
        for name in ("api.h", "api-dll.h", "api.cpp", "Control.cpp", "Main.ns",
                     "VectorCall.nsproj", "VectorCallDll.nsproj"):
            shutil.copyfile(fixture / name, case / name)
        prefix = mode + "-O" + str(opt)
        flags = ["-target", "x86_64-pc-windows-msvc", "-std=c++20", "-O" + str(opt)]
        run(prefix + "-productor", ["clang.exe", *flags, "-c", str(case / "api.cpp"), "-o", str(case / "api.obj")], repo)
        symbols = run(prefix + "-simbolos", ["llvm-nm.exe", "--defined-only", str(case / "api.obj")], repo)
        if ("?most@T09Cpp@@YA?AUSmall@1@U21@HM@Z" not in symbols or
                "?fetchCallback@T09Cpp@@YAP6A?AUSmall@1@U21@HM@ZXZ" not in symbols):
            raise SystemExit(prefix + ": simbolo sysv_abi inesperado; evidencia: " + str(root))
        if mode == "estatica":
            run(prefix + "-archivo", ["llvm-lib.exe", "/nologo", "/out:" + str(case / "api.lib"), str(case / "api.obj")], repo)
            control_flags = []
            library = "api.lib"
            project = "VectorCall.nsproj"
        else:
            run(prefix + "-dll", ["lld-link.exe", "/nologo", "/dll", "/noentry",
                                  "/nodefaultlib", "/export:?most@T09Cpp@@YA?AUSmall@1@U21@HM@Z", "/export:?fetchCallback@T09Cpp@@YAP6A?AUSmall@1@U21@HM@ZXZ", "/out:" + str(case / "api.dll"),
                                  "/implib:" + str(case / "api-dll.lib"), str(case / "api.obj")], repo)
            control_flags = ["-DAPI_DLL_IMPORT"]
            library = "api-dll.lib"
            project = "VectorCallDll.nsproj"
        run(prefix + "-control-obj", ["clang.exe", *flags, *control_flags, "-c",
                                     str(case / "Control.cpp"), "-o", str(case / "control.obj")], repo)
        run(prefix + "-control-link", ["lld-link.exe", "/nologo", "/entry:main",
                                      "/subsystem:console", "/nodefaultlib",
                                      "/out:" + str(case / "control.exe"), str(case / "control.obj"), str(case / library)], repo)
        c_output = run(prefix + "-control-run", [str(case / "control.exe")], repo)
        if args.solo_c:
            print(prefix + ": control C++ termino con codigo 0", flush=True)
            continue
        out = case / "out" / "VectorCall.exe"
        out.parent.mkdir()
        ir = run(prefix + "-ns-ir", [args.nsc, "--dump-project-oop-llvm", str(case / project),
                 "--library-reference-root", str(case), "--cpp-clang-root=" + args.clang_root], repo)
        if ('?fetchCallback@T09Cpp@@YAP6A?AUSmall@1@U21@HM@ZXZ' not in ir or
                '?most@T09Cpp@@YA?AUSmall@1@U21@HM@Z' not in ir):
            raise SystemExit(prefix + ": faltan simbolos C++ en LLVM; evidencia: " + str(root))
        if not re.search(r'call\s+x86_64_sysvcc\s+\{\s*<2 x float>,\s*i32\s*\}\s+%[^\s(]+\(<2 x float>', ir):
            raise SystemExit(prefix + ": falta llamada indirecta sysv_abi con i64 e i32; evidencia: " + str(root))
        run(prefix + "-nsc", [args.nsc, "--emit-project-oop-exe", str(case / project),
                              "--library-reference-root", str(case),
                              "--cpp-clang-root=" + args.clang_root, "-O" + str(opt),
                              "--no-artifact-cache", "-o", str(out)], repo)
        ns_output = run(prefix + "-ns-run", [str(out)], repo)
        if c_output != ns_output:
            raise SystemExit(prefix + ": salidas diferentes; evidencia: " + str(root))
        if opt == 0:
            cache = case / "artifact-cache"
            cache_args = [args.nsc, "--emit-project-oop-exe", str(case / project),
                          "--library-reference-root", str(case), "--cpp-clang-root=" + args.clang_root,
                          "--artifact-cache=" + str(cache), "--time", "-O0", "-o", str(out)]
            for attempt in (1, 2):
                out.unlink(missing_ok=True)
                output = run(prefix + "-cache-" + str(attempt), cache_args, repo)
                if "compilation-bundle-cache: disabled" not in output:
                    raise SystemExit(prefix + ": resultado de cache inesperado; evidencia: " + str(root))
                run(prefix + "-cache-run-" + str(attempt), [str(out)], repo)
        print(prefix + ": C++ y NativeSharp terminaron con codigo 0", flush=True)
print("OK: ocho controles C++" if args.solo_c else "OK: ocho comparaciones C++ y NativeSharp",
      "; evidencia:", root)
