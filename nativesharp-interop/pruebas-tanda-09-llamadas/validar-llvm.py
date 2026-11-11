"""T09-002: comprueba la emision LLVM de llamadas y globales C."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--nsc", required=True)
parser.add_argument("--clang-root", required=True)
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix="nsc-t09-llvm-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
print("Evidencia:", root, flush=True)


def run(label, command):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    (root / (label + ".log")).write_text(
        "COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" +
        result.stdout, encoding="utf-8")
    if result.returncode:
        raise SystemExit(label + ": fallo; evidencia: " + str(root))
    return result.stdout


if "clang version 23.1.0" not in run("clang-version", ["clang.exe", "--version"]):
    raise SystemExit("Se requiere Clang 23.1.0; evidencia: " + str(root))
for name in ("api.h", "api-dll.h", "api.c", "Main.ns", "Llamadas.nsproj", "LlamadasDll.nsproj"):
    shutil.copyfile(fixture / name, root / name)
run("productor", ["clang.exe", "-target", "x86_64-pc-windows-msvc", "-std=c11",
                  "-c", str(root / "api.c"), "-o", str(root / "api.obj")])
run("archivo", ["llvm-lib.exe", "/nologo", "/out:" + str(root / "api.lib"), str(root / "api.obj")])
run("dll", ["lld-link.exe", "/nologo", "/dll", "/noentry", "/nodefaultlib",
            "/export:no_args", "/export:sum_scalars", "/export:increment_pointer",
            "/export:check_variadic", "/export:shared_value,DATA",
            "/out:" + str(root / "api.dll"), "/implib:" + str(root / "api-dll.lib"),
            str(root / "api.obj")])
for mode, project in (("estatica", "Llamadas.nsproj"), ("dll", "LlamadasDll.nsproj")):
    output = run(mode + "-llvm", [args.nsc, "--dump-project-oop-llvm", str(root / project),
                 "--cpp-clang-root=" + args.clang_root, "--library-reference-root", str(root)])
    import_attr = "dllimport " if mode == "dll" else ""
    expected = ["@shared_value = external " + import_attr + "global i32",
                "declare " + import_attr + "i32 @check_variadic(i32, ...)",
                "store i32 31, ptr @shared_value", "double 2.250000e+00",
                "i64 -5000000000", "ptr %abi.arg.copy"]
    missing = [item for item in expected if item not in output]
    for target in (r"@check_variadic", r"%[\w.]+"):
        if not re.search(r"call i32 \(i32, \.\.\.\) " + target + r"\(i32 11, i32 -2, double", output):
            missing.append("llamada variadica " + target)
    if missing:
        raise SystemExit(mode + ": faltan " + repr(missing) + "; evidencia: " + str(root))
    print("OK:", mode, "LLVM conserva llamadas, promociones y global", flush=True)
print("Evidencia:", root)
