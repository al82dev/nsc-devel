#!/usr/bin/env python3
"""
NativeSharp -- lector de la corrida. Convierte _logs\\ entero en UN SOLO archivo.

POR QUE EXISTE
--------------
Despues de `all.bat`, _logs\\ tiene siete archivos y unos cuantos megas. El veredicto esta en
SUMMARY.txt, el motivo real esta repartido entre 01_build.log, 02_ctest.log, 03_fixtures.log y
04_examples.log, y encontrarlo significa abrir cuatro archivos y buscar. Eso es trabajo mecanico
que una maquina hace mejor y sin equivocarse de archivo.

Este script produce `_logs\\REPORTE.txt`: el veredicto primero, y debajo el EXTRACTO del log de
cada cosa que fallo, ya recortado. El reporte es AUTOCONTENIDO a proposito -- si algo fallo, el
fragmento del log que lo explica esta dentro, no referenciado. Un reporte que dice "ver
02_ctest.log" no ha ahorrado nada.

QUE NO HACE
-----------
No juzga. No decide si un fallo es grave. No adivina la causa. Copia lo que el log dice, en el
orden en que hace falta leerlo. La interpretacion es de quien lee, y una herramienta que
interpreta por su cuenta es una herramienta en la que hay que dejar de confiar el dia que se
equivoca.

CODIGO DE SALIDA
----------------
0 si la corrida entera salio limpia; 1 si hay cualquier fallo o si la corrida murio a la mitad.
Asi `all.bat` puede terminar diciendo la verdad con una sola linea.
"""

from __future__ import annotations  # las anotaciones no se evaluan: corre en cualquier 3.7+

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
LOGS = ROOT / "_logs"
REPORT = LOGS / "REPORTE.txt"

# Tope global. Un reporte que no cabe de una sentada vuelve a ser un log.
MAX_LINES = 700

# Cuanto se copia de cada extracto antes de recortar.
CTEST_TAIL = 40
FIXTURE_TAIL = 30
BUILD_MAX = 60


def read(path: Path) -> list[str]:
    """Los logs de Windows mezclan cp1252 y UTF-8. Se leen en bytes y se decodifica sin morir."""
    if not path.is_file():
        return []
    return path.read_bytes().decode("utf-8", errors="replace").splitlines()


def ascii_only(text: str) -> str:
    """El reporte se mira con `type` en cmd.exe. ASCII puro para que no salga basura."""
    return text.encode("ascii", errors="replace").decode("ascii")


class Report:
    def __init__(self) -> None:
        self.lines: list[str] = []
        self.truncated = False

    def add(self, text: str = "") -> None:
        if len(self.lines) >= MAX_LINES:
            self.truncated = True
            return
        self.lines.append(ascii_only(text.rstrip()))

    def section(self, title: str) -> None:
        self.add()
        self.add("=" * 78)
        self.add(title)
        self.add("=" * 78)

    def block(self, source: list[str], limit: int, label: str) -> None:
        """Copia un extracto, recortando por el PRINCIPIO: lo util de un fallo esta al final."""
        cut = source[-limit:] if len(source) > limit else source
        if len(source) > limit:
            self.add("    [... %d lineas antes, ver %s ...]" % (len(source) - limit, label))
        for line in cut:
            self.add("    " + line)


# --------------------------------------------------------------------------------------
# SUMMARY.txt -- el veredicto del checklist, linea a linea.
# --------------------------------------------------------------------------------------

def parse_summary(summary: list[str]) -> tuple[list[str], list[str], int, int, bool]:
    failures = [line.strip() for line in summary if line.strip().startswith("[FAIL]")]
    skipped = [line.strip() for line in summary if line.strip().startswith("[SKIP]")]
    passed = failed = -1
    finished = False
    for line in summary:
        stripped = line.strip()
        if stripped.startswith("Passed:"):
            passed = int(re.sub(r"\D", "", stripped) or -1)
        elif stripped.startswith("Failed:"):
            failed = int(re.sub(r"\D", "", stripped) or -1)
        elif stripped.startswith("Finished:"):
            finished = True
    return failures, skipped, passed, failed, finished


# --------------------------------------------------------------------------------------
# 02_ctest.log -- los tests que fallaron, CON la salida que imprimieron.
#
# El formato de ctest --output-on-failure pone la salida del test JUSTO DEBAJO de su linea
# "***Failed", y el siguiente test empieza con una linea "        Start  N:". Ese par delimita el
# extracto sin necesidad de conocer nada del test.
# --------------------------------------------------------------------------------------

CTEST_FAIL = re.compile(r"^\s*\d+/\d+\s+Test\s+#(\d+):\s+(\S+)\s+\.*\*\*\*(\w+)")
CTEST_START = re.compile(r"^\s*Start\s+\d+:")


def parse_ctest(ctest: list[str]) -> tuple[list[tuple[str, str, list[str]]], str]:
    failures: list[tuple[str, str, list[str]]] = []
    index = 0
    while index < len(ctest):
        match = CTEST_FAIL.match(ctest[index])
        if match is None:
            index += 1
            continue
        name = match.group(2)
        kind = match.group(3).strip()
        body: list[str] = []
        index += 1
        while index < len(ctest) and not CTEST_START.match(ctest[index]):
            if ctest[index].strip():
                body.append(ctest[index])
            index += 1
        failures.append((name, kind, body))

    verdict = ""
    for line in ctest:
        if "tests passed" in line and "out of" in line:
            verdict = line.strip()
    return failures, verdict


# --------------------------------------------------------------------------------------
# 01_build.log -- con /WX un warning ES un error, asi que los dos importan.
# --------------------------------------------------------------------------------------

BUILD_INTERESTING = re.compile(r"(\berror\b|\bwarning\b|^FAILED:|ninja: build stopped|CMake Error)", re.IGNORECASE)


def parse_build(build: list[str]) -> tuple[bool, list[str]]:
    hits = [line.rstrip() for line in build if BUILD_INTERESTING.search(line)]
    # Deduplicado conservando el orden: clang repite el mismo diagnostico por cada unidad que
    # incluye la cabecera, y treinta copias de la misma linea no dicen mas que una.
    seen: set[str] = set()
    unique: list[str] = []
    for line in hits:
        key = line.strip()
        if key in seen:
            continue
        seen.add(key)
        unique.append(line)
    broke = any("FAILED:" in line or "ninja: build stopped" in line or "CMake Error" in line for line in build)
    return broke, unique


# --------------------------------------------------------------------------------------
# 03_fixtures.log / 04_examples.log -- bloques "--- <etiqueta> ---" ... "[exit N]".
# --------------------------------------------------------------------------------------

FIXTURE_HEADER = re.compile(r"^---\s+(.*?)\s+---\s*$")


def fixture_blocks(log: list[str]) -> dict[str, list[str]]:
    blocks: dict[str, list[str]] = {}
    current: str | None = None
    for line in log:
        header = FIXTURE_HEADER.match(line.strip())
        if header is not None:
            current = header.group(1)
            blocks[current] = []
            continue
        if current is not None:
            blocks[current].append(line.rstrip())
    return blocks


def find_block(blocks: dict[str, list[str]], label: str) -> tuple[str, list[str]] | None:
    """La etiqueta del SUMMARY es "nombre -O0"; la del log es "nombre -O0" o "nombre -O0 build+link"."""
    for key, body in blocks.items():
        if key == label or key.startswith(label + " "):
            return key, body
    return None


def main() -> int:
    if not LOGS.is_dir():
        print("report.py: no existe _logs\\ -- la corrida no llego a empezar.")
        return 1

    summary = read(LOGS / "SUMMARY.txt")
    build = read(LOGS / "01_build.log")
    ctest = read(LOGS / "02_ctest.log")
    fixtures = fixture_blocks(read(LOGS / "03_fixtures.log"))
    examples = fixture_blocks(read(LOGS / "04_examples.log"))
    triage = read(LOGS / "TRIAGE.txt")
    manifest = read(LOGS / "06_manifest.log")

    # DESFASE DE VINTAGE. _logs se borra entero al arrancar el checklist, asi que en una corrida
    # normal todos sus archivos son de la misma. Si NO lo son -- porque alguien corrio un paso
    # suelto a mano -- el reporte estaria mezclando estados del arbol, y eso es exactamente como
    # se lee un fallo que ya no existe o se da por bueno un verde viejo. Vale mas avisarlo.
    stamps = []
    for name in ("SUMMARY.txt", "01_build.log", "02_ctest.log", "03_fixtures.log", "04_examples.log"):
        path = LOGS / name
        if path.is_file():
            stamps.append((name, path.stat().st_mtime))
    spread = (max(t for _, t in stamps) - min(t for _, t in stamps)) if len(stamps) > 1 else 0.0

    failures, skipped, passed, failed, finished = parse_summary(summary)
    ctest_failures, ctest_verdict = parse_ctest(ctest)
    build_broke, build_hits = parse_build(build)

    report = Report()
    report.add("NativeSharp -- REPORTE DE LA CORRIDA")
    report.add("Generado por report.py a partir de _logs\\. Todo lo necesario esta AQUI DENTRO.")
    report.add()

    # ---- 1. VEREDICTO ------------------------------------------------------------------
    report.section("1. VEREDICTO")
    if not summary:
        report.add("SUMMARY.txt NO EXISTE. El checklist no llego a escribir nada:")
        report.add("la corrida murio antes del primer paso, o no se ejecuto.")
    elif not finished:
        report.add("LA CORRIDA MURIO A LA MITAD -- SUMMARY.txt no tiene linea 'Finished:'.")
        report.add("Lo que sigue es lo que alcanzo a registrarse, no un resultado completo.")
    if passed >= 0 or failed >= 0:
        report.add("Passed: %d   Failed: %d" % (passed, failed))
    if ctest_verdict:
        report.add("ctest:  %s" % ctest_verdict)
    if build_broke:
        report.add("BUILD:  ROTO -- nada de lo de abajo mide el arbol nuevo.")
    if spread > 900.0:
        report.add()
        report.add("AVISO -- LOS LOGS NO SON DE LA MISMA CORRIDA.")
        report.add("Entre el mas viejo y el mas nuevo hay %d minutos, y _logs se borra entero al" % int(spread // 60))
        report.add("arrancar el checklist. Alguien corrio un paso suelto: lo de abajo mezcla dos")
        report.add("estados del arbol. Correr `all.bat` limpio antes de creerse nada de esto.")
        for name, stamp in sorted(stamps, key=lambda item: item[1]):
            report.add("    %-20s %s" % (name, __import__("time").strftime("%Y-%m-%d %H:%M:%S", __import__("time").localtime(stamp))))
    report.add()
    if failures:
        report.add("FALLOS (%d):" % len(failures))
        for line in failures:
            report.add("  " + line)
    elif summary and finished and not build_broke:
        report.add("SIN FALLOS.")
    if skipped:
        report.add()
        report.add("OMITIDOS (%d):" % len(skipped))
        for line in skipped:
            report.add("  " + line)

    # ---- 2. BUILD ----------------------------------------------------------------------
    report.section("2. BUILD  (01_build.log)")
    if not build:
        report.add("01_build.log no existe.")
    elif not build_hits:
        report.add("Limpio: ni un error ni un warning. Se compila con /WX, asi que esto es real.")
    else:
        report.add("%d lineas con error/warning (deduplicadas):" % len(build_hits))
        report.add()
        report.block(build_hits, BUILD_MAX, "01_build.log")

    # ---- 3. CTEST ----------------------------------------------------------------------
    report.section("3. CTEST  (02_ctest.log)")
    if not ctest:
        report.add("02_ctest.log no existe -- ctest no llego a correr.")
    elif not ctest_failures:
        report.add("Todos los tests pasaron. %s" % ctest_verdict)
    else:
        report.add("%d test(s) fallaron. Su salida, entera:" % len(ctest_failures))
        for name, kind, body in ctest_failures:
            report.add()
            report.add("--- %s  (%s) ---" % (name, kind))
            if body:
                report.block(body, CTEST_TAIL, "02_ctest.log")
            else:
                report.add("    (el test no imprimio nada -- probablemente murio sin llegar a")
                report.add("     ejecutar sus comprobaciones)")

    # ---- 4. FIXTURES -------------------------------------------------------------------
    #
    # AQUI VA LA LISTA ENTERA, no solo lo que fallo, y es deliberado. Bajo ctest en Windows las
    # fixtures COMPILAN Y ENLAZAN pero NO SE EJECUTAN, asi que un ctest verde no dice nada de sus
    # codigos de salida: la unica prueba de que una fixture DA 42 es su linea aqui. Un reporte que
    # solo listara los fallos obligaria a abrir SUMMARY.txt para contestar "y la fixture nueva,
    # corrio?" -- que es justo la pregunta que hay que hacerse despues de anadir una, y justo el
    # descuido que dejo pasar el ID 127 durante dos rondas.
    report.section("4. FIXTURES  (veredicto por caso; los fallos, con su log)")
    roster = [
        line.strip() for line in summary
        if line.strip().startswith("[ OK ]") or line.strip().startswith("[FAIL]")
        or line.strip().startswith("[SKIP]")
    ]
    if not roster:
        report.add("SUMMARY.txt no tiene ninguna linea de veredicto.")
    else:
        for line in roster:
            report.add("  " + line)

    fixture_failures = [
        line for line in failures
        if not line.startswith("[FAIL] ctest") and not line.startswith("[FAIL] bug_triage")
    ]
    if fixture_failures:
        report.add()
        report.add("--- el log de cada fixture que fallo ---")
        for line in fixture_failures:
            label = line[len("[FAIL]"):].split("--")[0].strip()
            report.add()
            report.add("--- %s ---" % line.strip())
            found = find_block(fixtures, label) or find_block(examples, label)
            if found is None:
                report.add("    (sin bloque en los logs de fixtures)")
            else:
                report.block(found[1], FIXTURE_TAIL, "03_fixtures.log/04_examples.log")

    # ---- 5. TRIAGE ---------------------------------------------------------------------
    report.section("5. TRIAGE DE BUGS REPORTADOS  (TRIAGE.txt)")
    if not triage:
        report.add("TRIAGE.txt no existe.")
    else:
        alive = any("[VIVO]" in line or "VIVO" in line.upper() for line in triage)
        if not alive:
            report.add("Cero bugs vivos.")
        report.block([line.rstrip() for line in triage], 40, "TRIAGE.txt")

    # ---- 6. MANIFIESTO -----------------------------------------------------------------
    report.section("6. MANIFIESTO  (06_manifest.log)")
    if not manifest:
        report.add("06_manifest.log no existe -- verify_manifest.py no llego a correr.")
    else:
        report.block([line.rstrip() for line in manifest], 40, "06_manifest.log")

    if report.truncated:
        report.lines.append("")
        report.lines.append("[REPORTE RECORTADO al tope de %d lineas.]" % MAX_LINES)

    LOGS.mkdir(exist_ok=True)
    REPORT.write_text("\n".join(report.lines) + "\n", encoding="ascii", errors="replace")

    broken = bool(failures) or build_broke or not finished or bool(ctest_failures)
    print("report.py: escrito %s (%d lineas)." % (REPORT, len(report.lines)))
    return 1 if broken else 0


if __name__ == "__main__":
    sys.exit(main())
