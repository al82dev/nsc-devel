"""T09-015: comprueba el diagnostico de un simbolo C sin definicion."""
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
root = Path(tempfile.mkdtemp(prefix="nsc-t09-simbolo-ausente-", dir=r"A:\temp"))
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
obj = root / "control.obj"
compiled = run("control-compilacion", ["clang.exe", "-target", "x86_64-pc-windows-msvc",
               "-std=c11", "-O0", "-c", str(fixture / "Control.c"), "-o", str(obj)])
if compiled.returncode != 0:
    raise SystemExit("El control C no compilo; evidencia: " + str(root))
link = run("control-enlace", ["lld-link.exe", "/nologo", "/entry:main", "/subsystem:console",
           "/nodefaultlib", "/out:" + str(root / "no_debe_existir.exe"), str(obj)])
symbol = "simbolo_ausente_t09"
if link.returncode == 0 or "undefined symbol" not in link.stdout or symbol not in link.stdout:
    raise SystemExit("El enlace C no identifico el simbolo ausente; evidencia: " + str(root))
print("OK: el enlace C identifica", symbol, flush=True)
if not args.solo_c:
    result = run("nsc", [args.nsc, "--emit-project-oop-exe",
                 str(fixture / "SimboloAusente.nsproj"),
                 "--cpp-clang-root=" + args.clang_root, "--no-artifact-cache",
                 "-o", str(root / "no_debe_existir-ns.exe")])
    if (result.returncode == 0 or "unresolved external symbol" not in result.stdout
            or symbol not in result.stdout or "internal error" in result.stdout):
        raise SystemExit("NSC no identifico el simbolo ausente; evidencia: " + str(root))
    print("OK: NSC identifica", symbol)
print("Evidencia:", root)
