"""Comprobacion dirigida T09-015: ruta de una biblioteca C ausente."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--nsc", required=True, help="Compilador de desarrollo a verificar")
args = parser.parse_args()

fixture = Path(__file__).resolve().parent
repo = fixture.parents[1]
missing = fixture / "biblioteca_ausente_t09.lib"
if missing.exists():
    raise SystemExit("La biblioteca de prueba debe permanecer ausente: " + str(missing))

temp = Path(tempfile.mkdtemp(prefix="nsc-t09-lib-ausente-", dir=r"A:\temp"))
env = os.environ.copy()
env["TMP"] = env["TEMP"] = str(temp)
command = [
    args.nsc,
    "--emit-project-oop-exe",
    str(fixture / "MissingCppLibrary.nsproj"),
    "--library-reference-root",
    str(fixture),
    "--no-artifact-cache",
    "-o",
    str(temp / "no_debe_existir.exe"),
]
result = subprocess.run(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors="replace")
output = result.stdout
(temp / "diagnostico.txt").write_text(output, encoding="utf-8")
(temp / "resultado.txt").write_text(str(result.returncode) + "\n", encoding="utf-8")
normalized = output.replace("\\", "/").casefold()
expected_path = missing.as_posix().casefold()
if (result.returncode == 0 or "referenced library does not exist" not in normalized
        or expected_path not in normalized or "internal error" in normalized):
    print(output)
    raise SystemExit("FALLO: el diagnostico no identifica la biblioteca esperada; evidencia: " + str(temp))
print("OK: la referencia C ausente identifica su ruta; evidencia: " + str(temp))
