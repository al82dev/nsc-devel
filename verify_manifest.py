"""MANIFEST_SHA256.txt -- verificacion y regeneracion.

CC1 fase 5, tareas 67 y 68.

POR QUE ESTE ARCHIVO CRECIO. La tarea 67 decia "regenerar MANIFEST_SHA256.txt CON
verify_manifest.py". Este script solo sabia VERIFICAR: no habia generador en ningun sitio del
arbol. Es la tercera tarea del milestone cuyo texto da por hecha una pieza que no existe, despues
de la 40 y la 47 (las dos daban por hecho un "pipeline clasico"). Asi que el generador se escribe
aqui, y con el la REGLA que define que entra en el manifiesto -- que hasta ahora solo existia
implicita, en la lista de 1109 rutas del propio archivo.

LA REGLA, reconstruida del manifiesto existente y ahora escrita:

  * en la raiz, los archivos `.md`, `.txt` y `.py`;
  * los diez directorios de DIRECTORIES, enteros y recursivos;
  * nada mas.

Queda FUERA, y conviene saberlo: `.git`, los arboles de build, `_logs`, `_logs_nslink`, y -- esto es lo que
sorprende -- los `.bat` de la raiz. Es decir, **el manifiesto no cubre los scripts que construyen y
verifican el paquete**. No lo cambio aqui: ampliar la cobertura es una DECISION sobre que significa
el manifiesto, no un arreglo del hallazgo R-001, que era simplemente que estaba desactualizado.
`--audit` lista lo que queda fuera para que la decision se pueda tomar mirando datos.

USO

  python verify_manifest.py                verifica (comportamiento original, mismos codigos de salida)
  python verify_manifest.py --audit        lista los archivos del arbol que la regla NO cubre
  python verify_manifest.py --clean        ARCHIVA los obsoletos de la tarea 68 en _sandbox/
  python verify_manifest.py --write        regenera, mostrando que cambio, y verifica al terminar

  Para el cierre del milestone:  python verify_manifest.py --clean --write
"""

from __future__ import annotations

import hashlib
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
MANIFEST = ROOT / "MANIFEST_SHA256.txt"

# Los diez directorios que el manifiesto cubre, enteros.
#
# `mkdown` entro el 2026-09-02, cuando la documentacion y los STAGE*_CHANGED_FILES.txt dejaron de
# vivir en la raiz. Sin esta linea el siguiente --write habria dejado ~120 documentos fuera del
# manifiesto SIN decir nada: no faltan, es que no se recorren, y un manifiesto que no cubre lo que
# cree cubrir es peor que no tenerlo.
DIRECTORIES = (
    "BUGS_ENCONTRADOS",
    "book_examples",
    "claude",
    "examples",
    "include",
    "lib",
    "mkdown",
    "src",
    "tests",
    "tools",
)

# Extensiones que se cubren en la RAIZ. Los .bat quedan fuera -- ver la cabecera.
ROOT_SUFFIXES = (".md", ".txt", ".py")

# Nunca se recorren, esten donde esten.
# `_logs_nslink` esta aqui por la misma razon que `_logs`: son artefactos de una corrida, no
# del paquete. Y hay que nombrarlo APARTE porque la comparacion es por nombre exacto de
# componente de ruta, no por prefijo: "_logs_nslink" no es "_logs", asi que sin esta linea
# entrarian al manifiesto los ~90 .obj y .exe de la corrida con NSLink y cambiarian en cada
# pasada.
EXCLUDED_DIRECTORIES = (
    ".git", "_logs", "_logs_lldlink", "_logs_nslink", "__pycache__", "_sandbox")

# Donde van a parar los archivos que el milestone deja atras. NO SE BORRAN: se archivan.
#
# La primera version de --clean los borraba. Borrar es una decision que no se puede deshacer sin
# git, y el valor de estos archivos no es cero: son el registro de por donde paso el trabajo. Un
# duplicado obsoleto molesta en la raiz del arbol; archivado no molesta a nadie y sigue ahi el dia
# que alguien pregunte "por que habia dos 68_triage".
#
# _sandbox/ queda FUERA del manifiesto (ver EXCLUDED_DIRECTORIES): es historia, no es el paquete.
SANDBOX = "_sandbox"

# CC1 tarea 68 -- duplicados y restos que el milestone deja atras. Cada uno tiene un sustituto
# vivo, y por eso se pueden archivar sin perder nada del estado actual:
#   * los dos `68_triage_*` parciales     -> 68_triage_y_cierre_de_bugs_reportados.md
#   * los dos FIX_*_CHANGED_FILES.txt     -> STAGE56_BUGFIX_CHANGED_FILES.txt
#   * 70_roadmap_consolidado.md           -> 70_roadmap_del_compilador.md
#   * `8`                                 -> nada; es un archivo suelto de 93 bytes creado por una
#                                            redireccion de shell mal escrita
OBSOLETE_FILES = (
    "68_triage_de_bugs_y_fix_b3.md",
    "68_triage_de_bugs_y_cuatro_fixes.md",
    "FIX_B3_CHANGED_FILES.txt",
    "FIX_B2_B3_B5_B8_CHANGED_FILES.txt",
    "70_roadmap_consolidado.md",
    "8",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def covered_files() -> list[str]:
    """Las rutas que la regla cubre, relativas a ROOT y con '/' siempre, ordenadas.

    El separador se normaliza a '/' a proposito: el manifiesto se genera en Windows y se lee
    tambien fuera, y un manifiesto que dependa del separador del sistema que lo escribio no es un
    mecanismo de integridad, es una trampa.
    """
    found: list[str] = []

    for entry in sorted(ROOT.iterdir()):
        # El manifiesto NO se incluye a si mismo. Es un `.txt` de la raiz, asi que la regla lo
        # cogeria; y un manifiesto que contenga su propio hash es IMPOSIBLE de satisfacer -- el
        # hash cambia en el momento de escribirlo. El manifiesto original tampoco se incluia (0
        # coincidencias de su propio nombre dentro de si mismo, comprobado); esto lo deja escrito
        # en vez de depender de que nadie lo note.
        #
        # Lo encontre probando este script contra un arbol de juguete, no leyendolo. Sin esa
        # prueba, la tarea 67 habria quedado con un criterio -- "reporta 0 fallos" -- imposible de
        # cumplir por construccion.
        if entry.name == MANIFEST.name:
            continue
        if entry.is_file() and entry.suffix in ROOT_SUFFIXES:
            found.append(entry.name)

    for directory in DIRECTORIES:
        base = ROOT / directory
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file():
                continue
            if any(part in EXCLUDED_DIRECTORIES for part in path.relative_to(ROOT).parts):
                continue
            found.append(path.relative_to(ROOT).as_posix())

    return sorted(set(found))


def read_manifest() -> dict[str, str]:
    if not MANIFEST.is_file():
        return {}
    entries: dict[str, str] = {}
    for raw in MANIFEST.read_text(encoding="utf-8").splitlines():
        if not raw.strip():
            continue
        try:
            expected, relative = raw.split("  ", 1)
        except ValueError:
            continue
        entries[relative.removeprefix("./")] = expected
    return entries


def verify() -> int:
    if not MANIFEST.is_file():
        print("ERROR: MANIFEST_SHA256.txt not found", file=sys.stderr)
        return 2

    checked = 0
    failed = 0
    for line_number, raw in enumerate(MANIFEST.read_text(encoding="utf-8").splitlines(), start=1):
        if not raw.strip():
            continue
        try:
            expected, relative = raw.split("  ", 1)
        except ValueError:
            print(f"FAIL line {line_number}: malformed manifest entry", file=sys.stderr)
            failed += 1
            continue
        relative = relative.removeprefix("./")
        path = ROOT / relative
        if not path.is_file():
            print(f"FAIL missing: {relative}", file=sys.stderr)
            failed += 1
            continue
        actual = sha256(path)
        checked += 1
        if actual != expected:
            print(f"FAIL hash: {relative}", file=sys.stderr)
            print(f"  expected {expected}", file=sys.stderr)
            print(f"  actual   {actual}", file=sys.stderr)
            failed += 1

    # El manifiesto puede estar completo en lo que lista y aun asi no cubrir el arbol. Un archivo
    # nuevo que nadie anadio es tan invalidante como un hash que no cuadra, y antes no se veia.
    listed = set(read_manifest())
    uncovered = [name for name in covered_files() if name not in listed]
    if uncovered:
        print(f"FAIL: {len(uncovered)} file(s) covered by the rule are missing from the manifest",
              file=sys.stderr)
        for name in uncovered[:20]:
            print(f"  not listed: {name}", file=sys.stderr)
        if len(uncovered) > 20:
            print(f"  ... and {len(uncovered) - 20} more", file=sys.stderr)
        failed += len(uncovered)

    if failed:
        print(f"MANIFEST: {checked} checked, {failed} failure(s)")
        return 1

    print(f"MANIFEST: {checked}/{checked} PASS")
    return 0


def audit() -> int:
    """Lo que hay en el arbol y la regla NO cubre. No es un fallo: es la lista con la que decidir
    si la cobertura del manifiesto es la que se quiere."""
    covered = set(covered_files())
    outside: list[str] = []
    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(ROOT)
        parts = relative.parts
        if any(part in EXCLUDED_DIRECTORIES for part in parts):
            continue
        if parts[0].startswith("build-"):
            continue
        name = relative.as_posix()
        if name == "MANIFEST_SHA256.txt" or name in covered:
            continue
        outside.append(name)

    print(f"AUDIT: {len(covered)} file(s) covered, {len(outside)} outside the rule")
    for name in sorted(outside):
        print(f"  outside: {name}")
    return 0


def clean() -> int:
    """Archiva los obsoletos en _sandbox/. No borra nada."""
    destination = ROOT / SANDBOX
    moved = 0
    for name in OBSOLETE_FILES:
        path = ROOT / name
        if not path.is_file():
            print(f"absent (nothing to do): {name}")
            continue
        destination.mkdir(exist_ok=True)
        target = destination / name
        if target.exists():
            # Ya hay uno archivado con ese nombre. No se pisa: lo archivado es historia, y una
            # historia que se sobrescribe no es historia. Se numera.
            index = 2
            while (destination / f"{name}.{index}").exists():
                index += 1
            target = destination / f"{name}.{index}"
        path.replace(target)
        print(f"archived: {name} -> {SANDBOX}/{target.name}")
        moved += 1
    print(f"CLEAN: {moved} file(s) archived of {len(OBSOLETE_FILES)} listed")
    if moved:
        print(f"        (nada se borro; estan en {SANDBOX}/ y siguen en git)")
    return 0


def write() -> int:
    previous = read_manifest()
    files = covered_files()

    lines = []
    current: dict[str, str] = {}
    for name in files:
        digest = sha256(ROOT / name)
        current[name] = digest
        lines.append(f"{digest}  {name}")
    # newline="\n" EXPLICITO. Sin el, `write_text` en Windows traduce cada \n a \r\n, y entonces
    # `read_text` fuera de Windows deja un \r pegado al final de cada RUTA -- con lo que ningun
    # archivo se encuentra y la verificacion falla entera. Dos parrafos mas arriba, en
    # covered_files(), este mismo archivo se molesta en normalizar el separador a '/' "porque el
    # manifiesto se genera en Windows y se lee tambien fuera"; escribirlo con CRLF echaba abajo la
    # mitad de esa portabilidad. Lo introduje yo en la primera version y lo vi comparando el
    # archivo generado contra el original.
    with MANIFEST.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write("\n".join(lines) + "\n")

    # Regenerar a ciegas esconde exactamente lo que hay que mirar: QUE cambio. Un manifiesto que se
    # rehace sin decirlo convierte cualquier deriva en verde.
    added = sorted(set(current) - set(previous))
    removed = sorted(set(previous) - set(current))
    changed = sorted(n for n in set(current) & set(previous) if current[n] != previous[n])

    print(f"WRITE: {len(current)} entries "
          f"(+{len(added)} added, -{len(removed)} removed, ~{len(changed)} changed)")
    for name in added:
        print(f"  + {name}")
    for name in removed:
        print(f"  - {name}")
    for name in changed:
        print(f"  ~ {name}")

    print()
    return verify()


def main(argv: list[str]) -> int:
    flags = set(argv[1:])
    unknown = flags - {"--audit", "--clean", "--write"}
    if unknown:
        print(f"ERROR: unknown option(s): {' '.join(sorted(unknown))}", file=sys.stderr)
        print(__doc__, file=sys.stderr)
        return 2

    if "--audit" in flags:
        return audit()

    status = 0
    if "--clean" in flags:
        status = clean()
        print()
    if "--write" in flags:
        return write()
    if "--clean" in flags:
        # Se borro algo, asi que el manifiesto quedo desactualizado por definicion.
        print("NOTE: run with --write to regenerate the manifest after cleaning")
        return status
    return verify()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
