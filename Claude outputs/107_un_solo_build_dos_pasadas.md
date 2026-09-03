# 107 -- Un solo build, dos pasadas

Fecha: 2026-09-02
Ámbito: solo el compilador (`A:\nsc-devel`).
Ficheros tocados: `all.bat`, `nslink_solo.bat`, `nslink_pipeline.py`,
`run_windows_checklist.bat` (comentario).

## QUÉ estaba mal

`all.bat` tenía dos árboles de build, `build-clang-lldlink\` y `build-clang-nslink\`, y **la
pasada 2 volvía a configurar, compilar los 419 objetivos e instalar**. Medido en el log de la
corrida de esa noche:

```
_logs_nslink\01_build.log
  -- The CXX compiler identification is Clang 23.1.0 with MSVC-like command-line
  -- Check for working CXX compiler: C:/clang-llvm/bin/clang-cl.exe - skipped
  419 lineas [N/419]
```

Un build entero, desde cero. **Y construía exactamente el mismo compilador.**

El enlazador del C++ no depende de `NSC_LLD_LINK`: está clavado en la línea de configuración de
`run_windows_checklist.bat`:

```
cmake -S ... -G Ninja -DCMAKE_CXX_COMPILER="%LLVM%/bin/clang-cl.exe"
      -DCMAKE_LINKER="%LLVM%/bin/lld-link.exe"
      -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" ...
```

Las dos pasadas compilaban con clang-cl y enlazaban con lld-link, las dos. `NSC_LLD_LINK` lo lee
`nsc` **en tiempo de ejecución** (`findLldLink()`, `src/Linker/LldLinker.cpp`) para decidir con qué
enlaza **los programas NativeSharp que compila**. Eso es lo único que distingue a las dos pasadas,
y no necesita recompilar una sola línea de C++.

## POR QUÉ estaba así, y por qué el motivo no se sostenía

El comentario de `all.bat` lo justificaba con dos argumentos. Los dos eran del momento en que se
escribió y ninguno sigue en pie:

**«Con dos árboles cada pasada es completa por sí sola.»** Cierto, y a cambio construye dos veces
lo mismo. La pasada 2 ahora depende de que la 1 dejara el árbol sano — que es exactamente lo que
ya pasaba, porque las dos construyen las mismas fuentes y fallarían igual.

**«Si los dos `nsc.exe` no se comportan igual, es un fallo de reproducibilidad y esta corrida es
donde tiene que salir.»** Eso convierte cada corrida en una prueba de reproducibilidad que nunca
ha detectado nada, a cambio de un build por corrida, **y que además debilita lo que la corrida sí
mide**: hoy las dos pasadas usan dos binarios distintos y decimos que son el mismo porque salen de
las mismas fuentes. Para un pipeline cuyo trabajo entero es aislar una variable, «mismo fichero»
es mejor que «mismo código».

Y la separación —que era el motivo real de tener dos árboles— no la daba el árbol. **Ninguna
prueba escribe dentro del árbol de build:** `stage48_5` monta su `ManagedBuild\` bajo su raíz de
evidencia, los logs van a `NSC_LOG_DIR`, los binarios de los trabajos a `_logs_*\bin`, los
temporales de enlace a `_logs_nslink\linktemps` y la evidencia de los rojos a
`_evidencia\<pasada>` desde el doc 104. Todo lo que había que separar ya lo separan otras
variables.

## CÓMO queda

```
PASADA 1  lld-link   build-clang\   ->  _logs_lldlink\    construye e instala
PASADA 2  nslink.exe (el mismo)     ->  _logs_nslink\     NO construye
```

`all.bat` deja de poner `NSC_BUILD_DIR`, así que las dos pasadas usan `build-clang\`, el valor por
omisión del checklist. La variable sigue existiendo para poder apuntar a otro árbol a mano.

### El modo de la pasada 2 no es «norebuild» siempre

Esta es la parte que se puede hacer mal sin notarlo. La pasada 2 no construye, pero tiene que
medir **exactamente los mismos pasos** que la 1 o los dos reportes no se pueden comparar. Por eso
sólo se remapea `full`:

```
full      ->  norebuild    sin build; ctest + trabajos + ejemplos, igual que la 1
quick     ->  quick        el build es incremental y no hace nada; mismos pasos
build     ->  build        ninguna de las dos mide nada; simetrico
norebuild ->  norebuild
```

Si la pasada 2 usara `norebuild` siempre, un `all.bat quick` —que se salta ctest— lo correría en
la 2 y no en la 1, y la comparación mediría dos cosas distintas.

### La limpieza

`build-clang-lldlink\` y `build-clang-nslink\` se borran **siempre**, también en `quick` y
`norebuild`: no son un árbol que conservar, son varios GB que ningún script vuelve a mirar.
`build-clang\` se borra como se borraban antes los dos — salvo en `quick` y `norebuild`, que
existen precisamente para no reconstruir.

### Los otros dos sitios que nombraban los árboles

- `nslink_solo.bat` buscaba `CTestTestfile.cmake` en `build-clang-lldlink\` y luego en
  `build-clang-nslink\`. Ahora prueba `build-clang\` primero y **mantiene los dos nombres viejos
  como respaldo**, por si queda un árbol de antes del cambio.
- `nslink_pipeline.py`, `find_build_directory()`: la misma tupla, en el mismo orden nuevo.

## Lo que se gana, dicho sin inflarlo

Aquel build eran **~71 s** de una corrida de ~20 minutos, medido por las fechas de los ficheros
entre el final de la pasada 1 y el final del build de la 2. En tiempo es poco.

Lo que se gana de verdad es que **las dos pasadas usan el mismo binario**. Y un detalle
secundario: `cmake --install` corría dos veces, así que el `nsc.exe` que quedaba instalado al
final era el de la pasada 2; ahora se instala una vez y el compilador bajo prueba es
demostrablemente el mismo en las dos.

## Verificación

`.bat` no se pueden ejecutar desde el contenedor, así que lo comprobado es de forma, no de
comportamiento:

- Los tres `.bat` siguen siendo **CRLF**, con `file`. (Un `.bat` en LF es una corrida perdida.)
- `nslink_pipeline.py` parsea (`ast.parse`).
- El remapeo de modos se lee entero en el bloque de comentario de `all.bat`, y las cuatro
  entradas están escritas: no hay ninguna combinación sin decidir.
- `report.py` ya trataba `[SKIP]` como un estado de primera clase (líneas 94 y 327), así que el
  `[SKIP] compiler build (norebuild)` de la pasada 2 entra en el reporte sin tocar nada.
- En `norebuild` el checklist **sí** sincroniza `lib\` desde el prefijo instalado antes de ctest
  (paso 1b) y **sí** escribe `01_build.log` —lo crea el `>>` de `prepare_libraries.cmd`—, así que
  ni ctest ni `report.py` se quedan sin lo que necesitan.

Lo que esto **no** verifica es que la corrida entera pase. Eso lo dice el próximo `all.bat`, y
hay una diferencia esperada en el reporte de la pasada 2: donde antes ponía `[ OK ] compiler
build + install` ahora pondrá `[SKIP] compiler build (norebuild)`, así que su contador `Passed:`
va a ser menor que el de la pasada 1. **No es un fallo**: es la pasada 2 diciendo que no
construyó, que es justo lo que queríamos.
