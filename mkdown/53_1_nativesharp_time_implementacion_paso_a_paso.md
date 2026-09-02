# 53.1 — Implementación paso a paso: Time

Este documento acompaña a `53_1_nativesharp_time.md` (el documento conceptual). Ahí está el
"qué" y el "por qué" de cada decisión de diseño; acá está el "cómo", en el orden real en que
sucedió, con los comandos y salidas reales que confirmaron cada paso — incluidos los tropiezos, no
solo el camino feliz.

## Paso 0 — Confirmar la base antes de tocar nada

Antes de escribir una sola línea, se corrió la suite completa tal cual quedó al cerrar Stage 52,
para tener una línea de base indiscutible contra la cual comparar cualquier resultado futuro:

```
$ ctest --output-on-failure -j$(nproc)
...
100% tests passed, 0 tests failed out of 210
```

210/210. Cualquier falla posterior a partir de acá es atribuible a Stage 53.1, no a una regresión
heredada.

## Paso 1 — Investigar si existe un mecanismo de FFI de importación

Antes de diseñar nada, había que confirmar (no asumir) si NativeSharp tiene alguna forma de
llamar una API de Win32 arbitraria. Se buscó el uso de `extern` en el frontend:

- `Parser.cpp` confirma: **todo método requiere cuerpo, salvo los `abstract`** — incluidos los
  marcados `extern`.
- Los usos existentes de `extern` (Stage 47/48) son siempre `extern "C"` + `export`, con cuerpo
  real, para **exportar** una función NativeSharp hacia afuera (a un host C/C++/C#).

Conclusión confirmada: no existe la dirección inversa. No hay atajo. Cada primitiva de Win32
nueva pasa por el mecanismo de "operación núcleo" ya usado por `String`/`Console`. Ver la sección
3 del documento conceptual para el razonamiento completo.

## Paso 2 — Reverse-engineering del mecanismo de operación núcleo usando `String`/`Console` como plantilla

Se leyeron, en orden, los 8 archivos que ya implementan `StringConcat`/`ConsoleWriteLine` para
entender la forma exacta de las 4 capas (frontend → HIR → MIR → LLVM) y la capa de runtime por
separado. El detalle completo de qué hace cada capa está en la sección 4 del documento
conceptual — acá solo el orden en que se investigó:

1. `include/nativesharp/Frontend/ProjectMethodBodyModel.h` — el enum `MethodCoreOperationKind`.
2. `src/Frontend/ProjectMethodBodyModel.cpp` — el bloque de reconocimiento (nombre de clase +
   `static` + firma exacta).
3. `include/nativesharp/Frontend/ProjectOopHirModel.h` / `.cpp` — el nodo HIR.
4. `include/nativesharp/Frontend/ProjectOopMirModel.h` / `.cpp` — el nodo MIR y la conversión
   HIR→MIR.
5. `src/LLVM/ProjectOopLlvmLowerer.cpp` — el lowering final a una llamada LLVM real.
6. `src/Runtime/WindowsRuntimeBuilder.cpp` — la implementación real del símbolo runtime, en un
   módulo LLVM separado.

## Paso 3 — Diseñar la superficie mínima antes de escribir compilador

Antes de tocar C++ se decidió la superficie completa en NativeSharp puro (ver sección 9 del
documento conceptual), y se identificaron exactamente 3 primitivas que necesitan tocar el sistema
operativo: `QueryCounterRaw`, `QueryFrequencyRaw`, `QuerySystemFileTimeRaw`. Todo lo demás
(`TimeSpan`, `DateTime`, `Stopwatch`) se puede escribir como NativeSharp normal encima de esas 3.

Se detectó en este paso el riesgo de referencia circular entre proyectos (ver sección 8 del
documento conceptual) y se decidió de antemano poner `NativeClock.ns` en el proyecto
`NativeSharp.Core` en vez de crear un proyecto `Diagnostics` separado — evitando el problema en
vez de tropezar con él más tarde.

## Paso 4 — Escribir las 3 primitivas con cuerpo placeholder

`lib/NativeSharp.Core/NativeClock.ns`, con las tres funciones devolviendo `return 0;` como cuerpo
placeholder (el cuerpo real nunca se ejecuta una vez que el compilador las reconoce como
operación núcleo — ver Paso 1 del documento conceptual, sección 4, capa 1).

## Paso 5 — Extender las 4 capas del compilador

Se replicó, capa por capa, el mismo patrón exacto que ya usa `ConsoleWriteLine`, agregando las
tres operaciones nuevas al lado de cada bloque existente (nunca reemplazando nada, siempre
agregando un bloque hermano) en los 6 archivos de las capas 1-4:

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` — 3 nuevos valores de enum.
- `src/Frontend/ProjectMethodBodyModel.cpp` — bloque de reconocimiento (clase
  `"NativeSharp.Core.Diagnostics.NativeClock"`, `static`, `builtin:long`, 0 parámetros, despacho
  por nombre) + 3 líneas en `methodCoreOperationKindName()`.
- `include/nativesharp/Frontend/ProjectOopHirModel.h` — 3 nuevos valores de enum.
- `src/Frontend/ProjectOopHirModel.cpp` — 3 casos de `toString()` + bloque de creación de nodo
  (nodo sin hijos, `"builtin:long"`).
- `include/nativesharp/Frontend/ProjectOopMirModel.h` — 3 nuevos valores de enum.
- `src/Frontend/ProjectOopMirModel.cpp` — 3 casos de `toString()` + 3 líneas de conversión
  HIR→MIR.

## Paso 6 — Lowering a LLVM y wrappers de runtime

`src/LLVM/ProjectOopLlvmLowerer.cpp`: 3 constantes de nombre de símbolo, 6 campos nuevos
(`LLVMValueRef`/`LLVMTypeRef`), extensión de `declareRuntime()` (declaración + cadena de
null-check), 3 `case` de lowering con guard de error.

`src/Runtime/WindowsRuntimeBuilder.cpp`: el helper compartido `buildOutParamI64Call()` (ver
sección 5 del documento conceptual para el porqué de este diseño), las 3 funciones
`buildClockQueryCounter/buildClockQueryFrequency/buildClockSystemFileTime`, la declaración de los
3 imports crudos de Win32 (`QueryPerformanceCounter`, `QueryPerformanceFrequency`,
`GetSystemTimeAsFileTime`) y su incorporación a la cadena de construcción/verificación existente
en `buildInternal()`.

## Paso 7 — Escribir `TimeSpan`, `DateTime`, `Stopwatch` en NativeSharp puro

Con las 3 primitivas ya wireadas, el resto es NativeSharp normal: `lib/NativeSharp.Core/TimeSpan.ns`,
`lib/NativeSharp.Core/DateTime.ns` (con la derivación de la constante de época, ver sección 6 del
documento conceptual), y el nuevo proyecto `lib/NativeSharp.Core.Diagnostics/` con
`Stopwatch.ns`.

## Paso 8 — Primer intento de build: timeout

```
$ cmake --build . -j$(nproc)
```

Excedió el timeout por defecto de 120 segundos de la herramienta. No es un error del compilador —
simplemente una build completa de LLVM+el árbol nsc tarda más que eso. Se reintentó con un
timeout explícito más alto:

```
$ cmake --build . -j$(nproc)     # (timeout 570000 ms)
```

Terminó sin errores de compilación.

## Paso 9 — Reconfigurar CMake para registrar el test nuevo

Agregar `tests/unit/Stage53_1TimeTests.cpp` como ejecutable nuevo requiere un target nuevo en
`CMakeLists.txt` — eso exige un `cmake .` completo de reconfiguración, no alcanza con
`cmake --build .` solo (que no re-lee `CMakeLists.txt` para descubrir targets nuevos si no se le
pide explícitamente).

## Paso 10 — 16 fallos de link: `undefined symbol`

Tras la reconfiguración y el rebuild, `ctest` reportó 16 tests fallando en la etapa de link, todos
con la misma forma de error. Se reprodujo directo para tener el mensaje completo:

```
$ ./bin/nsc --emit-exe /tmp/ns51/probe/Main.ns -o /tmp/ns51/probe/Main.exe
lld-link: error: undefined symbol: QueryPerformanceCounter
>>> referenced by ...
lld-link: error: undefined symbol: QueryPerformanceFrequency
lld-link: error: undefined symbol: GetSystemTimeAsFileTime
```

Causa raíz: nsc no linkea contra un `kernel32.lib` real — sintetiza su propia biblioteca de
importación mínima de `KERNEL32.dll` en el momento del link, a partir de una whitelist fija de
símbolos (`kernel32Symbols`), **duplicada en 5 archivos**, ninguno de los cuales incluía las 3
funciones nuevas:

- `src/Linker/WindowsLibraryBuilder.cpp`
- `src/Linker/WindowsExecutableBuilder.cpp`
- `tests/unit/Stage48_3CHostIntegrationTests.cpp`
- `tests/unit/Stage47HostCAbiTests.cpp`
- `tests/unit/Stage48_4CppHostIntegrationTests.cpp`

**Fix**: se agregaron las 3 entradas nuevas a `kernel32Symbols` en los 5 archivos (tamaño del
arreglo 10→13), aplicado con dos scripts de reemplazo exacto de bloque (uno para el formato
multilínea de los 2 archivos de producción, otro para el formato compacto de los 3 archivos de
test) para garantizar que las 5 copias quedaran consistentes entre sí. Tras el fix, la
reproducción directa:

```
$ ./bin/nsc --emit-exe /tmp/ns51/probe/Main.ns -o /tmp/ns51/probe/Main.exe
$ echo $?
0
```

## Paso 11 — Error de compilación NativeSharp: receptor de struct no direccionable

Al escribir `Stopwatch.ElapsedMilliseconds()`/`ElapsedSeconds()` con la forma directa
`Elapsed().TotalMilliseconds()`:

```
nsc: error: Stage 40 struct instance receiver must be an addressable local, parameter or field;
temporary struct values are not addressable in Stage 40
```

Causa: el valor devuelto por `Elapsed()` es un temporal sin dirección de memoria estable. **Fix**:
nombrar el resultado en un local antes de llamar el método (`TimeSpan elapsed = Elapsed(); return
elapsed.TotalMilliseconds();`). El mismo error volvió a aparecer, por el mismo motivo, en varios
puntos de `examples/project_stage53_1/Main.ns` (por ejemplo
`TimeSpan.FromSeconds(1.5).Ticks`) — se corrigió sistemáticamente en todo el archivo, no solo en
el primer punto donde apareció.

## Paso 12 — Escribir el programa de verificación empaquetado

`examples/project_stage53_1/Main.ns`: 25 chequeos secuenciales, cada uno devolviendo un código de
salida distinto si falla (convención ya establecida por `examples/project_stage51/Main.ns`). Ver
sección 11 del documento conceptual para el desglose de qué cubre cada rango de chequeos.

## Paso 13 — Escribir el test dedicado (`tests/unit/Stage53_1TimeTests.cpp`)

Dos funciones de test:

- `testNativeClockCoreOperationsLowerToRuntimeCalls`: fixture mínima escrita a mano que compila
  un método usando las 3 operaciones y confirma en el LLVM IR resultante la presencia de
  `"call i64 @__nativesharp_clock_query_counter"` (y las otras dos).
- `testPackagedExample`: compila el proyecto empaquetado completo a través de todo el pipeline
  real (análisis de cuerpo de método → OOP MIR → LLVM → verify → target machine → objeto COFF →
  verificador COFF), después construye un ejecutable PE real vía
  `Driver::run({"--emit-project-oop-exe", ...})` — el mismo camino que usa el CLI real de `nsc` —
  y lo corre bajo Wine esperando código de salida 0.

### Paso 13a — Falso positivo en una aserción del test

La primera versión de `testNativeClockCoreOperationsLowerToRuntimeCalls` buscaba
`llvm.find("__nativesharp_console_writeline") == npos` para confirmar que el método de prueba NO
llama a `Console.WriteLine`. Falló, porque `declareRuntime()` declara ese símbolo
**incondicionalmente** (esté o no usado) — la sola declaración externa ya contiene el string.
**Fix**: cambiar la búsqueda a `"call void @__nativesharp_console_writeline"` (una llamada real,
no la declaración).

### Paso 13b — `std::string_view` colgante en el propio código del test

`testPackagedExample` originalmente armaba:

```cpp
std::array<std::string_view, 5> args = {..., project.string(), ..., exePath.string()};
```

`project.string()` y `exePath.string()` devuelven `std::string` **temporales** — el
`string_view` apunta a memoria que se destruye al final de la expresión completa, y el `Driver`
recibía texto corrupto (basura binaria en el mensaje de error, en vez de la ruta real). **Fix**:
introducir locales nombrados primero —

```cpp
const std::string projectText = project.string();
const std::string exeText = exePath.string();
```

— y recién ahí construir el arreglo de `string_view` sobre esos locales, que sí viven hasta el
final del test. Este es exactamente el mismo patrón que ya usa el helper `emitExecutable()` de
`Stage52_4OwnedCleanupRuntimeTests.cpp` — el bug era no haberlo replicado desde el principio.

## Paso 14 — Registrar el target en `CMakeLists.txt`

```cmake
add_executable(nsc_stage53_1_time_tests tests/unit/Stage53_1TimeTests.cpp)
target_link_libraries(nsc_stage53_1_time_tests PRIVATE nsc_driver nsc_linker nsc_runtime nsc_llvm nsc_frontend nsc_target)
target_compile_features(nsc_stage53_1_time_tests PRIVATE cxx_std_20)
target_compile_definitions(nsc_stage53_1_time_tests PRIVATE NSC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
nsc_enable_warnings(nsc_stage53_1_time_tests)
add_test(NAME nsc.stage53-1.time COMMAND nsc_stage53_1_time_tests)
```

## Paso 15 — Verificación del test dedicado, aislado

```
$ ./bin/tests/nsc_stage53_1_time_tests
PASS: 25 checks
```

## Paso 16 — Recuento final contra la línea de base certificada

Reconfirmado en este mismo momento, con traza real (no reconstruida):

```
$ ctest -R "stage53" --output-on-failure
Test project /tmp/ns51/work/build
    Start 73: nsc.stage53-1.time
1/1 Test #73: nsc.stage53-1.time ...............   Passed    1.17 sec

100% tests passed, 0 tests failed out of 1
```

```
$ ./bin/tests/nsc_stage53_1_time_tests
PASS: 25 checks
```

```
$ ctest --output-on-failure -j$(nproc)
...
100% tests passed, 0 tests failed out of 211

Total Test time (real) =   8.79 sec
```

211/211 — los 210 de la línea de base del Paso 0 más el nuevo `nsc.stage53-1.time`, sin ninguna
regresión.

## Cierre formal de esta ronda

Stage 53.1 queda implementado, wireado en las 4 capas del compilador más la capa de runtime,
verificado con un test dedicado que ejecuta el pipeline real de principio a fin (incluida
ejecución real bajo Wine del PE generado), y sin ninguna regresión sobre la suite existente. Los 5
errores encontrados durante el desarrollo (timeout de build, whitelist de símbolos del linker en 5
archivos, dos violaciones de la regla de receptor direccionable de Stage 40, un falso positivo de
test, y un `string_view` colgante) quedan documentados acá con su causa raíz y su fix — ninguno
quedó parchado sin entender por qué pasaba.
