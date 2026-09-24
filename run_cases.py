#!/usr/bin/env python3
"""
NativeSharp -- construye y ejecuta TODAS las fixtures y ejemplos EN PARALELO.

POR QUE EXISTE
--------------
El checklist las corria en serie, una `call :run_case` detras de otra. La matriz actual ejecuta
cada caso en O0, O1, O2 y O3, con compilaciones y enlaces independientes en paralelo en vez de
dejarlos uno despues de otro en una maquina con muchos nucleos
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

En serie eso desperdicia una sincronizacion por trabajo. EN PARALELO ES UNA CARRERA QUE CORROMPE `lib\\`:
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

# NSC_LOG_DIR: `all.bat` corre las dos pasadas -- una por enlazador -- en directorios separados, y
# cada una escribe en el suyo. Sin la variable, `_logs` como siempre, asi que llamar a este script
# a mano sigue funcionando igual.
LOGS = ROOT / os.environ.get("NSC_LOG_DIR", "_logs")
CASE_LOGS = LOGS / "cases"
BINDIR = LOGS / "bin"
SUMMARY = LOGS / "SUMMARY.txt"
RESULTS = CASE_LOGS / "results.json"

BUILD_TOOL = ROOT / "tools" / "build_project.cmd"

# Un trabajo que tarda mas que esto esta colgado, no lento. Sin tope, un solo caso atascado se
# come la corrida entera y no hay forma de saber cual.
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


OPT_LEVELS = ("-O0", "-O1", "-O2", "-O3")


_LINKER_PATH_CACHE: str | None = None


def linker_path() -> str:
    """La ruta del enlazador que este arnes esta usando, para el fixture c26_driver_minimo.

    POR QUE EXISTE. El criterio de cierre del Bloque 4 (doc 69) es que un programa NativeSharp
    "llame al enlazador", y c26 lo hace con Process.Run. Pero un programa NativeSharp no sabe
    buscar en el PATH -- no hay API para eso todavia -- asi que la ruta se la tiene que dar
    quien la conoce.

    SE LA PREGUNTAMOS AL COMPILADOR, Y ESA ES LA CORRECCION DEL 2026-09-02. La primera version
    de esta funcion reimplementaba la busqueda en Python: NSC_LLD_LINK, y si no,
    shutil.which("lld-link"). Parecia "la misma regla que findLldLink()" y no lo era: en la
    corrida de esa noche `nsc` enlazo los 90 trabajos sin problema y shutil.which no encontro
    nada, asi que c26 imprimio "sin NSC_CC1_LINKER ni NSC_LLD_LINK, no se lanzo el enlazador" --
    el paso 6, que es LITERALMENTE el criterio de cierre del bloque, no se ejecuto, y la corrida
    salio verde igual.

    Otra copia de una regla que se desincroniza en silencio, la cuarta de la ronda (docs 102,
    103, 105). `nsc --print-linker` ya existia y llama a LldLinker::discover(), que es LA
    funcion: preguntarselo a el no puede desincronizarse por construccion.

    Si `nsc` no encuentra enlazador, se devuelve cadena vacia y c26 avisa por su salida sin
    fallar -- una fixture que se pusiera roja en una maquina sin enlazador estaria midiendo la
    maquina.

    (En la pasada 2 esto ni hace falta: nslink_pipeline.py pone NSC_LLD_LINK apuntando a
    nslink.exe, que es el segundo sitio donde c26 mira. Pero conviene que las dos pasadas
    lleguen por el mismo camino.)
    """
    global _LINKER_PATH_CACHE
    if _LINKER_PATH_CACHE is not None:
        return _LINKER_PATH_CACHE

    nsc = os.environ.get("NSC_EXE", "")
    if not nsc:
        nsc = str(Path(os.environ.get("NS_ROOT", r"C:\native-sharp")) / "bin" / "nsc.exe")

    resolved = ""
    if Path(nsc).is_file():
        try:
            probe = subprocess.run([nsc, "--print-linker"], capture_output=True, text=True,
                                   errors="replace", timeout=60)
            for line in probe.stdout.splitlines():
                # `nsc --print-linker` imprime "linker: <ruta>" y despues flavor/target.
                if line.startswith("linker:"):
                    candidate = line.split(":", 1)[1].strip()
                    if candidate and Path(candidate).is_file():
                        resolved = candidate
                    break
        except (OSError, subprocess.SubprocessError):
            resolved = ""

    _LINKER_PATH_CACHE = resolved
    return resolved


def child_environment(optimization: str) -> dict:
    """El entorno de UN trabajo. Cada hijo tiene el suyo: en serie `set NSC_OPT` era global y
    funcionaba por accidente; en paralelo dos trabajos con distinto -O se pisarian."""
    env = dict(os.environ)
    env["NSC_OPT"] = optimization
    # Ver la cabecera: sin esto, cada build_project.cmd re-sincroniza lib\ con un rmdir, y en
    # paralelo eso es una carrera que corrompe la biblioteca.
    env["NS_LIB_READY"] = "1"
    # Para c26_driver_minimo -- ver linker_path() justo arriba.
    linker = linker_path()
    if linker:
        env["NSC_CC1_LINKER"] = linker
    return env


def with_link_temp_subdirectory(env: dict, label: str, optimization: str) -> dict:
    """Un subdirectorio de temporales de enlace POR TRABAJO, y solo si alguien los pidio.

    `NSC_KEEP_LINK_TEMPS` hace que `nsc` conserve el directorio temporal del enlace, donde viven
    program.obj, runtime.obj y las bibliotecas de importacion de esa invocacion. La usa la pasada
    con NSLink de all.bat para poder entregarselos a quien desarrolla el enlazador.

    El problema que resuelve esta funcion: `nsc` nombra ese directorio con un sello de reloj, asi
    que con dieciseis trabajos en paralelo vaciandose en la MISMA raiz no hay forma de saber cual
    es de quien. Un subdirectorio por trabajo lo vuelve deterministico.

    SI LA VARIABLE NO ESTA PUESTA, ESTO NO HACE NADA. La corrida oficial no deja temporales.
    """
    root = env.get("NSC_KEEP_LINK_TEMPS", "")
    if not root:
        return env
    env["NSC_KEEP_LINK_TEMPS"] = str(Path(root) / f"{label}_{optimization.lstrip('-')}")
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
            cwd=str(ROOT),
            env=with_link_temp_subdirectory(child_environment(optimization), label, optimization),
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

    # El enlazador se resuelve UNA VEZ, aqui, y no dentro de los hilos: la llamada a
    # `nsc --print-linker` es un subproceso, y hacerlo 90 veces en paralelo seria absurdo ademas
    # de una carrera sobre la cache.
    #
    # Y SE IMPRIME. Sin esta linea, que c26 no encuentre enlazador se ve solo dentro del log de
    # ese caso -- que es exactamente como la corrida del 2026-09-02 salio VERDE con el paso 6 del
    # criterio de cierre del Bloque 4 sin ejecutarse. Un aviso que hay que ir a buscar no es un
    # aviso.
    linker = linker_path()
    if linker:
        print(f"[CASOS] enlazador para c26_driver_minimo: {linker}")
    else:
        print("[CASOS] AVISO: `nsc --print-linker` no devolvio ningun enlazador.")
        print("[CASOS] c26_driver_minimo NO va a lanzar el enlazador -- el paso 6 de su")
        print("[CASOS] criterio de cierre se saltara, y el caso saldra verde igualmente.")

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
