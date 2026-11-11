"""T09-013: invalida cache cuando cambia una biblioteca C estatica."""
import argparse
import hashlib
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-cache-biblioteca-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
for name in ("Api.h", "Api.c", "Main.ns", "Main.nsproj"):
    shutil.copyfile(fixture / name, root / name)
exe = root / "Main.exe"
command = [args.nsc, "--cpp-clang-root=" + args.clang_root,
           "--emit-project-oop-exe", str(root / "Main.nsproj"), "-O0", "--time",
           "--artifact-cache=" + str(root / "cache"), "-o", str(exe)]
step = 0
print("Evidencia:", root, flush=True)

def invoke(argv):
    global step
    step += 1
    result = subprocess.run(argv, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    (root / ("paso-" + str(step) + ".log")).write_text(
        "COMMAND: " + repr(argv) + "\nEXIT: " + str(result.returncode) +
        "\n" + result.stdout, encoding="utf-8")
    return result

def produce(value):
    compiled = invoke(["clang.exe", "-target", "x86_64-pc-windows-msvc", "-std=c11",
                       "-O0", "-DVALUE=" + str(value), "-c", str(root / "Api.c"),
                       "-o", str(root / "api.obj")])
    if compiled.returncode:
        raise SystemExit("No se compilo el productor C; evidencia: " + str(root))
    archived = invoke(["llvm-lib.exe", "/nologo", "/out:" + str(root / "api.lib"),
                       str(root / "api.obj")])
    if archived.returncode:
        raise SystemExit("No se creo la biblioteca C; evidencia: " + str(root))
    return hashlib.sha256((root / "api.lib").read_bytes()).hexdigest()

def build(reason, value):
    exe.unlink(missing_ok=True)
    result = invoke(command)
    if result.returncode != 0 or "compilation-bundle-cache: " + reason not in result.stdout:
        print(result.stdout)
        raise SystemExit("Cache inesperada, se esperaba " + reason + "; evidencia: " + str(root))
    result = invoke([str(exe)])
    if result.returncode != value:
        raise SystemExit("Resultado inesperado, se esperaba " + str(value) + "; evidencia: " + str(root))
    print(reason, value, "correcto", flush=True)

first = produce(12)
build("not-found", 12)
build("hit", 12)
second = produce(14)
if first == second:
    raise SystemExit("La biblioteca no cambio de contenido; evidencia: " + str(root))
build("not-found", 14)
build("hit", 14)
(root / "hashes.txt").write_text(first + "\n" + second + "\n", encoding="ascii")
print("OK: cache invalida biblioteca cambiada; evidencia:", root)
