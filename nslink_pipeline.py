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
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent

# Artefactos crudos, en ESTE repo. No es `_logs\` a proposito: report.py construye el veredicto
# oficial desde ahi, y el checklist lo borra entero al arrancar.
OUT = ROOT / "_logs_nslink"
CASE_LOGS = OUT / "cases"
BINDIR = OUT / "bin"
RESULTS = OUT / "results.json"
REPORT = OUT / "REPORTE_NSLINK.txt"

# La corrida de referencia, SOLO PARA LEER. La escribe run_cases.py con lld-link.
REFERENCE = ROOT / "_logs" / "cases" / "results.json"

# El buzon en el arbol de NSLink. Un directorio de ENTRADA, aparte de su corpus curado: no es
# nuestro sitio decidir que entra en `corpus\`, eso lo decide quien mantiene NSLink.
DEFAULT_NSLINK_DEVEL = Path(r"A:\nslink-devel")
INBOX_NAME = "_inbox_nsc"

# Tope de lo que se copia al buzon. Un .obj de estos anda por los 100 KB; 84 fallos serian ~8 MB
# en el arbol de otro. Se avisa en vez de llenarselo en silencio.
INBOX_MAX_BYTES = 64 * 1024 * 1024

# Un trabajo que tarda mas que esto esta colgado, no lento.
JOB_TIMEOUT_SECONDS = 300

IC = "tests/interop_contract"
LC = "tests/language"
CC = "tests/cc1"

# (proyecto relativo a la raiz, etiqueta, salida esperada)
#
# ESTA TABLA ESTA DUPLICADA DE run_cases.py, Y ES DELIBERADO. Importarla ataria este experimento
# al script de produccion: un cambio aqui para probar algo podria romper la corrida oficial, que
# es exactamente lo que un pipeline aislado no debe poder hacer. El precio es que si se anade una
# fixture hay que anadirla en los dos sitios; el reporte avisa cuando los conteos dejan de cuadrar.
#
# `expected = None` significa BUILD-ONLY: compila y enlaza, no se ejecuta.
CASES: list[tuple[str, str, int | None]] = [
    # --- interop contract ---
    (f"{IC}/c1_03_function_pointer_parameter/c1_03_function_pointer_parameter.nsproj", "c1_03_function_pointer_parameter", 42),
    (f"{IC}/c1_04_function_pointer_return/c1_04_function_pointer_return.nsproj", "c1_04_function_pointer_return", 42),
    (f"{IC}/c1_05_raw_pointer_cast/c1_05_raw_pointer_cast.nsproj", "c1_05_raw_pointer_cast", 42),
    (f"{IC}/c2_11_fixed_field_array/c2_11_fixed_field_array.nsproj", "c2_11_fixed_field_array", 42),
    (f"{IC}/c2_12_union/c2_12_union.nsproj", "c2_12_union", 42),
    (f"{IC}/c4_ref_out_in/c4_ref_out_in.nsproj", "c4_ref_out_in", 42),
    (f"{IC}/c6_struct_math/c6_struct_math.nsproj", "c6_struct_math", 42),
    (f"{IC}/c7_definite_assignment/c7_definite_assignment.nsproj", "c7_definite_assignment", 42),
    (f"{IC}/c8_type_root/c8_type_root.nsproj", "c8_type_root", 42),
    # La frontera de excepciones a traves de un callback de C: sale con 141 A PROPOSITO. Es el
    # caso mas interesante de todo el barrido para NSLink, porque depende de .pdata/.xdata.
    (f"{IC}/c1_14_callback_exception_boundary/c1_14_callback_exception_boundary.nsproj", "c1_14_callback_exception_boundary", 141),
    # Dos bibliotecas con el mismo simbolo: se certifica ENLAZANDO, no ejecutando. Es resolucion
    # de simbolos pura -- otro caso que mide al enlazador y no al compilador.
    (f"{IC}/c1_13_two_libraries_same_symbol/c1_13_two_libraries_same_symbol.nsproj", "c1_13_two_libraries_same_symbol", None),

    # --- language ---
    (f"{LC}/l1_conditional/l1_conditional.nsproj", "l1_conditional", 42),
    (f"{LC}/l2_dowhile/l2_dowhile.nsproj", "l2_dowhile", 42),
    (f"{LC}/l3_switch/l3_switch.nsproj", "l3_switch", 42),

    # --- milestone CC1 ---
    (f"{CC}/c1_constants/c1_constants.nsproj", "cc1_c1_constants", 42),
    (f"{CC}/c2_ternary_ownership/c2_ternary_ownership.nsproj", "cc1_c2_ternary_ownership", 42),
    (f"{CC}/c3_dowhile_flow/c3_dowhile_flow.nsproj", "cc1_c3_dowhile_flow", 42),
    (f"{CC}/c4_catch_continue/c4_catch_continue.nsproj", "cc1_c4_catch_continue", 42),
    (f"{CC}/c5_catch_break/c5_catch_break.nsproj", "cc1_c5_catch_break", 42),
    (f"{CC}/c6_catch_return/c6_catch_return.nsproj", "cc1_c6_catch_return", 42),
    (f"{CC}/c7_finally_switch/c7_finally_switch.nsproj", "cc1_c7_finally_switch", 42),
    (f"{CC}/c8_loop_catch_switch/c8_loop_catch_switch.nsproj", "cc1_c8_loop_catch_switch", 42),
    (f"{CC}/c9_sizeof/c9_sizeof.nsproj", "cc1_c9_sizeof", 42),
    (f"{CC}/c10_stackalloc/c10_stackalloc.nsproj", "cc1_c10_stackalloc", 42),
    (f"{CC}/c11_raw_new/c11_raw_new.nsproj", "cc1_c11_raw_new", 42),
    (f"{CC}/c12_pointer_arith/c12_pointer_arith.nsproj", "cc1_c12_pointer_arith", 42),
    (f"{CC}/c13_increment/c13_increment.nsproj", "cc1_c13_increment", 42),
    (f"{CC}/c14_compound_assign/c14_compound_assign.nsproj", "cc1_c14_compound_assign", 42),
    (f"{CC}/c15_arrow_member/c15_arrow_member.nsproj", "cc1_c15_arrow_member", 42),
    (f"{CC}/c16_enum_layout/c16_enum_layout.nsproj", "cc1_c16_enum_layout", 42),
    (f"{CC}/c17_constant_promotion/c17_constant_promotion.nsproj", "cc1_c17_constant_promotion", 42),
    (f"{CC}/c18_stackalloc_hardening/c18_stackalloc_hardening.nsproj", "cc1_c18_stackalloc_hardening", 42),
    (f"{CC}/c19_sbyte/c19_sbyte.nsproj", "cc1_c19_sbyte", 42),
    (f"{CC}/c20_property_compound/c20_property_compound.nsproj", "cc1_c20_property_compound", 42),
    (f"{CC}/c21_number_parsing/c21_number_parsing.nsproj", "cc1_c21_number_parsing", 42),
    (f"{CC}/c22_console_and_formatting/c22_console_and_formatting.nsproj", "cc1_c22_console_and_formatting", 42),
    (f"{CC}/c23_views_and_lifetimes/c23_views_and_lifetimes.nsproj", "cc1_c23_views_and_lifetimes", 42),

    # --- ejemplos empaquetados ---
    ("examples/project_stage56_c1/Stage56C1InteropCompletion.nsproj", "project_stage56_c1", 0),
    ("examples/project_stage56_c2c4/Stage56C2C4LanguageCompletion.nsproj", "project_stage56_c2c4", 0),
    ("examples/project_stage56_c6/Stage56C6StructMath.nsproj", "project_stage56_c6", 0),
    ("examples/project_stage56_c7/project_stage56_c7.nsproj", "project_stage56_c7", 0),
    ("examples/project_stage56_c8/project_stage56_c8.nsproj", "project_stage56_c8", 0),
]

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


def child_environment(nslink: Path) -> dict:
    """El entorno de UN trabajo.

    `NSC_LLD_LINK` es la pieza entera de este pipeline -- ver la cabecera del archivo.

    `NSC_OPT` se BORRA a proposito: no se llama a build_project.cmd, el nivel de optimizacion va
    en la linea de comandos. Si alguien lo tiene exportado en su sesion apuntando a otra cosa,
    aqui no manda.
    """
    env = dict(os.environ)
    env["NSC_LLD_LINK"] = str(nslink)
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

    env = child_environment(nslink)

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
# La comparacion. Es la mitad util del pipeline: "84 OK" no dice nada por si solo si no se sabe
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
        out.append("  La tabla CASES de este script esta duplicada de run_cases.py a proposito")
        out.append("  (aislamiento). Si alguien anadio una fixture alla, hay que anadirla aqui.")
        out.append("")
    return out


# ----------------------------------------------------------------------------------------------
# EL BUZON. Lo que se le deja a quien desarrolla NSLink.
# ----------------------------------------------------------------------------------------------
def inbox_readme(nsc: Path, nslink: Path, groups: dict, copied: list[str],
                 skipped: list[str]) -> str:
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
        inbox_readme(nsc, nslink, groups, copied, skipped), encoding="utf-8", errors="replace")
    return len(copied), skipped


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
    for stale in CASE_LOGS.glob("*.log"):
        stale.unlink()

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
