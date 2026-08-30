# 53.2 — Implementación paso a paso: Environment (ronda 1)

Acompaña a `53_2_nativesharp_environment.md`. Mismo formato que
`53_1_nativesharp_time_implementacion_paso_a_paso.md`: acá está el "cómo", en el orden real en
que sucedió, con comandos y salidas reales.

## Paso 0 — Confirmar la base

```
$ ctest --output-on-failure -j$(nproc)
...
100% tests passed, 0 tests failed out of 211
```

211/211 — el estado exacto en que quedó Stage 53.1, reconfirmado con una corrida fresca antes de
tocar una sola línea de esta ronda.

## Paso 1 — Investigar el "siguiente candidato obvio": variables de entorno

El primer instinto fue empezar por `GetEnvironmentVariable`/`SetEnvironmentVariable`, por ser lo
más asociado al nombre "Environment". Antes de escribir una sola línea de compilador, se
investigó si el mecanismo de operación núcleo ya soporta devolver un `NativeSharp.Core.String`
de longitud calculada en **tiempo de ejecución** (no de compilación):

- Se releyó `lowerStringConcat`/`lowerStringSubstring` (`ProjectOopLlvmLowerer.cpp`): ambas
  calculan la longitud de salida a partir de valores ya conocidos en tiempo de lowering (longitud
  de las entradas, o el rango pedido) — nunca desde un valor que solo Win32 conoce al momento de
  ejecutar.
- Se confirmó que `__nativesharp_alloc(i64 tamaño) -> ptr` (el heap allocator crudo,
  `src/Runtime/WindowsRuntimeBuilder.cpp`) SÍ acepta un tamaño calculado en tiempo de ejecución —
  técnicamente el mecanismo existe — pero ninguna operación núcleo existente lo usa para construir
  un `String` completo (con su cabecera `{ i64 length; ... }`) desde cero dentro del runtime
  module; siempre es el código NativeSharp del usuario (o una operación núcleo de tamaño
  precomputable) el que arma esa estructura.

Conclusión: viable, pero es un mecanismo nuevo, no una extensión directa del patrón existente.
Se decidió, en este mismo paso, sacar `GetEnvironmentVariable`/`SetEnvironmentVariable` de esta
ronda (documentado en detalle en la sección 3 del documento conceptual) y quedarse con las tres
primitivas que SÍ encajan en el patrón ya probado: `TickCount64`, `ProcessorCount`, `Exit`.

## Paso 2 — Elegir la API de Win32 para "cantidad de procesadores"

Se descartó `GetSystemInfo` (requiere reproducir el layout completo de `SYSTEM_INFO`, 48 bytes,
para leer un único campo `DWORD` al offset 32) a favor de `GetActiveProcessorCount(WORD
GroupNumber)`, que devuelve el conteo directamente como valor de retorno — mismo criterio que
ya guió la elección de `buildOutParamI64Call` en Stage 53.1: preferir la API de Win32 que evita
manipular structs a mano cuando existe una alternativa igual de correcta.

## Paso 3 — Confirmar que `ExitProcess` ya está disponible para reutilizar

Se grepeó `WindowsRuntimeBuilder.cpp` en busca de usos existentes de `ExitProcess` antes de
declarar nada nuevo: ya estaba declarado e importado (usado internamente por `buildTrap`, el
manejo de excepciones no capturadas, y el final de `main`). Confirmado también en la whitelist
`kernel32Symbols` de los 5 archivos del linker/tests: `"ExitProcess"` ya era el primer símbolo de
la lista, agregado en una ronda muy anterior a Stage 53. Esto significa que `Exit(int)` no
necesita ningún import nuevo — solo un wrapper nuevo que reutiliza la función ya declarada.

## Paso 4 — Escribir las 4 capas del compilador + la capa de runtime

Se replicó el mismo patrón exacto de Stage 53.1, capa por capa, en los mismos 8 archivos
(`ProjectMethodBodyModel.h/.cpp`, `ProjectOopHirModel.h/.cpp`, `ProjectOopMirModel.h/.cpp`,
`ProjectOopLlvmLowerer.cpp`, `WindowsRuntimeBuilder.cpp`), agregando siempre un bloque hermano
junto al de `NativeClock`/`ConsoleWriteLine`, nunca reemplazando nada:

- **Capa 1** (`ProjectMethodBodyModel`): 3 valores nuevos de `MethodCoreOperationKind`
  (`EnvironmentTickCount64`, `EnvironmentProcessorCount`, `EnvironmentExit`); bloque de
  reconocimiento para la clase `"NativeSharp.Core.Environment"`, con dos formas de firma
  distintas (cero argumentos con retorno `long`/`int`, y un argumento `int` con retorno `void`).
- **Capa 2** (`ProjectOopHirModel`): 3 valores nuevos de enum; dos bloques de creación de nodo
  (uno para el par cero-argumentos, reutilizando la misma estructura condicional que ya
  distingue `long` de `int` según cuál de las dos operaciones es; otro para `Exit`, copiado casi
  literal del bloque de `ConsoleWriteLine` pero con un argumento `int` en vez de `string`).
- **Capa 3** (`ProjectOopMirModel`): 3 valores nuevos de enum + 3 líneas de conversión HIR→MIR,
  triviales.
- **Capa 4** (`ProjectOopLlvmLowerer`): 3 constantes de nombre de símbolo, 6 campos nuevos, la
  extensión de `declareRuntime()` (declarando `TickCount64Raw`/`ProcessorCountRaw` como funciones
  `i64()`/`i32()` respectivamente y `Exit` como `void(i32)`), y 3 `case` de lowering (los
  primeros dos calcados de los de `NativeClock`; el de `Exit` calcado del de
  `CoreConsoleWriteLine` con un argumento `int` en vez de `string`).
- **Runtime** (`WindowsRuntimeBuilder.cpp`): 3 funciones nuevas
  (`buildEnvironmentTickCount64`/`buildEnvironmentProcessorCount`/`buildEnvironmentExit`), la
  declaración de los 2 imports Win32 nuevos (`GetTickCount64`, `GetActiveProcessorCount` — nótese
  que son solo 2, no 3: `ExitProcess` se reutiliza, ver Paso 3), y su incorporación a la cadena de
  construcción/verificación existente en `buildInternal()`.

## Paso 5 — Whitelist del linker: 2 símbolos nuevos, no 3

Aplicando la lección de Stage 53.1 (donde el mismo olvido costó 16 tests fallando con `undefined
symbol`), se agregaron `GetTickCount64` y `GetActiveProcessorCount` a `kernel32Symbols` en los
mismos 5 archivos tocados la ronda anterior, ANTES de compilar por primera vez, no después de ver
fallar el link:

- `src/Linker/WindowsLibraryBuilder.cpp`
- `src/Linker/WindowsExecutableBuilder.cpp`
- `tests/unit/Stage48_3CHostIntegrationTests.cpp`
- `tests/unit/Stage47HostCAbiTests.cpp`
- `tests/unit/Stage48_4CppHostIntegrationTests.cpp`

(`ExitProcess` no se agregó porque ya estaba desde antes — ver Paso 3.) El tamaño del arreglo
`std::array<std::string_view, N>` se ajustó de 13 a 15 en los 5 archivos, junto con las cadenas.

## Paso 6 — Escribir `Environment.ns`

`lib/NativeSharp.Core/Environment.ns`, con las tres operaciones núcleo como la API pública final
(sin capa "Raw" separada — ver sección 5 del documento conceptual para el porqué), y el
comentario de cabecera documentando explícitamente por qué `GetEnvironmentVariable`/
`GetCommandLineArgs` no están en esta ronda.

## Paso 7 — Primera compilación completa: sin errores

```
$ cmake --build . -j$(nproc)          # (con timeout extendido, igual que Stage 53.1 Paso 8)
...
[100%] Built target nsc_stage53_1_time_tests
```

Compiló limpio en el primer intento, en las 8 capas del compilador tocadas. A diferencia de
Stage 53.1 (5 errores reales encontrados y corregidos durante el desarrollo), esta ronda no tuvo
ningún error de compilación ni de link. La diferencia se explica por dos factores concretos: (1)
el mecanismo de 4 capas ya estaba completamente entendido y probado, no había nada que
reverse-engenieriar por primera vez; y (2) el fix del whitelist de kernel32 se aplicó de
antemano (Paso 5), en vez de descubrirse por un fallo de link como pasó la ronda anterior.

## Paso 8 — Verificación manual de las 3 operaciones, antes del test dedicado

Se escribió un programa mínimo de prueba y se compiló/corrió directamente, con el mismo camino
real que usa un usuario de `nsc`:

```
$ ./bin/nsc --emit-project-oop-exe /tmp/probe53_2/Probe.nsproj -O0 -o /tmp/probe53_2/Main.exe
project OOP PE executable emission: ok
entry-symbol: __nsc_method_Probe532__Program__Main
machine: AMD64 (0x8664)
format: PE32+
subsystem: Windows CUI
optimization: O0
sections: 5
imports: KERNEL32.dll KERNELBASE.dll ucrtbase.dll

$ WINEDEBUG=-all timeout 20 /usr/lib/wine/wine64 /tmp/probe53_2/Main.exe
$ echo $?
42
```

El programa de prueba llamaba `Environment.Exit(42)` al final. El código de salida `42`
observado confirma, de punta a punta, que las tres operaciones funcionan: si `TickCount64()` o
`ProcessorCount()` hubieran fallado, el programa habría retornado 1 o 2 antes de llegar a `Exit`;
si `Exit(42)` no hubiera terminado el proceso de verdad, la ejecución habría caído a la línea
`return 99;` y el código observado habría sido 99, no 42.

## Paso 9 — Escribir el programa de verificación empaquetado

`examples/project_stage53_2/Main.ns`: 6 chequeos, con una particularidad respecto al patrón de
Stage 53.1 — el ÚLTIMO chequeo no usa `return 0` sino `Environment.Exit(0)`, seguido de
`return 77` como red de seguridad. Esto convierte el mecanismo de salida exitosa en un chequeo en
sí mismo: si `Exit` estuviera roto (cayera al `return 77` en vez de terminar el proceso), el
código de salida observado sería 77, inmediatamente distinguible de 0 y de cualquier otro
chequeo (1-4) de este mismo archivo. Verificado aparte, con el mismo camino real:

```
$ ./bin/nsc --emit-project-oop-exe examples/project_stage53_2/Stage53_2Environment.nsproj \
    -O0 -o /tmp/probe53_2/Stage53_2.exe
project OOP PE executable emission: ok
...

$ WINEDEBUG=-all timeout 20 /usr/lib/wine/wine64 /tmp/probe53_2/Stage53_2.exe
$ echo $?
0
```

## Paso 10 — Escribir el test dedicado y registrarlo en CMake

`tests/unit/Stage53_2EnvironmentTests.cpp`, calcado de `Stage53_1TimeTests.cpp` (mismos helpers
`writeFile`/`writeProject`/`commandAvailable`/`wineCommand`/`runUnderWine`, duplicados a
propósito — ver la convención ya documentada en Stage 53.1 sobre duplicación entre archivos de
test independientes). Dos diferencias notables respecto al fixture mínimo de Stage 53.1:

- El fixture de `testEnvironmentCoreOperationsLowerToRuntimeCalls` NO ejercita `Exit()` (a
  diferencia de `TickCount64`/`ProcessorCount`, que sí aparecen en el `Run()` de la fixture):
  como `Exit` termina el proceso, una llamada real a él en un fixture puramente estructural haría
  el resto del código después de esa llamada inalcanzable y sin sentido de probar ahí. La
  cobertura real de `Exit()` es responsabilidad exclusiva del programa empaquetado (Paso 9), que
  SÍ lo ejecuta de verdad bajo Wine — es justamente el punto de esa pieza.
- La aserción de `ProcessorCount()` busca `"call i32 @__nativesharp_environment_processor_count"`
  (no `i64`, a diferencia de todas las aserciones de Stage 53.1) — reflejo directo de que esta es
  la primera operación núcleo de cero argumentos que devuelve un entero de 32 bits.

`CMakeLists.txt`: mismo bloque de 6 líneas que Stage 53.1, target
`nsc_stage53_2_environment_tests`, test `nsc.stage53-2.environment`. Reconfiguración de CMake
necesaria (target nuevo):

```
$ cmake .
...
-- Build files have been written to: /tmp/ns51/work/build
```

## Paso 11 — Build del target nuevo y verificación aislada

```
$ cmake --build . -j$(nproc) --target nsc_stage53_2_environment_tests
...
[100%] Built target nsc_stage53_2_environment_tests

$ ./tests/nsc_stage53_2_environment_tests
PASS: 23 checks
```

Compiló limpio, sin ningún error — segunda confirmación de que el mecanismo ya maduro de Stage
53.1 se reutilizó sin sorpresas esta vez.

## Paso 12 — Recuento final contra la línea de base

```
$ ctest --output-on-failure -j$(nproc)
...
212/212 Test  #74: nsc.stage53-2.environment .........................   Passed    1.04 sec

100% tests passed, 0 tests failed out of 212

Total Test time (real) =   9.18 sec
```

212/212 — los 211 de la línea de base del Paso 0 más el nuevo `nsc.stage53-2.environment`, sin
ninguna regresión.

## Cierre formal de esta ronda

Stage 53.2 ronda 1 queda implementada, wireada en las 4 capas del compilador más la capa de
runtime, verificada con un test dedicado que ejecuta el pipeline real de principio a fin
(incluida ejecución real bajo Wine, donde el propio `Exit(0)` es el mecanismo de verificación
final), y sin ninguna regresión sobre la suite existente. A diferencia de Stage 53.1, esta ronda
no encontró ningún error real durante el desarrollo — la decisión de aplicar de antemano la
lección del whitelist de kernel32 (Paso 5) y de recortar de entrada las dos operaciones que
requerían un mecanismo de marshaling nuevo (Paso 1) son, en retrospectiva, la razón concreta de
por qué.
