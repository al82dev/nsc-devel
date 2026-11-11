"""T09-001: comprueba representaciones HIR/MIR de llamadas C reales."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--nsc", required=True)
parser.add_argument("--clang-root", required=True)
args = parser.parse_args()
fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
root = Path(tempfile.mkdtemp(prefix="nsc-t09-hir-mir-", dir=r"A:\temp"))
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


version = run("clang-version", ["clang.exe", "--version"])
if "clang version 23.1.0" not in version:
    raise SystemExit("Se requiere Clang 23.1.0; evidencia: " + str(root))
for name in ("api.h", "api-dll.h", "api.c", "Main.ns", "Llamadas.nsproj", "LlamadasDll.nsproj"):
    shutil.copyfile(fixture / name, root / name)
run("productor", ["clang.exe", "-target", "x86_64-pc-windows-msvc", "-std=c11",
                  "-c", str(root / "api.c"), "-o", str(root / "api.obj")])
run("archivo", ["llvm-lib.exe", "/nologo", "/out:" + str(root / "api.lib"),
                str(root / "api.obj")])
run("dll", ["lld-link.exe", "/nologo", "/dll", "/noentry", "/nodefaultlib",
            "/export:no_args", "/export:sum_scalars", "/export:increment_pointer",
            "/export:check_variadic", "/export:shared_value,DATA",
            "/out:" + str(root / "api.dll"), "/implib:" + str(root / "api-dll.lib"),
            str(root / "api.obj")])
expected_hir = ("static-call type=builtin:int namespace-function=#4", "check_variadic",
                "delegate-create", "delegate-invoke", "conversion=integral-promotion",
                "conversion type=builtin:double")
expected_mir = ("direct-static-call type=builtin:int namespace-function=#4 callee=check_variadic",
                "delegate-create", "callee=check_variadic", "delegate-invoke",
                "conversion=integral-promotion", "convert type=builtin:double")
for mode, project in (("estatica", "Llamadas.nsproj"), ("dll", "LlamadasDll.nsproj")):
    common = [str(root / project), "--cpp-clang-root=" + args.clang_root,
              "--library-reference-root", str(root)]
    hir = run(mode + "-hir", [args.nsc, "--dump-project-oop-hir", *common])
    mir = run(mode + "-mir", [args.nsc, "--dump-project-oop-mir", *common])
    for stage, output, expected in (("HIR", hir, expected_hir), ("MIR", mir, expected_mir)):
        missing = [item for item in expected if item not in output]
        if missing:
            raise SystemExit(mode + " " + stage + ": faltan " + repr(missing) +
                             "; evidencia: " + str(root))
    print("OK:", mode, "HIR y MIR conservan las formas probadas", flush=True)
print("Evidencia:", root)
