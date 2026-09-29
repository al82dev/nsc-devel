"""build_al_dia.py -- ¿hace falta reconstruir el compilador, o el que hay ya sirve?

POR QUE EXISTE. `all.bat` en modo `full` BORRA `build-clang\\` y construye desde cero, siempre.
Eso son ~71 s de una corrida de ~20 minutos, y la inmensa mayoria de las veces no cambio ni una
linea de C++ -- solo una fixture, un doc o una tabla de casos. Los modos que no borran ya existen
(`quick`, `norebuild`), pero hay que ACORDARSE de escribirlos, y acordarse es justo lo que falla:
un `norebuild` escrito por costumbre despues de tocar el compilador mide el binario viejo y da un
verde que no vale nada.

Este script quita la decision de la memoria de quien lo lanza y la basa en las marcas de tiempo.

EL SESGO ES DELIBERADO Y VA HACIA RECONSTRUIR. Decir "esta al dia" cuando no lo esta produce una
corrida entera medida contra un compilador viejo -- el peor fallo que puede tener este pipeline,
porque sale VERDE. Decir "hay que construir" cuando no hacia falta cuesta segundos, porque Ninja
es incremental y no rehace lo que no cambio. Por eso cualquier duda --un fichero que no se puede
leer, un arbol que no esta, un reloj raro-- devuelve "hay que construir".

QUE CUENTA COMO FUENTE. Lo que el paso de build compila:

    src/  include/  CMakeLists.txt      el compilador
    lib/                                la biblioteca, que se instala en el prefijo
    tests/unit/  tests/support/         los ejecutables de prueba que ctest corre

Y lo que NO, porque son DATOS y no codigo: `tests/cc1/`, `tests/bug_triage/`, `examples/`,
`book_examples/`. Una fixture nueva no cambia el compilador; la recoge `run_cases.py` sin que
nadie compile C++. Incluirlas forzaria un build completo cada vez que se toca un `.ns` de prueba,
que es la mitad de lo que se toca aqui.

`tests/unit/` SI cuenta, y es el caso que costo una corrida el 2026-09-03: cambie la firma de una
interfaz, un doble de prueba se quedo con la antigua, y el build fallo. Si el check ignorara
`tests/unit/`, ese build ni siquiera se habria intentado.

SALIDA:
    0   al dia -- se puede saltar el build
    1   hay que construir (y dice por que, con el fichero concreto)

USO:
    python build_al_dia.py               ¿estan al dia el compilador Y los ejecutables de prueba?
    python build_al_dia.py --solo-nsc    ¿esta al dia solo el compilador?
    ... --silencioso                     sin texto; solo el codigo de salida
"""
import os
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent
PREFIJO = Path(r"C:\native-sharp-3.0")
ARBOL = RAIZ / "build-clang"

# DOS PREGUNTAS DISTINTAS, Y HAY QUE PODER HACER LAS DOS.
#
#   --solo-nsc   ¿esta al dia el COMPILADOR? Lo que entra en `nsc.exe` y en lo que se instala.
#                Es lo que pregunta build_nsc.bat, que construye solo el objetivo `nsc`.
#   (sin flag)   ¿esta al dia TODO lo que el paso de build de all.bat compila? Eso incluye los
#                ~40 ejecutables de prueba, que ctest necesita.
#
# Mezclarlas seria un fallo en las dos direcciones: con la primera, all.bat se saltaria un build
# que sus tests si necesitaban; con la segunda, build_nsc.bat reconstruiria el compilador cada vez
# que alguien toca un fichero de pruebas que a `nsc.exe` no le afecta.
FUENTES_NSC = [
    RAIZ / "src",
    RAIZ / "include",
    RAIZ / "lib",
]
FUENTES_PRUEBAS = [
    RAIZ / "tests" / "unit",
    RAIZ / "tests" / "support",
]
FUENTES_FICHERO = [RAIZ / "CMakeLists.txt"]

# Extensiones que participan en el build. Un `.md` dentro de src/ no reconstruye nada.
EXTENSIONES = {".cpp", ".h", ".hpp", ".c", ".inc", ".ns", ".nsproj", ".txt", ".cmake"}


def fuentes(solo_nsc):
    return FUENTES_NSC if solo_nsc else (FUENTES_NSC + FUENTES_PRUEBAS)


def productos_por_fecha(solo_nsc):
    """Los ficheros del build cuya FECHA se compara contra las fuentes.

    Se toma el MAS ANTIGUO de todos: basta con que uno se haya quedado atras para que el
    conjunto no este al dia.

    SOLO ARTEFACTOS QUE EL COMPILADOR REHACE, y `build.ninja` NO es uno. Ninja lo regenera
    unicamente cuando cambia `CMakeLists.txt`, asi que su fecha se queda en el ultimo
    configure y no se mueve con las fuentes. Compararla producia un falso "hay que
    construir" en la secuencia mas normal que hay:

        configure          -> build.ninja = T0
        editar src/Foo.cpp -> Foo.cpp     = T1 > T0
        build_nsc.bat      -> nsc.exe     = T2 > T1

    ahi el mas antiguo era `build.ninja` (T0), `Foo.cpp` (T1) le ganaba, y el veredicto era
    "hay que construir" con el compilador RECIEN construido. Dentro de build_nsc.bat eso
    costaba segundos --ninja no rehace lo que no cambio-- y por eso paso desapercibido; pero
    test_nsc.bat usa este mismo veredicto para AVISAR de que se va a medir un compilador
    viejo, y un aviso que salta siempre no lo lee nadie. La existencia del arbol se sigue
    comprobando, en `productos_que_deben_existir`; lo que se separa es EXISTIR de ESTAR AL DIA.
    """
    lista = [PREFIJO / "bin" / "nsc.exe"]
    if not solo_nsc:
        # Un ejecutable de prueba cualquiera: si no esta, el build de all.bat tiene trabajo.
        lista.append(ARBOL / "tests" / "nsc_cc1_correctness_tests.exe")
    return lista


def productos_que_deben_existir(solo_nsc):
    """Lo anterior, MAS el arbol de build.

    `build.ninja` va aqui y no en la comparacion de fechas: si el arbol no existe no hay
    nada que reaprovechar, por muy nuevo que sea el `nsc.exe` instalado -- pero su fecha no
    dice nada sobre si ese `nsc.exe` esta al dia.
    """
    return productos_por_fecha(solo_nsc) + [ARBOL / "build.ninja"]


def mtime(ruta):
    try:
        return ruta.stat().st_mtime
    except OSError:
        return None


def fuente_mas_nueva(solo_nsc):
    """(marca de tiempo, ruta) del fichero de fuente mas reciente, o (None, None)."""
    mejor = None
    mejor_ruta = None
    for fichero in FUENTES_FICHERO:
        t = mtime(fichero)
        if t is None:
            # Falta CMakeLists.txt: no es "al dia", es un arbol roto.
            return (float("inf"), fichero)
        if mejor is None or t > mejor:
            mejor, mejor_ruta = t, fichero
    for base in fuentes(solo_nsc):
        if not base.is_dir():
            continue
        for actual, _dirs, ficheros in os.walk(base):
            for nombre in ficheros:
                if Path(nombre).suffix.lower() not in EXTENSIONES:
                    continue
                ruta = Path(actual) / nombre
                t = mtime(ruta)
                if t is None:
                    return (float("inf"), ruta)
                if mejor is None or t > mejor:
                    mejor, mejor_ruta = t, ruta
    return (mejor, mejor_ruta)


def main(argv):
    verboso = "--silencioso" not in argv
    solo_nsc = "--solo-nsc" in argv
    etiqueta = "compilador" if solo_nsc else "compilador y pruebas"

    for producto in productos_que_deben_existir(solo_nsc):
        if mtime(producto) is None:
            if verboso:
                print(f"[build] hay que construir ({etiqueta}): no existe {producto}")
            return 1

    producto_mas_viejo = min(productos_por_fecha(solo_nsc), key=lambda p: mtime(p))
    t_producto = mtime(producto_mas_viejo)

    t_fuente, ruta_fuente = fuente_mas_nueva(solo_nsc)
    if t_fuente is None:
        if verboso:
            print("[build] hay que construir: no se encontro ninguna fuente que comparar")
        return 1
    if t_fuente == float("inf"):
        if verboso:
            print(f"[build] hay que construir: no se pudo leer {ruta_fuente}")
        return 1

    if t_fuente > t_producto:
        if verboso:
            print(f"[build] hay que construir ({etiqueta}): {ruta_fuente}")
            print(f"[build]   es mas nuevo que {producto_mas_viejo}")
        return 1

    if verboso:
        print(f"[build] al dia ({etiqueta}): nada mas nuevo que {producto_mas_viejo}")
        print(f"[build]   la fuente mas reciente es {ruta_fuente}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
