"""T09: valida cache directa C e invalidacion de dependencias."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-cache-", dir=r"A:\temp"))
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(root)
for name in ("Api.h", "nested.h", "Main.ns", "Main.nsproj"):
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

def build(reason, value):
    exe.unlink(missing_ok=True)
    result = invoke(command)
    if result.returncode != 0 or "compilation-bundle-cache: " + reason not in result.stdout:
        raise SystemExit("Cache inesperada, se esperaba " + reason + "; evidencia: " + str(root))
    result = invoke([str(exe)])
    if result.returncode != value:
        raise SystemExit("Resultado inesperado, se esperaba " + str(value) + "; evidencia: " + str(root))
    print(reason, value, "correcto", flush=True)

build("not-found", 12)
build("hit", 12)
(root / "optional.h").write_text("/* existe */\n", encoding="ascii")
build("not-found", 14)
build("hit", 14)
(root / "nested.h").write_text("#define NEST 3\n", encoding="ascii")
build("not-found", 16)
build("hit", 16)
(root / "nested.h").write_text("#error invalid_transitive_header\n", encoding="ascii")
failed = invoke(command)
if failed.returncode == 0 or "invalid_transitive_header" not in failed.stdout:
    raise SystemExit("Cache oculto un error del encabezado; evidencia: " + str(root))
(root / "nested.h").write_text("#define NEST 3\n", encoding="ascii")
build("hit", 16)
with (root / "Api.h").open("a", encoding="ascii") as header:
    header.write("static const char *unused_time = __TIME__;\n")
build("disabled", 16)
build("disabled", 16)
print("OK: cache C directa; evidencia:", root)
