#!/usr/bin/env python3
"""
NativeSharp -- PIPELINE AISLADO QUE ENLAZA CON NSLink, Y ENTREGA LOS FALLOS A QUIEN LO DESARROLLA.

QUE ES
------
Compila y ejecuta las mismas 42 fixtures y ejemplos que la corrida normal, pero pasando
`NSC_LLD_LINK=nslink.exe` para que `nsc` use NSLink en vez de lld-link. Compara caso por caso
contra la corrida de referencia con lld-link, y **deja el material de cada fallo en el arbol de
NSLink** para que quien lo desarrolla lo vea sin tener que instalar ni correr nada de aqui.

Dos preguntas, y esa es toda la ambicion del archivo:

    1. NSLink enlaza lo que el compilador emite HOY?
    2. Cuando no, que le hace falta a quien lo arregla para reproducirlo en un minuto?

QUE **NO** TOCA
---------------
  * NSLink. Ni su codigo, ni sus pruebas, ni su corpus curado. Lo unico que se escribe en su
    arbol es un directorio de ENTRADA, aparte y borrable, que no forma parte de su build.
  * Los scripts de construccion de este repo: all.bat, run_windows_checklist.bat, run_cases.py,
    report.py, tools\\*.cmd. Ni se modifican ni se invocan.

Llama a `nsc.exe` DIRECTAMENTE con los mismos dos comandos que `build_project.cmd` emite. Efectos
buscados: `prepare_libraries.cmd` no corre, asi que no hay `rmdir /s /q lib\\NativeSharp.Core` ni
la carrera que obliga a `NS_LIB_READY`; y nada se escribe dentro de `_logs\\`, para que report.py
no vea esto y el veredicto oficial no se contamine.

COMO SUSTITUYE EL ENLAZADOR, sin recompilar el compilador
--------------------------------------------------------
`LldLinker::discover()` (src/Linker/LldLinker.cpp) lee `NSC_LLD_LINK` ANTES de buscar `lld-link`
en el PATH -- su propio error lo dice: "set NSC_LLD_LINK to its executable path". Apuntarlo a
`nslink.exe` basta. Es lo que hace `C:\\native-sharp\\bin\\nslink-env.bat`, puesto aqui por
trabajo en vez de en la sesion para que la corrida no dependa de que alguien llamo al .bat.

QUE SE LE ENTREGA A NSLink, Y POR QUE ESO Y NO UN LOG
----------------------------------------------------
Por cada caso que falla se copia al buzon:

  * el `.obj` -- el COFF que NSLink no supo enlazar, emitido por LLVM a traves de `nsc`;
  * el log completo de `nsc`, que INCLUYE el mensaje del propio enlazador. `LldLinker`
    redirige stdout y stderr del enlazador a un fichero temporal, lo lee, y lo pega dentro de su
    `error` ("lld-link failed with process status N:\\n<diagnostico>"); ese `error` sale por el
    stderr de `nsc`, y aqui se captura entero.

El `.obj` importa mas que el log, y lo dice su propio `corpus/README.md`:

    "LLVM / clang-cl: no disponible en el entorno donde se construyo esta etapa. Su ausencia
     importa mas de lo que parece: LLVM emite COMDAT, .drectve y simbolos debiles, ninguno de
     los cuales aparece en los objetos de NSASM de este directorio."

Estos objetos son exactamente eso, y con ellos el fallo se reproduce con `nslink` a secas, sin
`nsc`, sin este script y sin esta maquina.

LO QUE NO SE PUEDE ENTREGAR, dicho para que nadie lo busque
----------------------------------------------------------
El fichero de respuesta (`<out>.link.rsp`). `nsc` lo escribe, lo usa y lo BORRA
(LldLinker.cpp: `remove(responsePath)`), asi que cuando este script ve el fallo ya no existe. No
se reconstruye a mano porque la lista de bibliotecas de importacion se genera en tiempo de
enlace y adivinarla seria dar por medido algo que no se midio. Lo que si esta documentado en el
buzon es la lista FIJA de argumentos que `nsc` pone siempre, copiada de LldLinker.cpp.

USO
---
    python nslink_pipeline.py                     # todo, -O0 y -O2, y deja el buzon
    python nslink_pipeline.py -j 8
    python nslink_pipeline.py --opt O0             # un solo nivel (o --opt=-O0)
    python nslink_pipeline.py --filter c23
    python nslink_pipeline.py --dry-run
    python nslink_pipeline.py --inbox D:\\otro\\sitio
    python nslink_pipeline.py --no-inbox           # no escribir nada fuera de este repo

Rutas, por si no estan en su sitio por omision:

    set NSC_EXE=C:\\native-sharp\\bin\\nsc.exe
    set NSLINK_EXE=C:\\native-sharp\\bin\\nslink.exe
    set NSLINK_SDK=C:\\native-sharp\\sdk\\win64
    set NSLINK_DEVEL=A:\\nslink-devel

QUE DEJA
--------
En ESTE repo (artefactos crudos de la corrida):

    _logs_nslink\\cases\\<etiqueta>_<opt>.log
    _logs_nslink\\bin\\<etiqueta>_<opt>.obj / .exe
    _logs_nslink\\results.json
    _logs_nslink\\REPORTE_NSLINK.txt

En el arbol de NSLink (el buzon, solo lo que le sirve a quien lo arregla):

    <nslink-devel>\\_inbox_nsc\\README.md
    <nslink-devel>\\_inbox_nsc\\REPORTE.txt
    <nslink-devel>\\_inbox_nsc\\fallos\\<etiqueta>_<opt>.log
    <nslink-devel>\\_inbox_nsc\\fallos\\<etiqueta>_<opt>.obj

`_logs_nslink\\` esta FUERA de `_logs\\` y no dentro, por una razon concreta: el checklist borra
`_logs\\` entero al arrancar, y el reporte desapareceria en cuanto se corriera `all.bat`.

CODIGO DE SALIDA
----------------
    0  ningun caso empeora respecto a la referencia con lld-link
    1  hay REGRESIONES (pasaba con lld-link, falla con NSLink)
    2  no se pudo arrancar (falta nsc.exe o nslink.exe, o el filtro no deja ningun caso)

Un caso que falla con LOS DOS enlazadores no cuenta como regresion y se lista aparte: eso no es
culpa de NSLink, y mezclarlo esconderia lo unico que este pipeline existe para ver.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent

# Artefactos crudos, en ESTE repo. No es el directorio de la corrida oficial a proposito:
# report.py construye el veredicto desde ahi y el checklist lo borra entero al arrancar.
#
# NSLINK_OUT_DIR lo pone `nslink_solo.bat`, el ciclo corto de cuando SOLO cambio nslink.exe. Ese
# ciclo escribe en `_logs_nslink_solo\` y NO puede tocar `_logs_nslink\`, que es la pasada 2 de la
# ultima corrida completa: si la pisara, el par (_logs_lldlink, _logs_nslink) dejaria de ser
# comparable -- serian dos medidas de dos momentos distintos con la misma cara.
OUT = ROOT / os.environ.get("NSLINK_OUT_DIR", "_logs_nslink")
CASE_LOGS = OUT / "cases"
BINDIR = OUT / "bin"
RESULTS = OUT / "results.json"
REPORT = OUT / "REPORTE_NSLINK.txt"

# ----------------------------------------------------------------------------------------------
# LAS DOS PASADAS DE all.bat. Cada una es un directorio COMPLETO y con la misma forma: SUMMARY,
# los seis logs numerados, TRIAGE, cases\, bin\ y su propio REPORTE.txt. No comparten un solo
# fichero, y este script no escribe dentro de ninguno de los dos salvo su propia comparacion.
PASS_LLD = ROOT / "_logs_lldlink"
PASS_NSLINK = OUT

# La corrida de referencia, SOLO PARA LEER. La escribe run_cases.py con lld-link.
REFERENCE = PASS_LLD / "cases" / "results.json"

# El buzon en el arbol de NSLink. Un directorio de ENTRADA, aparte de su corpus curado: no es
# nuestro sitio decidir que entra en `corpus\`, eso lo decide quien mantiene NSLink.
DEFAULT_NSLINK_DEVEL = Path(r"A:\nslink-devel")
INBOX_NAME = "_inbox_nsc"

# Los logs de la pasada con NSLink, COMPLETOS, DENTRO DEL BUZON. El buzon lleva el material de
# los fallos, elegido caso por caso; esto es la corrida entera al lado -- los ocho logs, el
# TRIAGE, el REPORTE y `cases\` -- para poder mirar cualquier cosa sin pedirla.
#
# VAN DENTRO DE `_inbox_nsc\` Y NO EN UN DIRECTORIO HERMANO A PROPOSITO. `_inbox_nsc` es el
# UNICO sitio del arbol de NSLink donde escribe nsc: no forma parte de su construccion, no entra
# en su manifiesto (su decision D-058) y se puede borrar entero. Un segundo directorio suelto
# tendria que ganarse esas tres cosas otra vez, y por nada: es el mismo material, del mismo
# emisor, con la misma vida.
#
# Se REEMPLAZA en cada corrida, por lo mismo que `fallos\`: un directorio donde se acumulan tres
# corridas es peor que no tenerlo, porque nadie sabe cual sigue viva.
#
# NO se copia `bin\` ni `linktemps\`: son cientos de megas de .exe y .obj por corrida, y lo que
# de ahi hace falta para reproducir un fallo ya va al buzon, elegido caso por caso.
LOGS_NAME = "logs-nsc-tests-with-nslink"
# `linktemps_ctest` es el de nslink_solo.bat -- se le puso otro nombre para que el paso de los
# 84 no lo borrara, y al renombrarlo se quedo FUERA de esta lista: la primera corrida del
# ciclo corto copio 5319 ficheros al arbol de NSLink. Cualquier directorio de temporales de
# enlace va aqui.
LOGS_SKIP = ("bin", "bin_triage", "linktemps", "linktemps_ctest")

# Tope de lo que se copia al buzon. Un .obj de estos anda por los 100 KB; un corpus entero en
# rojo serian varios MB
# en el arbol de otro. Se avisa en vez de llenarselo en silencio.
INBOX_MAX_BYTES = 64 * 1024 * 1024

# Dos numeros y un espacio, para que `all.bat` los sume a su resumen sin parsear JSON. Es el
# mismo truco que run_cases.py usa con su counts.txt, y por la misma razon: cmd.exe no lee JSON
# sin pelearse.
COUNTS = OUT / "counts.txt"

# NSL-ACT-001. Donde `nsc` deja los temporales del enlace cuando NSC_KEEP_LINK_TEMPS esta puesta.
# Un subdirectorio POR TRABAJO: con dieciseis en paralelo, una raiz compartida seria adivinar cual
# de los `nativesharp-stage20-*` es de quien.
LINKTEMPS = OUT / "linktemps"

# LA OFERTA DE CORPUS. Cuando NSLink no falla nada, el buzon quedaria vacio y quien lo desarrolla
# se quedaria sin nada que hacer en paralelo. Estos seis casos se copian SIEMPRE, fallen o no,
# porque cada uno ejercita una parte distinta del enlazador y su corpus dice que le faltan
# objetos de LLVM. Es una OFERTA: que entre en `corpus\` lo decide quien mantiene NSLink.
CORPUS_OFFERING: list[tuple[str, str]] = [
    ("c1_14_callback_exception_boundary_O0",
     "Excepcion cruzando un callback de C: .pdata/.xdata, o sea PeExceptionTable."),
    ("c1_13_two_libraries_same_symbol_O0",
     "Dos bibliotecas definiendo el mismo simbolo: resolucion de simbolos pura."),
    ("cc1_c16_enum_layout_O0",
     "Layout de enum: tamanos y offsets que el enlazador tiene que respetar tal cual."),
    ("cc1_c18_stackalloc_hardening_O0",
     "Memoria de pila con guardias: toca __chkstk y el prologo de marco."),
    ("cc1_c23_views_and_lifetimes_O2",
     "El caso mas grande del arbol, a -O2: genericos monomorfizados, COMDAT y secciones "
     "plegables. Es el que mas se parece a lo que un compilador de verdad emite."),
    ("project_stage56_c1_O0",
     "Interop completo: importaciones, exportaciones y ABI de C."),
]

# Un trabajo que tarda mas que esto esta colgado, no lento.
JOB_TIMEOUT_SECONDS = 300

# LA TABLA DE CASOS VIVE EN cases_table.py, y desde el 2026-09-03 hay UNA SOLA. Estaba escrita
# aqui y otra vez en el otro script, con un aviso pidiendo que quien anadiera una fixture se
# acordara de las dos. Ese aviso no era un guardian: el doc 109 cuenta la vez que las dos tablas
# quedaron en 48 y 44 y la corrida de NSLink salio verde midiendo cuatro casos menos.
#
# El modulo es de DATOS: no importa codigo, no ejecuta nada, no tiene dependencias. Los dos
# scripts siguen siendo independientes entre si.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cases_table import CASES, IC, LC, CC  # noqa: E402


OPT_LEVELS = ("-O0", "-O2")

# La lista FIJA de argumentos que `nsc` mete en el fichero de respuesta para un ejecutable,
# copiada de src/Linker/LldLinker.cpp (linkConsoleExecutable). Se documenta en el buzon porque el
# .rsp de verdad se borra y esto es lo unico que se puede afirmar sin adivinar.
EXECUTABLE_RESPONSE_ARGUMENTS = [
    "/nodefaultlib",
    "/entry:__nativesharp_start",
    "/subsystem:console",
    "/machine:x64",
    "/dynamicbase",
    "/nxcompat",
    "/highentropyva",
    "/out:<salida.exe>",
    "<programa.obj>",
    "<runtime.obj>          (temporal, lo genera nsc en cada enlace)",
    "<bibliotecas de importacion...>",
]


# ----------------------------------------------------------------------------------------------
# Localizar las herramientas. Se falla ANTES de compilar nada, con la lista de lo que se probo:
# una corrida entera que muere en el trabajo 40 porque falta un ejecutable es tiempo tirado.
# ----------------------------------------------------------------------------------------------
def locate(kind: str, env_name: str, default: Path) -> Path | None:
    tried: list[Path] = []
    override = os.environ.get(env_name)
    if override:
        candidate = Path(override)
        tried.append(candidate)
        if candidate.is_file():
            return candidate
    tried.append(default)
    if default.is_file():
        return default
    print(f"[ERROR] no encuentro {kind}. Probado:", file=sys.stderr)
    for path in tried:
        print(f"        {path}", file=sys.stderr)
    print(f"        Se puede fijar con: set {env_name}=<ruta>", file=sys.stderr)
    return None


def child_environment(nslink: Path, stem: str) -> dict:
    """El entorno de UN trabajo.

    `NSC_LLD_LINK` es la pieza entera de este pipeline -- ver la cabecera del archivo.

    `NSC_KEEP_LINK_TEMPS` es la peticion 001 de NSLink: hace que `nsc` NO borre el directorio
    temporal del enlace, donde viven `program.obj`, `runtime.obj` y las bibliotecas de
    importacion de ESA invocacion, y de paso conserva el fichero de respuesta. Se pone en TODOS
    los trabajos y no solo en los seis de la oferta, porque cuando un enlace falla lo que hace
    falta es precisamente el comando que fallo, y hasta que falla no se sabe cual sera.

    `NSC_OPT` se BORRA a proposito: no se llama a build_project.cmd, el nivel de optimizacion va
    en la linea de comandos. Si alguien lo tiene exportado en su sesion apuntando a otra cosa,
    aqui no manda.
    """
    env = dict(os.environ)
    env["NSC_LLD_LINK"] = str(nslink)
    env["NSC_KEEP_LINK_TEMPS"] = str(LINKTEMPS / stem)
    sdk = os.environ.get("NSLINK_SDK")
    if not sdk:
        guess = nslink.parent.parent / "sdk" / "win64"
        if guess.is_dir():
            sdk = str(guess)
    if sdk:
        env["NSLINK_SDK"] = sdk
    env.pop("NSC_OPT", None)
    return env


def run_job(project: str, label: str, expected: int | None, optimization: str,
            nsc: Path, nslink: Path) -> dict:
    stem = f"{label}_{optimization.lstrip('-')}"
    log_path = CASE_LOGS / f"{stem}.log"
    obj = BINDIR / f"{stem}.obj"
    exe = BINDIR / f"{stem}.exe"
    lines: list[str] = [
        f"--- {label} {optimization} ---",
        f"proyecto: {project}",
        f"compiler: {nsc}",
        f"linker  : {nslink}   (via NSC_LLD_LINK)",
        "",
    ]
    started = time.monotonic()

    def finish(ok: bool, detail: str, phase: str) -> dict:
        log_path.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")
        return {"label": label, "opt": optimization, "ok": ok, "detail": detail,
                "phase": phase, "log": log_path.name, "project": project,
                "obj": str(obj) if obj.is_file() else "",
                "seconds": round(time.monotonic() - started, 2)}

    project_path = ROOT / project
    if not project_path.is_file():
        lines.append(f"[ERROR] proyecto no encontrado: {project_path}")
        return finish(False, "proyecto no encontrado", "setup")

    # STDOUT Y STDERR. `nsc.exe` manda los diagnosticos por error estandar, y el mensaje del
    # ENLAZADOR viaja dentro de uno de ellos: LldLinker redirige la salida del enlazador a un
    # fichero temporal, la lee y la pega en su `error`. Quedarse solo con stdout tiraria justo
    # eso, que es lo unico que le sirve a quien desarrolla NSLink.
    def record(completed, header: str) -> None:
        lines.append(f"$ {header}")
        if completed.stdout:
            lines.append(completed.stdout.rstrip())
        if completed.stderr:
            lines.append(completed.stderr.rstrip())
        lines.append("")

    env = child_environment(nslink, stem)

    # LOS DOS COMANDOS QUE build_project.cmd EMITE, escritos aqui directamente. Es lo unico que
    # ese .cmd hace de verdad; el resto es localizar el compilador y re-sincronizar `lib\`, y
    # ninguna de las dos cosas se quiere aqui.
    steps = [
        ("obj", [str(nsc), "--emit-project-oop-obj", str(project_path), optimization, "-o", str(obj)]),
        ("exe", [str(nsc), "--emit-project-oop-exe", str(project_path), optimization, "-o", str(exe)]),
    ]
    for phase, command in steps:
        try:
            completed = subprocess.run(command, cwd=str(ROOT), env=env,
                                       capture_output=True, text=True, errors="replace",
                                       timeout=JOB_TIMEOUT_SECONDS)
        except subprocess.TimeoutExpired:
            lines.append(f"[TIMEOUT] la fase '{phase}' supero {JOB_TIMEOUT_SECONDS} s")
            return finish(False, f"timeout en {phase}", phase)
        record(completed, " ".join(command))
        if completed.returncode != 0:
            return finish(False, f"fallo al generar {phase} (exit {completed.returncode})", phase)

    if expected is None:
        lines.append("[exit -] compila y enlaza, no se ejecuta")
        return finish(True, "compila y enlaza, no se ejecuta", "link")

    try:
        run = subprocess.run([str(exe)], cwd=str(ROOT), capture_output=True, text=True,
                             errors="replace", timeout=JOB_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        lines.append(f"[TIMEOUT] la ejecucion supero {JOB_TIMEOUT_SECONDS} s")
        return finish(False, "timeout al ejecutar", "run")
    record(run, str(exe))
    lines.append(f"[exit {run.returncode}]")

    ok = run.returncode == expected
    detail = f"exit {run.returncode}" if ok else f"exit {run.returncode}, se esperaba {expected}"
    return finish(ok, detail, "run")


# ----------------------------------------------------------------------------------------------
# La comparacion. Es la mitad util del pipeline: "todo OK" no dice nada por si solo si no se sabe
# cuantos pasaban antes.
# ----------------------------------------------------------------------------------------------
def load_reference() -> tuple[dict[tuple[str, str], bool], str]:
    if not REFERENCE.is_file():
        return {}, ("SIN REFERENCIA. No existe _logs\\cases\\results.json, asi que no se puede\n"
                    "  decir si algo EMPEORO, solo si pasa o no. Corre `all.bat` (que enlaza con\n"
                    "  lld-link) y vuelve a lanzar esto para tener la comparacion.")
    try:
        data = json.loads(REFERENCE.read_text(encoding="utf-8", errors="replace"))
        table = {(r["label"], r["opt"]): bool(r["ok"]) for r in data.get("results", [])}
    except Exception as error:                                            # noqa: BLE001
        return {}, f"SIN REFERENCIA. _logs\\cases\\results.json no se pudo leer: {error}"

    note = f"Referencia: {len(table)} trabajos de _logs\\cases\\results.json (lld-link)."
    # Si el compilador es MAS NUEVO que la referencia, la comparacion puede estar mintiendo: esa
    # referencia se saco con otro nsc.exe. Se dice, no se calla.
    nsc_env = os.environ.get("NSC_EXE")
    nsc_path = Path(nsc_env) if nsc_env else Path(r"C:\native-sharp\bin\nsc.exe")
    try:
        if nsc_path.is_file() and nsc_path.stat().st_mtime > REFERENCE.stat().st_mtime:
            note += ("\n  AVISO: nsc.exe es MAS NUEVO que esa referencia, o sea que se saco con\n"
                     "  otro compilador. Una diferencia puede no ser culpa de NSLink. Corre\n"
                     "  `all.bat` otra vez antes de sacar conclusiones.")
    except OSError:
        pass
    return table, note


def classify(results: list[dict], reference: dict[tuple[str, str], bool]) -> dict:
    return {
        "regressions": [r for r in results
                        if not r["ok"] and reference.get((r["label"], r["opt"])) is True],
        "already_failing": [r for r in results
                            if not r["ok"] and reference.get((r["label"], r["opt"])) is False],
        "unknown": [r for r in results
                    if not r["ok"] and (r["label"], r["opt"]) not in reference],
        "recovered": [r for r in results
                      if r["ok"] and reference.get((r["label"], r["opt"])) is False],
    }


def report_lines(results: list[dict], reference: dict[tuple[str, str], bool], note: str,
                 groups: dict, elapsed: float, jobs: int, nsc: Path, nslink: Path,
                 header: list[str]) -> list[str]:
    passed = sum(1 for r in results if r["ok"])
    failed = len(results) - passed

    out: list[str] = list(header)
    out.append("=" * 78)
    out.append("1. VEREDICTO")
    out.append("=" * 78)
    out.append(f"compiler: {nsc}")
    out.append(f"linker  : {nslink}   (NSC_LLD_LINK)")
    out.append(f"trabajos: {len(results)}   ({jobs} en paralelo, {elapsed:.1f} s)")
    out.append(f"OK: {passed}   FAIL: {failed}")
    out.append("")
    out.append(note)
    out.append("")
    if groups["regressions"]:
        out.append(f"REGRESIONES: {len(groups['regressions'])} -- pasaban con lld-link y fallan "
                   f"con NSLink.")
        out.append("Esto es lo unico que este pipeline existe para encontrar.")
    elif reference:
        out.append("SIN REGRESIONES: NSLink no rompe ningun caso que lld-link enlazaba bien.")
    out.append("")

    def section(title: str, rows: list[dict], explain: str = "") -> None:
        out.append("=" * 78)
        out.append(title)
        out.append("=" * 78)
        if explain:
            out.append(explain)
            out.append("")
        if not rows:
            out.append("  (ninguno)")
        for r in rows:
            out.append(f"  [{r['phase']:>5}] {r['label']} {r['opt']} -- {r['detail']}")
            out.append(f"          proyecto: {r['project']}")
            out.append(f"          log     : fallos\\{r['log']}")
            if r.get("obj"):
                out.append(f"          objeto  : fallos\\{Path(r['obj']).name}")
        out.append("")

    section("2. REGRESIONES  (pasaba con lld-link, falla con NSLink)", groups["regressions"])
    section("3. FALLABA YA CON lld-link  (no es culpa de NSLink)", groups["already_failing"],
            "Se listan aparte a proposito: mezclarlos con las regresiones esconderia lo unico\n"
            "que hay que mirar.")
    section("4. FALLA Y NO ESTA EN LA REFERENCIA", groups["unknown"],
            "Casos que la corrida de referencia no tenia. Si son varios, lo mas probable es que\n"
            "las tablas de casos de los dos scripts se hayan separado.")
    if groups["recovered"]:
        section("5. PASA CON NSLink Y FALLABA CON lld-link", groups["recovered"],
                "Inesperado, y por eso se reporta en vez de callarlo.")

    out.append("=" * 78)
    out.append("6. TODOS LOS CASOS, en el orden de la tabla")
    out.append("=" * 78)
    for r in results:
        mark = "[ OK ]" if r["ok"] else "[FAIL]"
        was = reference.get((r["label"], r["opt"]))
        tag = "" if was is None or was == r["ok"] else "   <-- CAMBIA"
        out.append(f"  {mark} {r['label']} {r['opt']} -- {r['detail']}{tag}")
    out.append("")

    if reference and len(reference) != len(results):
        out.append("=" * 78)
        out.append("AVISO: los conteos no cuadran")
        out.append("=" * 78)
        out.append(f"  referencia: {len(reference)} trabajos / aqui: {len(results)}")
        out.append("  Las dos pasadas leen la MISMA tabla (cases_table.py), asi que esto ya no")
        out.append("  puede ser una fixture registrada en un script y no en el otro. Mira si una")
        out.append("  de las dos pasadas se quedo a medias o si results.json es de otra corrida.")
        out.append("")
    return out


# ----------------------------------------------------------------------------------------------
# EL BUZON. Lo que se le deja a quien desarrolla NSLink.
# ----------------------------------------------------------------------------------------------
def link_inputs(stem: str) -> dict:
    """Las entradas REALES del enlace de un caso -- peticion 001 de NSLink.

    `nsc` crea `<raiz>/nativesharp-stage20-<sello>-<n>/` y mete ahi `program.obj`, `runtime.obj` y
    las bibliotecas de importacion de ESA invocacion. Con NSC_KEEP_LINK_TEMPS puesta ya no lo
    borra, asi que aqui solo hay que encontrarlo.

    Se coge el MAS RECIENTE si hay varios: un caso puede enlazar mas de una vez (bibliotecas antes
    del ejecutable), y el que interesa es el ultimo, el del `.exe`.
    """
    # Dos sitios posibles, y ninguno se adivina: el modo --run deja sus temporales en
    # `_logs_nslink\linktemps` (los crea el), y la pasada de all.bat los deja dentro del snapshot
    # de esa pasada, en `_logs_nslink\linktemps` tambien -- porque all.bat le dijo a `nsc` que los
    # pusiera en `_logs\linktemps` y despues movio `_logs` entero a `_logs_nslink`. Coinciden a
    # proposito: un solo sitio que mirar.
    root = LINKTEMPS / stem
    found: dict = {"program": None, "runtime": None, "libs": [], "rsp": None, "dir": None}
    if root.is_dir():
        candidates = [d for d in root.iterdir() if d.is_dir()]
        if candidates:
            newest = max(candidates, key=lambda d: d.stat().st_mtime)
            found["dir"] = newest
            program = newest / "program.obj"
            runtime = newest / "runtime.obj"
            if program.is_file(): found["program"] = program
            if runtime.is_file(): found["runtime"] = runtime
            found["libs"] = sorted(newest.glob("*.lib"))
    # El fichero de respuesta del EJECUTABLE no vive en el temporal: `nsc` lo escribe junto a la
    # salida, como `<salida>.link.rsp`. Es la invocacion exacta, que es lo que el handover decia
    # que no se podia dar -- se podia, solo habia que no borrarla.
    # El .rsp del ejecutable queda junto a la salida. En el modo --run eso es `_logs_nslink\bin`;
    # en la pasada de all.bat es `_logs\bin`, que el move convierte en `_logs_nslink\bin`. Otra
    # vez el mismo sitio.
    rsp = BINDIR / f"{stem}.exe.link.rsp"
    if rsp.is_file():
        found["rsp"] = rsp
    return found


def copy_link_inputs(stem: str, destination: Path) -> list[str]:
    """Copia las entradas del enlace de un caso al buzon. Devuelve los nombres copiados."""
    found = link_inputs(stem)
    written: list[str] = []
    if found["runtime"] is not None:
        target = destination / f"{stem}.runtime.obj"
        shutil.copy2(found["runtime"], target)
        written.append(target.name)
    if found["program"] is not None:
        target = destination / f"{stem}.program.obj"
        shutil.copy2(found["program"], target)
        written.append(target.name)
    if found["rsp"] is not None:
        target = destination / f"{stem}.link.rsp"
        shutil.copy2(found["rsp"], target)
        written.append(target.name)
    if found["libs"]:
        libdir = destination / f"{stem}.libs"
        libdir.mkdir(parents=True, exist_ok=True)
        names = []
        for lib in found["libs"]:
            shutil.copy2(lib, libdir / lib.name)
            names.append(lib.name)
        # La lista que la peticion 001 pedia por nombre, VOLCADA de la invocacion real -- no
        # reconstruida. Al lado van las bibliotecas mismas, que es mas de lo que se pidio.
        (destination / f"{stem}.libs.txt").write_text(
            "\n".join(names) + "\n", encoding="ascii", errors="replace")
        written.append(f"{stem}.libs.txt")
        written.append(f"{stem}.libs\\ ({len(names)} bibliotecas)")
    return written



def inbox_readme(nsc: Path, nslink: Path, groups: dict, copied: list[str],
                 skipped: list[str], offered: list[tuple[str, str, bool]]) -> str:
    failures = (groups["regressions"] + groups["already_failing"] + groups["unknown"])
    lines = [
        "# Buzon de NativeSharp para NSLink",
        "",
        "Lo escribe `nslink_pipeline.py`, que vive en el arbol de **nsc** (`A:\\nsc-devel`). Es un",
        "directorio de ENTRADA: **no forma parte de la construccion ni de las pruebas de NSLink**,",
        "nadie de aqui lo lee, y se puede borrar entero sin consecuencias.",
        "",
        "## Que es esto",
        "",
        "El compilador `nsc` compilo y enlazo su bateria completa de fixtures usando **NSLink en",
        "lugar de lld-link**, via `NSC_LLD_LINK`. Aqui esta el material de cada caso que fallo.",
        "",
        f"- compilador: `{nsc}`",
        f"- enlazador : `{nslink}`",
        f"- fallos    : **{len(failures)}**"
        f"  (regresiones: {len(groups['regressions'])},"
        f" ya fallaban con lld-link: {len(groups['already_failing'])},"
        f" sin referencia: {len(groups['unknown'])})",
        "",
        "`REPORTE.txt` tiene el detalle. La distincion importa: un caso que falla con LOS DOS",
        "enlazadores no es culpa de NSLink.",
        "",
        "## Que hay en `fallos\\`",
        "",
        "Por cada caso que fallo, dos ficheros con el mismo nombre base:",
        "",
        "| Fichero | Que es |",
        "|---|---|",
        "| `<caso>.log` | La salida COMPLETA de `nsc`, stdout y stderr. **El mensaje del propio",
        "NSLink esta aqui dentro**: `LldLinker` redirige la salida del enlazador a un fichero",
        "temporal, la lee y la pega en su error, que sale por el stderr de `nsc`. |",
        "| `<caso>.obj` | El COFF que NSLink no supo enlazar, emitido por LLVM a traves de `nsc`. |",
        "",
        "## Por que el `.obj` vale mas que el log",
        "",
        "El `corpus/README.md` de este mismo arbol dice lo que le falta al corpus:",
        "",
        "> **LLVM / clang-cl**: no disponible en el entorno donde se construyo esta etapa. Su",
        "> ausencia importa mas de lo que parece: LLVM emite COMDAT, `.drectve` y simbolos",
        "> debiles, ninguno de los cuales aparece en los objetos de NSASM de este directorio.",
        "",
        "Estos objetos son exactamente eso. Con ellos el fallo se reproduce con `nslink` a secas,",
        "sin `nsc`, sin el pipeline y sin la maquina donde se genero.",
        "",
        "**Si alguno resulta util como caso permanente, promoverlo a `corpus/llvm/` es decision",
        "de quien mantiene NSLink, no de quien deja este buzon.** Por eso no se escribe ahi.",
        "",
        "## Como reproducir un fallo",
        "",
        "```bat",
        "nslink --link fallos\\<caso>.obj -o prueba.exe ^",
        "       --entry __nativesharp_start",
        "```",
        "",
        "Ojo: eso es el objeto del PROGRAMA. El enlace real que hace `nsc` incluye ademas un",
        "objeto de RUNTIME que genera al vuelo en un directorio temporal, y las bibliotecas de",
        "importacion. El `.log` de cada caso trae la linea de error completa tal como salio.",
        "",
        "## La linea de comandos que `nsc` le pasa",
        "",
        "El fichero de respuesta (`<salida>.link.rsp`) **no se puede adjuntar**: `nsc` lo escribe,",
        "lo usa y lo borra en el mismo paso (`src/Linker/LldLinker.cpp`, `remove(responsePath)`).",
        "No se reconstruye a mano porque la lista de bibliotecas de importacion se genera en",
        "tiempo de enlace, y adivinarla seria dar por medido algo que no se midio.",
        "",
        "Lo que si es fijo, copiado de `linkConsoleExecutable` en ese mismo archivo:",
        "",
        "```",
    ]
    lines.extend(EXECUTABLE_RESPONSE_ARGUMENTS)
    lines.extend([
        "```",
        "",
        "Se invoca como `nslink @<fichero-de-respuesta>`, con stdin en NUL y stdout+stderr",
        "redirigidos a un fichero que `nsc` lee despues.",
        "",
        "## Los dos casos que mas dicen",
        "",
        "- `c1_14_callback_exception_boundary` -- una excepcion cruzando un callback de C. Depende",
        "  de `.pdata`/`.xdata`, o sea del `PeExceptionTable`. Sale con **141** a proposito.",
        "- `c1_13_two_libraries_same_symbol` -- dos bibliotecas que definen el mismo simbolo. Es",
        "  resolucion de simbolos pura; se certifica **enlazando**, no ejecutando.",
        "",
    ])
    lines.extend([
        "## `corpus-oferta\\` -- material para trabajar en paralelo, falle o no",
        "",
        "Estos objetos se copian **siempre**, incluso cuando NSLink no rompe nada. Si el buzon",
        "solo tuviera fallos, una corrida verde dejaria a quien desarrolla NSLink sin nada que",
        "hacer -- y su `corpus/README.md` dice que lo que le falta son justamente objetos de LLVM.",
        "",
        "| Objeto | Que ejercita | Presente |",
        "|---|---|:--:|",
    ])
    for stem, why, present in offered:
        lines.append(f"| `{stem}.obj` | {why} | {'si' if present else 'NO'} |")
    lines.extend([
        "",
        'Presente: NO quiere decir que ese caso no llego a generar objeto en esta corrida --',
        "el fallo fue antes, al compilar. Su log esta en `fallos\\\\`.",
        "",
        "### NSL-ACT-001 -- ATENDIDA: el objeto de runtime, emparejado con su programa",
        "",
        "Junto a cada `.obj` de arriba van ahora los DEMAS artefactos de **la misma invocacion**:",
        "",
        "| Fichero | Que es |",
        "|---|---|",
        "| `<caso>.runtime.obj` | El runtime que se enlazo con ESE programa. Se construye con",
        "`build(mainSymbol, ...)`, asi que emparejarlo con otro caso no tiene por que enlazar --",
        "por eso se conserva el directorio entero y no un fichero suelto. |",
        "| `<caso>.program.obj` | El objeto del programa **tal como entro al enlazador**. El",
        "`<caso>.obj` de al lado sale de `--emit-project-oop-obj`; deberian coincidir, y se dan los",
        "dos para que no haya que suponerlo. |",
        "| `<caso>.link.rsp` | **La invocacion exacta.** El handover decia que esto no se podia",
        "adjuntar porque `nsc` lo borra. Se podia: solo habia que no borrarlo. |",
        "| `<caso>.libs.txt` | La lista de bibliotecas de importacion de esa invocacion, VOLCADA,",
        "no reconstruida. |",
        "| `<caso>.libs\\\\` | Y las bibliotecas mismas, que es mas de lo que se pidio. |",
        "",
        "Como se hizo: `nsc` gana una variable de entorno, `NSC_KEEP_LINK_TEMPS`, que le dice donde",
        "dejar el directorio temporal del enlace y que no lo borre. **Apagada por omision** -- sin",
        "ella el comportamiento es el de siempre. Si, hubo que tocar `nsc`; la peticion decia que se",
        "avisara si pasaba, y esto es el aviso.",
        "",
        "Para los FALLOS va lo mismo, en `fallos\\\\`. Ahi el `.rsp` es lo que mas vale: es el comando",
        "que fallo.",
        "",
        "Lo que sigue sin ir: el `.exe` parcial. La peticion decia que no hacia falta.",
        "",
        "Promoverlos a `corpus/llvm/` es decision de quien mantiene NSLink. Aqui solo se ofrecen.",
        "",
    ])
    if skipped:
        lines.extend([
            "## Lo que NO se copio",
            "",
            f"{len(skipped)} objeto(s) se quedaron fuera por el tope de tamano del buzon",
            f"({INBOX_MAX_BYTES // (1024 * 1024)} MB). Se dice en vez de llenar el arbol de otro",
            "en silencio; sus logs si estan.",
            "",
        ])
        for name in skipped:
            lines.append(f"- `{name}`")
        lines.append("")
    lines.extend([
        "---",
        "",
        f"Ficheros copiados en esta corrida: {len(copied)}.",
        "Cada corrida REEMPLAZA `fallos\\` entero, para que nunca queden fallos de ayer",
        "mezclados con los de hoy.",
    ])
    return "\n".join(lines) + "\n"


def write_inbox(inbox: Path, results: list[dict], reference: dict[tuple[str, str], bool],
                note: str, groups: dict, elapsed: float, jobs: int,
                nsc: Path, nslink: Path) -> tuple[int, list[str]]:
    """Deja el material de los fallos en el arbol de NSLink. Devuelve (copiados, omitidos)."""
    failures = groups["regressions"] + groups["already_failing"] + groups["unknown"]
    fallos = inbox / "fallos"

    # Se REEMPLAZA `fallos\` entero: un buzon donde se acumulan los fallos de tres corridas es
    # peor que no tener buzon -- nadie sabe cual sigue vivo.
    if fallos.exists():
        shutil.rmtree(fallos, ignore_errors=True)
    fallos.mkdir(parents=True, exist_ok=True)

    # LA OFERTA DE CORPUS, siempre, fallen o no. Ver CORPUS_OFFERING arriba: si NSLink no rompe
    # nada, un buzon vacio deja a quien lo desarrolla sin material, y su propio corpus/README.md
    # dice que le faltan objetos de LLVM.
    oferta = inbox / "corpus-oferta"
    if oferta.exists():
        shutil.rmtree(oferta, ignore_errors=True)
    oferta.mkdir(parents=True, exist_ok=True)
    offered: list[tuple[str, str, bool]] = []
    for stem, why in CORPUS_OFFERING:
        source = BINDIR / f"{stem}.obj"
        present = source.is_file()
        if present:
            shutil.copy2(source, oferta / source.name)
        # NSL-ACT-001: el runtime, el program.obj REAL del enlace, el fichero de respuesta y las
        # bibliotecas de importacion -- los tres artefactos de UNA MISMA invocacion, que es lo que
        # la peticion subrayaba.
        copy_link_inputs(stem, oferta)
        offered.append((stem, why, present))

    copied: list[str] = []
    skipped: list[str] = []
    budget = INBOX_MAX_BYTES
    for r in failures:
        source_log = CASE_LOGS / r["log"]
        if source_log.is_file():
            shutil.copy2(source_log, fallos / source_log.name)
            copied.append(source_log.name)
        obj = Path(r["obj"]) if r.get("obj") else None
        if obj is not None and obj.is_file():
            size = obj.stat().st_size
            if size <= budget:
                shutil.copy2(obj, fallos / obj.name)
                copied.append(obj.name)
                budget -= size
            else:
                skipped.append(obj.name)
        # Y para un FALLO, lo que de verdad hace falta es el comando que fallo. Va aqui y no solo
        # en la oferta porque hasta que un caso falla no se sabe cual lo va a necesitar.
        stem = f"{r['label']}_{r['opt'].lstrip('-')}"
        copied.extend(copy_link_inputs(stem, fallos))

    header = [
        "NativeSharp -> NSLink -- corrida de la bateria de nsc enlazando con NSLink",
        "Generado por nslink_pipeline.py (arbol de nsc). Buzon de entrada: no forma parte de la",
        "construccion ni de las pruebas de NSLink, y se puede borrar entero.",
        "",
    ]
    (inbox / "REPORTE.txt").write_text(
        "\n".join(report_lines(results, reference, note, groups, elapsed, jobs,
                               nsc, nslink, header)) + "\n",
        encoding="utf-8", errors="replace")
    (inbox / "README.md").write_text(
        inbox_readme(nsc, nslink, groups, copied, skipped, offered),
        encoding="utf-8", errors="replace")
    return len(copied), skipped


def ctest_summary(directory: Path) -> dict:
    """Lo que ctest dijo en una pasada. Sale de `02_ctest.log`, que es la fuente, no del REPORTE.

    POR QUE HACE FALTA. La comparacion miraba SOLO `cases\results.json`, o sea los trabajos del corpus.
    El 2026-09-02 eso habria dicho "0 regresiones" con CINCO pruebas de ctest en rojo por culpa
    de NSLink -- todas las de biblioteca de importacion -- porque ninguno de los trabajos del corpus ejercita esa
    superficie. Un buzon que dice que todo esta bien justo cuando no lo esta es peor que no tener
    buzon.

    Devuelve {"total", "failed", "names", "leido"}. `leido` en falso significa que no habia log,
    no que la pasada fuera verde: la diferencia importa y se reporta.
    """
    log = directory / "02_ctest.log"
    summary = {"total": 0, "failed": 0, "names": [], "leido": False}
    if not log.is_file():
        return summary
    text = log.read_text(encoding="utf-8", errors="replace")
    summary["leido"] = True

    # "98% tests passed, 5 tests failed out of 243" / "100% tests passed out of 243"
    counts = re.search(r"tests passed(?:, (\d+) tests? failed)? out of (\d+)", text)
    if counts:
        summary["failed"] = int(counts.group(1) or 0)
        summary["total"] = int(counts.group(2))

    # "The following tests FAILED:" y debajo una linea por prueba: "\t 60 - nombre (Failed)"
    tail = text.split("The following tests FAILED:", 1)
    if len(tail) == 2:
        for line in tail[1].splitlines():
            named = re.match(r"\s*\d+\s*-\s*(\S+)\s*\(", line)
            if named:
                summary["names"].append(named.group(1))
    return summary


def find_build_directory() -> Path | None:
    """El arbol de build de donde sale ctest. NO se construye nada: solo se busca uno usable.

    Desde el 2026-09-02 hay UNO SOLO: all.bat construye el compilador una vez y las dos pasadas
    lo comparten. Los dos nombres viejos se siguen aceptando por si queda un arbol de antes.
    """
    for name in ("build-clang", "build-clang-lldlink", "build-clang-nslink"):
        candidate = ROOT / name
        if (candidate / "CTestTestfile.cmake").is_file():
            return candidate
    return None


def reverify_ctest_serially(names: list[str], nslink: Path) -> tuple[set, str]:
    """Vuelve a correr EN SERIE las pruebas que fallaron en paralelo.

    POR QUE EXISTE ESTE PASO. Una prueba que falla con `-j 16` y pasa sola no es un defecto del
    enlazador: es contencion entre procesos. Se demostro por construccion el 2026-09-02 con
    `nsc.stage48_5.csharp-pinvoke-host`, que lanza un `dotnet build` -- la cache global de NuGet y
    los nodos de MSBuild son recursos compartidos entre procesos, y dos invocaciones a la vez se
    estorban. Sola pasa; con `-j 16` falla.

    Sin este paso, esa prueba viaja al buzon de NSLink etiquetada como REGRESION y le hace perder
    el tiempo buscando un fallo que no es suyo. Una regresion que solo se reproduce en paralelo es
    un flake mientras no se demuestre lo contrario, y demostrarlo cuesta unos segundos.

    NSC_LLD_LINK SE FIJA AQUI, y es la parte que se puede hacer mal sin notarlo: `all.bat` limpia
    esa variable antes de llamar a la comparacion, asi que heredar el entorno significaria volver
    a correr las pruebas con lld-link -- pasarian todas y TODO quedaria marcado como contencion.
    El enlazador se pasa por argumento y se pone explicitamente.

    Devuelve (las que TAMBIEN fallan solas, un motivo si no se pudo comprobar).
    """
    if not names:
        return set(), ""
    build = find_build_directory()
    if build is None:
        return set(names), "no se encontro arbol de build: no se pudo comprobar en serie"

    environment = dict(os.environ)
    environment["NSC_LLD_LINK"] = str(nslink)
    environment.pop("NSC_KEEP_LINK_TEMPS", None)

    # ctest -R toma una expresion regular; se anclan los nombres para que `stage48_4` no arrastre
    # a `stage48_4x` si algun dia existe.
    pattern = "^(" + "|".join(re.escape(name) for name in names) + ")$"
    log = OUT / "02_ctest_serie.log"
    try:
        completed = subprocess.run(
            ["ctest", "--test-dir", str(build), "-R", pattern, "-j", "1", "--output-on-failure"],
            capture_output=True, text=True, env=environment, timeout=1800)
    except (OSError, subprocess.SubprocessError) as error:
        return set(names), f"no se pudo relanzar ctest en serie: {error}"

    output = (completed.stdout or "") + (completed.stderr or "")
    log.write_text(output, encoding="utf-8", errors="replace")

    still_failing = set()
    tail = output.split("The following tests FAILED:", 1)
    if len(tail) == 2:
        for line in tail[1].splitlines():
            named = re.match(r"\s*\d+\s*-\s*(\S+)\s*\(", line)
            if named:
                still_failing.add(named.group(1))
    return still_failing, ""


def ctest_lines(reference: dict, measured: dict) -> list[str]:
    """La seccion de ctest del reporte de comparacion."""
    out = ["", "=" * 78, "CTEST -- LAS PRUEBAS UNITARIAS, QUE TAMBIEN ENLAZAN", "=" * 78]
    if not measured["leido"]:
        out.append("  No hay 02_ctest.log en la pasada con NSLink. No se puede decir nada.")
        return out

    out.append(f"  NSLink   : {measured['total'] - measured['failed']}/{measured['total']} "
               f"({measured['failed']} en rojo)")
    if reference["leido"]:
        out.append(f"  lld-link : {reference['total'] - reference['failed']}/{reference['total']} "
                   f"({reference['failed']} en rojo)")
    else:
        out.append("  lld-link : sin 02_ctest.log -- no hay con que comparar.")

    broken = [n for n in measured["names"] if n not in reference["names"]]
    shared = [n for n in measured["names"] if n in reference["names"]]

    if not measured["names"]:
        out.append("")
        out.append("  Ninguna prueba en rojo con NSLink.")
        return out

    if broken:
        confirmed = [n for n in broken if n in measured.get("confirmadas", set(broken))]
        flaky = [n for n in broken if n not in confirmed]

        if confirmed:
            out.append("")
            out.append("  REGRESIONES DE CTEST -- pasan con lld-link, fallan con NSLink, Y FALLAN")
            out.append("  TAMBIEN AL CORRERLAS SOLAS:")
            for name in confirmed:
                out.append(f"    {name}")
            out.append("")
            out.append("  El detalle esta en 02_ctest.log; la corrida en serie que lo confirma, en")
            out.append("  02_ctest_serie.log. Los dos van al buzon. Son las que hay que mirar.")
        if flaky:
            out.append("")
            out.append("  SOLO FALLAN EN PARALELO -- NO son del enlazador:")
            for name in flaky:
                out.append(f"    {name}")
            out.append("")
            out.append("  Fallan con -j y pasan al correrlas solas, asi que es contencion entre")
            out.append("  procesos por algun recurso compartido -- la cache de NuGet y los nodos de")
            out.append("  MSBuild son los sospechosos conocidos. Es un defecto de NUESTRA suite, no")
            out.append("  de NSLink, y no cuenta como regresion.")
        if measured.get("motivo_sin_confirmar"):
            out.append("")
            out.append(f"  AVISO: {measured['motivo_sin_confirmar']}.")
            out.append("  Las de arriba se dan por confirmadas sin haberlo comprobado.")
    if shared:
        out.append("")
        out.append("  Ya fallaban con lld-link (NO es culpa de NSLink):")
        for name in shared:
            out.append(f"    {name}")
    return out


def load_pass(directory: Path) -> list[dict]:
    """Los resultados de una pasada de all.bat. Vacio si no llego a escribirlos."""
    # Dos sitios, y los dos son legitimos: `run_cases.py` (la corrida completa) escribe en
    # `cases\results.json`, y el modo suelto de este script en `results.json`. El ciclo corto usa
    # el segundo, asi que la comparacion tiene que saber leer los dos o no compararia nada.
    path = directory / "cases" / "results.json"
    if not path.is_file():
        path = directory / "results.json"
    if not path.is_file():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8", errors="replace"))
    except Exception:                                                     # noqa: BLE001
        return []
    return list(data.get("results", []))


def copy_pass_logs(destination: Path) -> tuple[int, str]:
    """Copia la pasada con NSLink entera al arbol de NSLink. Devuelve (ficheros, error)."""
    if not PASS_NSLINK.is_dir():
        return 0, f"no existe {PASS_NSLINK.name}"
    try:
        if destination.exists():
            shutil.rmtree(destination, ignore_errors=True)
        destination.mkdir(parents=True, exist_ok=True)
        copied = 0
        for item in sorted(PASS_NSLINK.iterdir()):
            if item.name in LOGS_SKIP:
                continue
            if item.is_dir():
                shutil.copytree(item, destination / item.name, dirs_exist_ok=True)
                copied += sum(1 for _ in (destination / item.name).rglob("*") if _.is_file())
            else:
                shutil.copy2(item, destination / item.name)
                copied += 1
        (destination / "LEEME.txt").write_text(
            "Los logs COMPLETOS de la pasada de nsc que enlaza con NSLink.\n"
            "\n"
            "Los deja A:\\nsc-devel\\all.bat en cada corrida, dentro del buzon y al lado de\n"
            "`fallos\\`, y SE REEMPLAZAN ENTEROS: lo que hay aqui es de la ultima corrida y de\n"
            "ninguna otra. Si necesitas conservar uno, copialo fuera.\n"
            "\n"
            "  REPORTE.txt          el veredicto de ESTA pasada, autocontenido\n"
            "  SUMMARY.txt          las lineas [ OK ] / [FAIL] de cada paso\n"
            "  01_build.log         construccion del compilador\n"
            "  02_ctest.log         las pruebas unitarias\n"
            "  03_fixtures.log      los trabajos del corpus\n"
            "  04_examples.log      los ejemplos empaquetados\n"
            "  05_triage_run.log    la regresion de bugs reportados\n"
            "  06_manifest.log      el manifiesto\n"
            "  10_triage.log        el detalle del triage\n"
            "  TRIAGE.txt           su resumen\n"
            "  cases\\<caso>.log     la salida COMPLETA de nsc, un fichero por trabajo\n"
            "\n"
            "NO se copian bin\\ ni linktemps\\: son cientos de megas por corrida. Lo que hace falta\n"
            "para reproducir un fallo -- el .obj, el runtime, las .lib y el fichero de respuesta --\n"
            "va elegido caso por caso a _inbox_nsc\\.\n"
            "\n"
            "Esto NO forma parte de tu construccion ni de tus pruebas, y se puede borrar entero.\n",
            encoding="ascii")
        return copied, ""
    except OSError as error:
        return 0, str(error)


def run_compare(inbox: Path | None, nsc: Path, nslink: Path) -> int:
    """NO COMPILA NADA. Compara las dos pasadas que `all.bat` acaba de hacer.

    POR QUE NO COMPILA. Las dos corridas ya existen: `all.bat` ejecuta el pipeline entero una vez
    por enlazador, cada una en su arbol y su directorio. Volver a compilar aqui mediria una
    TERCERA cosa y reportaria esa, no las que se acaban de medir.
    """
    reference_rows = load_pass(PASS_LLD)
    results = load_pass(PASS_NSLINK)

    # Los trabajos del corpus pueden NO estar, y no es un error: `nslink_solo.bat ctest` corre solo ctest,
    # que es donde viven los fallos del enlazador. Antes esto devolvia 2 y se saltaba la
    # comparacion de ctest -- o sea justo lo unico que ese modo habia medido.
    solo_ctest = not results
    if solo_ctest and not (PASS_NSLINK / "02_ctest.log").is_file():
        print(f"[NSLINK] {PASS_NSLINK.name}\\ no tiene ni resultados de los trabajos del corpus ni "
              "02_ctest.log: no hay nada que comparar.", file=sys.stderr)
        return 2

    reference = {(r["label"], r["opt"]): bool(r["ok"]) for r in reference_rows}
    if reference:
        note = (f"Referencia: {len(reference)} trabajos de {PASS_LLD.name}\\cases\\results.json "
                "(lld-link).")
    else:
        note = (f"SIN REFERENCIA. No existe {PASS_LLD.name}\\cases\\results.json, asi que no se\n"
                "  puede decir si algo EMPEORO, solo si pasa o no.")

    # run_cases.py no guarda el .obj en su results.json -- no lo necesita. Aqui si: el buzon copia
    # el COFF que NSLink no supo enlazar, y sin el, el fallo no es reproducible sin esta maquina.
    # Se busca por el mismo sello con el que run_cases.py nombra todo: <etiqueta>_<O0|O2>.
    enriched: list[dict] = []
    for row in results:
        stem = f"{row['label']}_{row['opt'].lstrip('-')}"
        inputs = link_inputs(stem)
        program = inputs.get("program")
        enriched.append({**row, "obj": str(program) if program else "",
                         "project": row.get("project", ""),
                         "phase": row.get("phase", "run")})

    groups = classify(enriched, reference)
    header = [
        "COMPARACION DE LAS DOS PASADAS DE all.bat",
        "",
        f"  PASADA 1  lld-link   {PASS_LLD.name}\\      {len(reference_rows)} trabajos",
        f"  PASADA 2  NSLink     {PASS_NSLINK.name}\\   {len(results)} trabajos",
        "",
        "Este fichero NO compila nada: lee lo que las dos pasadas ya midieron.",
        "",
    ]
    if solo_ctest:
        lines = list(header)
        lines.append("=" * 78)
        lines.append("LOS 84 TRABAJOS -- NO MEDIDOS EN ESTE CICLO")
        lines.append("=" * 78)
        lines.append("  Este ciclo corrio solo ctest (`nslink_solo.bat ctest`). Los trabajos del corpus no")
        lines.append("  se midieron, asi que de ellos no se dice nada -- ni bien ni mal.")
    else:
        lines = report_lines(enriched, reference, note, groups, 0.0, 0, nsc, nslink, header)

    # ctest APARTE de los trabajos del corpus, y es la mitad que faltaba. Las dos superficies fallan por
    # cosas distintas: los trabajos del corpus ejecutan un programa enlazado; ctest ademas construye DLLs,
    # bibliotecas de importacion y enlaza anfitriones de C y C++ contra ellas. El 2026-09-02 los
    # los trabajos salieron todos en verde con los dos enlazadores y ctest 243 vs 238.
    ctest_reference = ctest_summary(PASS_LLD)
    ctest_measured = ctest_summary(PASS_NSLINK)

    # Antes de llamar regresion a nada: las que fallaron en paralelo se vuelven a correr SOLAS.
    # Ver reverify_ctest_serially -- una que pasa sola es contencion de nuestra suite, y mandarla
    # al buzon como regresion le hace perder el tiempo a quien mantiene el enlazador.
    candidates = [n for n in ctest_measured["names"] if n not in ctest_reference["names"]]
    if candidates:
        print(f"[NSLINK] {len(candidates)} pruebas rojas: se relanzan EN SERIE para confirmar ...")
    confirmed, reason = reverify_ctest_serially(candidates, nslink)
    ctest_measured["confirmadas"] = confirmed
    ctest_measured["motivo_sin_confirmar"] = reason
    lines.extend(ctest_lines(ctest_reference, ctest_measured))

    comparison = PASS_NSLINK / "COMPARACION_CON_LLD.txt"
    comparison.write_text("\n".join(lines) + "\n", encoding="ascii", errors="replace")
    print("\n".join(lines))
    print(f"[NSLINK] comparacion en {comparison}")

    if inbox is not None and not solo_ctest:
        copied, skipped = write_inbox(inbox, enriched, reference, note, groups,
                                      0.0, 0, nsc, nslink)
        print(f"[NSLINK] buzon: {inbox}  ({copied} ficheros copiados, {len(skipped)} omitidos)")

    if inbox is not None:
        # Los logs de la pasada con NSLink, enteros, dentro del buzon. Esto SI va tambien en el
        # ciclo de solo-ctest: es donde esta 02_ctest.log, que es lo que se acaba de medir.
        destination = inbox / LOGS_NAME
        count, error = copy_pass_logs(destination)
        if error:
            print(f"[NSLINK] aviso: no se pudieron copiar los logs a {destination}: {error}",
                  file=sys.stderr)
        else:
            print(f"[NSLINK] logs : {destination}  ({count} ficheros)")

    # El codigo de salida cuenta LAS DOS superficies. Si solo contara los trabajos del corpus, `all.bat` habria
    # escrito "generado" en verde con cinco pruebas de ctest rojas por culpa del enlazador.
    # SOLO las confirmadas en serie. Una que solo falla en paralelo no puede poner en rojo el
    # ciclo del enlazador: no es suya.
    ctest_broken = [n for n in candidates if n in confirmed]
    ctest_flaky = [n for n in candidates if n not in confirmed]
    if solo_ctest:
        regressions = 0
        groups = {"regressions": []}
    if ctest_flaky:
        print(f"[NSLINK] {len(ctest_flaky)} solo fallan en paralelo (contencion, no del "
              f"enlazador): {', '.join(ctest_flaky)}")
    regressions = len(groups["regressions"])
    if regressions:
        print(f"[NSLINK] {regressions} REGRESIONES en los trabajos del corpus.")
    if ctest_broken:
        print(f"[NSLINK] {len(ctest_broken)} REGRESIONES en ctest: {', '.join(ctest_broken)}")
    if not regressions and not ctest_broken:
        print("[NSLINK] sin regresiones en ninguna de las dos superficies.")
    return 1 if (regressions or ctest_broken) else 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Corre las fixtures enlazando con NSLink y deja los fallos en su arbol.")
    parser.add_argument("-j", "--jobs", type=int, default=0,
                        help="trabajos en paralelo (por omision: los nucleos de la maquina)")
    # Se aceptan las dos escrituras, "O0" y "-O0", y no es capricho: `--opt -O0` hace que argparse
    # lea `-O0` como una OPCION y no como el valor, y la corrida muere con un error de linea de
    # comandos que no dice eso. Con la forma sin guion el caso comun funciona sin trucos.
    parser.add_argument("--opt", action="append", choices=["O0", "O2", "-O0", "-O2"],
                        metavar="NIVEL",
                        help="limitar a un nivel: O0 u O2 (se puede repetir)")
    parser.add_argument("--filter", default="",
                        help="solo los casos cuya etiqueta contenga este texto")
    parser.add_argument("--inbox", default="",
                        help="arbol de NSLink donde dejar los fallos "
                             r"(por omision NSLINK_DEVEL o A:\nslink-devel)")
    parser.add_argument("--no-inbox", action="store_true",
                        help="no escribir nada fuera de este repo")
    parser.add_argument("--dry-run", action="store_true",
                        help="lista los trabajos y sale, sin compilar nada")
    parser.add_argument("--compare", action="store_true",
                        help="NO compila: compara las dos pasadas que all.bat ya hizo, "
                             "llena el buzon y copia los logs al arbol de NSLink")
    arguments = parser.parse_args()

    nsc = locate("nsc.exe", "NSC_EXE", Path(r"C:\native-sharp\bin\nsc.exe"))
    nslink = locate("nslink.exe", "NSLINK_EXE", Path(r"C:\native-sharp\bin\nslink.exe"))
    if nsc is None or nslink is None:
        return 2

    inbox: Path | None = None
    if not arguments.no_inbox:
        devel = Path(arguments.inbox or os.environ.get("NSLINK_DEVEL")
                     or DEFAULT_NSLINK_DEVEL)
        if devel.is_dir():
            inbox = devel / INBOX_NAME
        else:
            print(f"[NSLINK] aviso: no existe {devel}, no se dejara buzon. "
                  "Se fija con --inbox o NSLINK_DEVEL.", file=sys.stderr)

    if arguments.compare:
        return run_compare(inbox, nsc, nslink)

    levels = (tuple(level if level.startswith("-") else "-" + level for level in arguments.opt)
              if arguments.opt else OPT_LEVELS)
    work = [(project, label, expected, optimization)
            for (project, label, expected) in CASES
            for optimization in levels
            if arguments.filter in label]

    if not work:
        print(f"[ERROR] ningun caso coincide con --filter {arguments.filter!r}", file=sys.stderr)
        return 2

    if arguments.dry_run:
        for _, label, expected, optimization in work:
            print(f"{label} {optimization} -> "
                  f"{expected if expected is not None else 'build-only'}")
        print(f"{len(work)} trabajos")
        print(f"compiler: {nsc}")
        print(f"linker  : {nslink}")
        print(f"buzon   : {inbox if inbox else '(ninguno)'}")
        return 0

    jobs = arguments.jobs if arguments.jobs > 0 else (os.cpu_count() or 4)

    CASE_LOGS.mkdir(parents=True, exist_ok=True)
    BINDIR.mkdir(parents=True, exist_ok=True)
    # Se limpian LOS DOS. Dejar un .obj de la corrida anterior es peor que no tenerlo: si un caso
    # sale de la tabla, su binario viejo se queda ahi con cara de recien hecho.
    for stale in CASE_LOGS.glob("*.log"):
        stale.unlink()
    for stale in list(BINDIR.glob("*.obj")) + list(BINDIR.glob("*.exe")):
        stale.unlink()
    for stale in list(BINDIR.glob("*.rsp")) + list(BINDIR.glob("*.log")):
        stale.unlink()
    if LINKTEMPS.exists():
        shutil.rmtree(LINKTEMPS, ignore_errors=True)
    LINKTEMPS.mkdir(parents=True, exist_ok=True)

    print(f"[NSLINK] {len(work)} trabajos, {jobs} en paralelo")
    print(f"[NSLINK] compiler: {nsc}")
    print(f"[NSLINK] linker  : {nslink}")
    print(f"[NSLINK] buzon   : {inbox if inbox else '(ninguno)'}")
    started = time.monotonic()
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        results = list(pool.map(
            lambda item: run_job(item[0], item[1], item[2], item[3], nsc, nslink), work))
    elapsed = time.monotonic() - started

    # El orden de la TABLA, no el de terminacion: un resumen que cambia de orden entre corridas no
    # se puede comparar con el anterior de un vistazo, que es justo para lo que se lee.
    order = {(label, optimization): index
             for index, (_, label, _, optimization) in enumerate(work)}
    results.sort(key=lambda r: order[(r["label"], r["opt"])])

    reference, note = load_reference()
    groups = classify(results, reference)
    failed = sum(1 for r in results if not r["ok"])
    regressions = len(groups["regressions"])

    header = ["NativeSharp -- CORRIDA CON NSLink (pipeline aislado)",
              "Este reporte NO forma parte del veredicto oficial: sale de nslink_pipeline.py,",
              "que no toca all.bat, run_cases.py, report.py ni tools\\*.cmd.",
              ""]
    REPORT.write_text("\n".join(report_lines(results, reference, note, groups, elapsed, jobs,
                                             nsc, nslink, header)) + "\n",
                      encoding="utf-8", errors="replace")

    RESULTS.write_text(json.dumps({
        "compiler": str(nsc),
        "linker": str(nslink),
        "jobs": jobs,
        "wall_seconds": round(elapsed, 1),
        "serial_seconds": round(sum(r["seconds"] for r in results), 1),
        "passed": len(results) - failed,
        "failed": failed,
        "regressions": regressions,
        "results": results,
    }, indent=2), encoding="utf-8")

    print(f"[NSLINK] {len(results) - failed} OK, {failed} FAIL en {elapsed:.1f} s")
    if reference:
        if regressions:
            print(f"[NSLINK] {regressions} REGRESIONES respecto a lld-link:")
            for r in groups["regressions"]:
                print(f"         {r['label']} {r['opt']} -- {r['detail']} ({r['phase']})")
        else:
            print("[NSLINK] sin regresiones respecto a lld-link")
    else:
        print("[NSLINK] sin referencia con lld-link -- corre all.bat para poder comparar")
    # Dos numeros y una cuenta de regresiones, para que all.bat arme su resumen combinado sin
    # parsear JSON. Se escribe SIEMPRE, incluso en verde: un fichero que solo aparece cuando algo
    # falla obliga a distinguir "no fallo" de "no corrio", y son cosas distintas.
    COUNTS.write_text(f"{len(results) - failed} {failed} {regressions}\n", encoding="ascii")

    print(f"[NSLINK] reporte: {REPORT}")

    if inbox is not None:
        copied, skipped = write_inbox(inbox, results, reference, note, groups,
                                      elapsed, jobs, nsc, nslink)
        print(f"[NSLINK] buzon para NSLink: {inbox}")
        print(f"[NSLINK]   {failed} fallo(s), {copied} fichero(s) copiado(s)"
              + (f", {len(skipped)} objeto(s) omitido(s) por tamano" if skipped else ""))

    # `.gitignore` de este repo tiene `/_logs` pero no `_logs_nslink`, asi que `git status` lo va
    # a mostrar. Se AVISA y no se arregla: modificar .gitignore es tocar un archivo del repo, y el
    # punto de este pipeline es no tocar nada.
    gitignore = ROOT / ".gitignore"
    try:
        covered = gitignore.is_file() and any(
            line.strip().strip("/") == OUT.name
            for line in gitignore.read_text(encoding="utf-8", errors="replace").splitlines())
    except OSError:
        covered = True
    if not covered:
        print(f"[NSLINK] nota: {OUT.name}\\ no esta en .gitignore. Si molesta en git status,")
        print(f"[NSLINK]       la linea es:  /{OUT.name}")

    return 1 if regressions else 0


if __name__ == "__main__":
    sys.exit(main())
