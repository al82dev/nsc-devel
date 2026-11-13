"""T09: compara llamadas C preserve_none desde C y NativeSharp."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-preserve-all-estrechos-", dir=r"A:\temp"))
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
        for name in ("api.h", "api-dll.h", "api.c", "Control.c", "Main.ns",
                     "VectorCall.nsproj", "VectorCallDll.nsproj"):
            shutil.copyfile(fixture / name, case / name)
        prefix = mode + "-O" + str(opt)
        flags = ["-target", "x86_64-pc-windows-msvc", "-std=c11", "-O" + str(opt)]
        run(prefix + "-productor", ["clang.exe", *flags, "-c", str(case / "api.c"), "-o", str(case / "api.obj")], repo)
        symbols = run(prefix + "-simbolos", ["llvm-nm.exe", "--defined-only", str(case / "api.obj")], repo)
        if "t09_regcall" not in symbols:
            raise SystemExit(prefix + ": simbolo regcall inesperado; evidencia: " + str(root))
        if mode == "estatica":
            run(prefix + "-archivo", ["llvm-lib.exe", "/nologo", "/out:" + str(case / "api.lib"), str(case / "api.obj")], repo)
            control_flags = []
            library = "api.lib"
            project = "VectorCall.nsproj"
        else:
            run(prefix + "-dll", ["lld-link.exe", "/nologo", "/dll", "/noentry",
                                  "/nodefaultlib", "/export:t09_regcall", "/export:t09_uc", "/export:t09_sh", "/export:t09_us", "/export:t09_get_sc", "/export:t09_get_uc", "/export:t09_get_sh", "/export:t09_get_us", "/out:" + str(case / "api.dll"),
                                  "/implib:" + str(case / "api-dll.lib"), str(case / "api.obj")], repo)
            control_flags = ["-DAPI_DLL_IMPORT"]
            library = "api-dll.lib"
            project = "VectorCallDll.nsproj"
        run(prefix + "-control-obj", ["clang.exe", *flags, *control_flags, "-c",
                                     str(case / "Control.c"), "-o", str(case / "control.obj")], repo)
        run(prefix + "-control-link", ["lld-link.exe", "/nologo", "/entry:main",
                                      "/subsystem:console", "/nodefaultlib",
                                      "/out:" + str(case / "control.exe"), str(case / "control.obj"), str(case / library)], repo)
        c_output = run(prefix + "-control-run", [str(case / "control.exe")], repo)
        if args.solo_c:
            print(prefix + ": control C termino con codigo 0", flush=True)
            continue
        out = case / "out" / "VectorCall.exe"
        out.parent.mkdir()
        ir = run(prefix + "-ns-ir", [args.nsc, "--dump-project-oop-llvm", str(case / project),
                 "--library-reference-root", str(case), "--cpp-clang-root=" + args.clang_root], repo)
        if not re.search(r'call\s+preserve_nonecc\s+i8\s+@(?:"\\01)?t09_regcall', ir):
            raise SystemExit(prefix + ": convencion o simbolo LLVM incorrecto; evidencia: " + str(root))
        for return_type, symbol in (("i8", "t09_uc"), ("i16", "t09_sh"), ("i16", "t09_us")):
            if not re.search(r'call\s+preserve_nonecc\s+' + return_type + r'\s+@(?:"\\01)?' + symbol, ir):
                raise SystemExit(prefix + ": falta firma estrecha " + symbol + "; evidencia: " + str(root))
        for return_type in ("i8", "i16"):
            if not re.search(r'call\s+preserve_nonecc\s+' + return_type + r'\s+%[^\s(]+', ir):
                raise SystemExit(prefix + ": falta llamada indirecta estrecha; evidencia: " + str(root))
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
            for expected in ("not-found", "hit"):
                out.unlink(missing_ok=True)
                output = run(prefix + "-cache-" + expected, cache_args, repo)
                if "compilation-bundle-cache: " + expected not in output:
                    raise SystemExit(prefix + ": resultado de cache inesperado; evidencia: " + str(root))
                run(prefix + "-cache-run-" + expected, [str(out)], repo)
        print(prefix + ": C y NativeSharp terminaron con codigo 0", flush=True)
print("OK: ocho controles C" if args.solo_c else "OK: ocho comparaciones C y NativeSharp",
      "; evidencia:", root)
