"""T09-010: comprueba el rechazo de escritura en una global C const."""
import argparse
import os
from pathlib import Path
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-global-const-", dir=r"A:\temp"))
env = os.environ.copy()
env["TMP"] = env["TEMP"] = str(root)
print("Evidencia:", root, flush=True)

def run(label, command):
    result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    (root / (label + ".log")).write_text(
        "COMMAND: " + repr(command) + "\nEXIT: " + str(result.returncode) + "\n" +
        result.stdout, encoding="utf-8")
    return result

version = run("clang-version", ["clang.exe", "--version"])
if version.returncode != 0 or "clang version 23.1.0" not in version.stdout:
    raise SystemExit("Se requiere Clang 23.1.0; evidencia: " + str(root))
control = run("control-c", ["clang.exe", "-target", "x86_64-pc-windows-msvc",
              "-std=c11", "-fsyntax-only", str(fixture / "Control.c")])
if (control.returncode == 0 or "t09_const_global" not in control.stdout
        or "const" not in control.stdout):
    raise SystemExit("Clang no identifico la global const; evidencia: " + str(root))
print("OK: Clang rechaza escritura de la global const", flush=True)
if not args.solo_c:
    result = run("nsc", [args.nsc, "--emit-project-oop-exe",
                 str(fixture / "GlobalConst.nsproj"),
                 "--cpp-clang-root=" + args.clang_root, "--no-artifact-cache",
                 "-o", str(root / "no_debe_existir.exe")])
    if (result.returncode == 0 or "left side of assignment is not assignable" not in result.stdout
            or "internal error" in result.stdout):
        raise SystemExit("NSC no produjo el rechazo esperado; evidencia: " + str(root))
    print("OK: NSC rechaza escritura de la global const")
print("Evidencia:", root)
