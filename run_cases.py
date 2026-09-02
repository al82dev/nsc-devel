#!/usr/bin/env python3
"""
NativeSharp -- construye y ejecuta TODAS las fixtures y ejemplos EN PARALELO.

POR QUE EXISTE
--------------
El checklist las corria en serie, una `call :run_case` detras de otra: 47 casos x 2 niveles de
optimizacion = 94 compilaciones y enlazados, uno despues de otro, en una maquina con muchos nucleos
ociosos. Cada caso es independiente de los demas --su propio .obj, su propio .exe, su propia
salida-- asi que la serie no compraba nada.

POR QUE EN PYTHON Y NO EN .bat
------------------------------
cmd.exe no sabe esperar. `start /b` lanza y sigue, asi que hay que inventar ficheros centinela y
un bucle de espera, y el codigo de salida de cada trabajo hay que sacarlo de otro fichero. Nada de
eso es robusto y todo es codigo que hay que mantener. Un `ThreadPoolExecutor` con `subprocess.run`
da paralelismo, codigo de salida fiable, timeout por trabajo y un log por caso sin trucos.

EL REQUISITO DURO, y no es opcional
-----------------------------------
`build_project.cmd` llama a `prepare_libraries.cmd` en CADA invocacion, y ese hace

    rmdir /s /q lib\\NativeSharp.Core   +   xcopy desde el prefijo instalado

En serie eso solo desperdicia 94 sincronizaciones. EN PARALELO ES UNA CARRERA QUE CORROMPE `lib\\`:
un proceso borra el directorio mientras otro lo esta leyendo, y el fallo sale en un caso al azar
con un mensaje que no tiene nada que ver.

`prepare_libraries.cmd` se salta el trabajo si `NS_LIB_READY` esta definida. El checklist ya
sincroniza antes de ctest y exporta esa variable, por eso hoy funciona. Aqui se pone
EXPLICITAMENTE en el entorno de cada hijo en vez de confiar en heredarla: si alguien corre este
script suelto, sigue siendo seguro.

QUE DEJA
--------
    _logs\\cases\\<etiqueta>_<opt>.log     un log por trabajo, entero
    _logs\\cases\\results.json             lo que report.py consolida
    _logs\\SUMMARY.txt                     las mismas lineas [ OK ] / [FAIL] que antes

El formato de SUMMARY.txt se conserva a proposito: report.py y la costumbre de leerlo no cambian.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent
LOGS = ROOT / "_logs"
CASE_LOGS = LOGS / "cases"
BINDIR = LOGS / "bin"
SUMMARY = LOGS / "SUMMARY.txt"
RESULTS = CASE_LOGS / "results.json"

BUILD_TOOL = ROOT / "tools" / "build_project.cmd"

# Un trabajo que tarda mas que esto esta colgado, no lento. Sin tope, un solo caso atascado se
# come la corrida entera y no hay forma de saber cual.
JOB_TIMEOUT_SECONDS = 300

IC = "tests/interop_contract"
LC = "tests/language"
CC = "tests/cc1"

# (proyecto relativo a la raiz, etiqueta, salida esperada)
#
# `expected = None` significa BUILD-ONLY: compila y enlaza, no se ejecuta. Es lo que hacia
# :build_only, y existe para el caso de las dos bibliotecas con el mismo simbolo, que se certifica
# enlazando.
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
    # La frontera de excepciones a traves de un callback de C: sale con 141 A PROPOSITO.
    (f"{IC}/c1_14_callback_exception_boundary/c1_14_callback_exception_boundary.nsproj", "c1_14_callback_exception_boundary", 141),
    # Dos bibliotecas con el mismo simbolo: se certifica ENLAZANDO, no ejecutando.
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


def child_environment(optimization: str) -> dict:
    """El entorno de UN trabajo. Cada hijo tiene el suyo: en serie `set NSC_OPT` era global y
    funcionaba por accidente; en paralelo dos trabajos con distinto -O se pisarian."""
    env = dict(os.environ)
    env["NSC_OPT"] = optimization
    # Ver la cabecera: sin esto, cada build_project.cmd re-sincroniza lib\ con un rmdir, y en
    # paralelo eso es una carrera que corrompe la biblioteca.
    env["NS_LIB_READY"] = "1"
    return env


def run_job(project: str, label: str, expected: int | None, optimization: str) -> dict:
    name = f"{label} {optimization}"
    log_path = CASE_LOGS / f"{label}_{optimization.lstrip('-')}.log"
    exe = BINDIR / f"{label}_{optimization.lstrip('-')}.exe"
    lines: list[str] = [f"--- {name} ---"]
    started = time.monotonic()

    project_path = ROOT / project
    if not project_path.is_file():
        lines.append(f"[ERROR] proyecto no encontrado: {project_path}")
        log_path.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")
        return {"label": label, "opt": optimization, "ok": False,
                "detail": "proyecto no encontrado", "log": log_path.name,
                "seconds": 0.0}

    # STDERR TAMBIEN, y no es un detalle. `nsc.exe` manda los diagnosticos por ERROR estandar:
    # sin esta rama, un caso que no compila deja un log con "[FAILED] ... (obj, exit 1)" y NADA
    # mas, y el mensaje del compilador hay que ir a buscarlo al log de ctest. Costo un viaje
    # entero encontrar por que fallaba c22.
    #
    # Y desde SHR-022 tambien importa en la EJECUCION: un programa NativeSharp ya puede escribir
    # por stderr, asi que descartarlo seria tirar justo la mitad que el Bloque 2 acaba de anadir.
    def record(completed) -> None:
        if completed.stdout:
            lines.append(completed.stdout.rstrip())
        if completed.stderr:
            lines.append(completed.stderr.rstrip())

    try:
        build = subprocess.run(
            ["cmd", "/c", str(BUILD_TOOL), str(project_path), str(exe)],
            cwd=str(ROOT), env=child_environment(optimization),
            capture_output=True, text=True, errors="replace",
            timeout=JOB_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        lines.append(f"[TIMEOUT] la construccion supero {JOB_TIMEOUT_SECONDS} s")
        log_path.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")
        return {"label": label, "opt": optimization, "ok": False,
                "detail": "timeout al construir", "log": log_path.name,
                "seconds": time.monotonic() - started}
    record(build)

    if build.returncode != 0:
        detail = "build failed" if expected is not None else "build/link failed"
        log_path.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")
        return {"label": label, "opt": optimization, "ok": False, "detail": detail,
                "log": log_path.name, "seconds": time.monotonic() - started}

    if expected is None:
        lines.append("[exit -] compila y linkea, no se ejecuta")
        log_path.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")
        return {"label": label, "opt": optimization, "ok": True,
                "detail": "compila y linkea, no se ejecuta", "log": log_path.name,
                "seconds": time.monotonic() - started}

    try:
        run = subprocess.run([str(exe)], cwd=str(ROOT), capture_output=True, text=True,
                             errors="replace", timeout=JOB_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        lines.append(f"[TIMEOUT] la ejecucion supero {JOB_TIMEOUT_SECONDS} s")
        log_path.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")
        return {"label": label, "opt": optimization, "ok": False,
                "detail": "timeout al ejecutar", "log": log_path.name,
                "seconds": time.monotonic() - started}
    record(run)
    lines.append(f"[exit {run.returncode}]")
    log_path.write_text("\n".join(lines) + "\n", encoding="utf-8", errors="replace")

    ok = run.returncode == expected
    detail = f"exit {run.returncode}" if ok else f"exit {run.returncode}, se esperaba {expected}"
    return {"label": label, "opt": optimization, "ok": ok, "detail": detail,
            "log": log_path.name, "seconds": time.monotonic() - started}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("-j", "--jobs", type=int, default=0,
                        help="trabajos en paralelo (por omision: los nucleos de la maquina)")
    parser.add_argument("--dry-run", action="store_true",
                        help="lista los trabajos y sale, sin construir nada")
    arguments = parser.parse_args()

    jobs = arguments.jobs if arguments.jobs > 0 else (os.cpu_count() or 4)

    work = [(project, label, expected, optimization)
            for (project, label, expected) in CASES
            for optimization in OPT_LEVELS]

    if arguments.dry_run:
        for project, label, expected, optimization in work:
            print(f"{label} {optimization} -> {expected if expected is not None else 'build-only'}")
        print(f"{len(work)} trabajos, {jobs} en paralelo")
        return 0

    CASE_LOGS.mkdir(parents=True, exist_ok=True)
    BINDIR.mkdir(parents=True, exist_ok=True)
    for stale in CASE_LOGS.glob("*.log"):
        stale.unlink()

    print(f"[FIXTURES] {len(work)} trabajos, {jobs} en paralelo")
    started = time.monotonic()
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        results = list(pool.map(lambda item: run_job(*item), work))
    elapsed = time.monotonic() - started

    # El ORDEN DE LA TABLA, no el de terminacion. Un resumen que cambia de orden entre corridas no
    # se puede comparar con el anterior de un vistazo, que es justo para lo que se lee.
    order = {(label, optimization): index
             for index, (_, label, _, optimization) in enumerate(work)}
    results.sort(key=lambda r: order[(r["label"], r["opt"])])

    passed = sum(1 for r in results if r["ok"])
    failed = len(results) - passed

    summary_lines = []
    for r in results:
        mark = "[ OK ]" if r["ok"] else "[FAIL]"
        summary_lines.append(f"{mark} {r['label']} {r['opt']} -- {r['detail']}")
    with SUMMARY.open("a", encoding="utf-8", errors="replace") as handle:
        handle.write("\n".join(summary_lines) + "\n")

    serial = sum(r["seconds"] for r in results)
    RESULTS.write_text(json.dumps({
        "jobs": jobs,
        "wall_seconds": round(elapsed, 1),
        "serial_seconds": round(serial, 1),
        "passed": passed,
        "failed": failed,
        "results": results,
    }, indent=2), encoding="utf-8")

    # Los contadores, para que el .bat los sume a los suyos sin tener que parsear JSON. Un
    # fichero con dos numeros y un espacio es lo unico que cmd.exe lee sin pelearse.
    (CASE_LOGS / "counts.txt").write_text(f"{passed} {failed}\n", encoding="ascii")

    print(f"[FIXTURES] {passed} OK, {failed} FAIL en {elapsed:.1f} s "
          f"(suma de tiempos {serial:.1f} s -- x{serial / elapsed:.1f})")
    for r in results:
        if not r["ok"]:
            print(f"           FAIL {r['label']} {r['opt']} -- {r['detail']}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
