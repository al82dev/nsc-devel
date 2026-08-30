# 53.2b — Implementación paso a paso: Environment (variables)

Acompaña a `53_2b_nativesharp_environment_variables.md`. Mismo formato que los documentos paso a
paso anteriores de Stage 53: el "cómo", en el orden real en que sucedió, con comandos y salidas
reales.

## Paso 0 — Confirmar la base

```
$ ctest --output-on-failure -j$(nproc)
...
100% tests passed, 0 tests failed out of 212
```

## Paso 1 — Confirmar que String es siempre reference type sin terminador nulo

Antes de escribir el mecanismo de asignación dinámica, se confirmó (releyendo
`lowerStringConcat`/`buildConsoleWriteLine`) que el layout de `NativeSharp.Core.String` es
exactamente `{ i64 length; i8 data[length] }` — sin ningún byte de terminador, y que la longitud
siempre se lee directamente de los primeros 8 bytes del puntero (sin GEP), y los datos empiezan
en el offset 8 (con GEP de un solo índice sobre un puntero `i8`). Esto confirmó que CUALQUIER
llamada a una API de Win32 que espere `LPCSTR` (terminado en null) va a necesitar una copia
intermedia — no hay atajo.

## Paso 2 — Confirmar que `__nativesharp_alloc` no es directamente reutilizable desde el runtime

Se investigó si el módulo de runtime podía simplemente llamar al símbolo `__nativesharp_alloc`
(la función que el módulo de PROGRAMA usa para pedir memoria) para reservar el `String` de
resultado. Se confirmó que, aunque `buildAllocate` SÍ define ese símbolo dentro del propio módulo
de runtime, su `LLVMValueRef` no se propaga fuera de esa función — hubiera hecho falta buscarlo
por nombre (`LLVMGetNamedFunction`) o cambiar la firma de `buildAllocate` para devolverlo. Se optó
por el camino más simple y consistente con el resto del archivo: pasar `getProcessHeap`/
`heapAlloc` (que YA se threadean como parámetros a casi todas las funciones `build*` de este
archivo) directamente a las dos funciones nuevas, e inlinear ahí las mismas dos líneas
(`GetProcessHeap` + `HeapAlloc`) que `buildAllocate` ya usa — sin necesidad de tocar
`buildAllocate` ni de buscar símbolos por nombre.

## Paso 3 — Elegir el patrón de bucle: replicar `buildPuts`, no inventar uno nuevo

Se buscó, dentro del propio `WindowsRuntimeBuilder.cpp`, algún bucle existente para usar como
plantilla, en vez de diseñar uno desde cero. `buildPuts` (la implementación de `puts` para
diagnóstico de C, usada por el manejo de errores no capturados) tiene exactamente el bucle
`measure`/`advance`/`measured` necesario para CONTAR la longitud de un string terminado en null
— usando un slot de índice alocado en el stack (`indexSlot`), no un nodo phi. Se reutilizó ese
mismo esqueleto de bloques (`entry`/`loop`/`body`/`done`) para el bucle nuevo de COPIA
(`buildNullTerminatedCopy`), agregando la escritura del byte en cada vuelta.

## Paso 4 — Escribir las 4 capas del compilador, reutilizando el bloque compartido de String

A diferencia de Stage 53.1/53.2 (donde cada operación nueva necesitó su propio bloque de
creación de nodo HIR), esta ronda reutilizó el bloque YA EXISTENTE que maneja
`StringConcat`/`StringEquals`/`StringSubstring`/`StringFromCharArray` (`ProjectOopHirModel.cpp`)
-- ese bloque ya sabe iterar sobre una cantidad variable de operandos string, aplicar
conversiones implícitas, y no necesita receptor (`this`) porque estas son operaciones estáticas.
Se agregaron `EnvironmentGetVariable`/`EnvironmentSetVariable` a esa misma condición, al mismo
`switch` de conteo de operandos esperados (1 para Get, 2 para Set, igual que `StringSubstring`
usa 3 y `StringFromCharArray` usa 1), y al mismo `switch` de selección de `ProjectOopHirNodeKind`.
Esto significó tocar MENOS código nuevo en la capa HIR que en Stage 53.1/53.2 (ningún bloque
nuevo, solo extensiones de tres condiciones existentes), porque la forma de la operación
("N argumentos string, sin receptor") ya tenía una plantilla probada.

Las capas 1 (`ProjectMethodBodyModel`), 3 (`ProjectOopMirModel`) y 4 (`ProjectOopLlvmLowerer`) sí
necesitaron bloques nuevos, siguiendo el patrón establecido en las rondas anteriores (bloque de
reconocimiento de clase+firma; conversión HIR→MIR trivial; `case` de lowering con guard de error).

## Paso 5 — Primera compilación: sin errores

```
$ cmake --build . -j$(nproc)          # (con timeout extendido)
...
[100%] Built target nsc_stage53_2b_environment_variables_tests   # (una vez agregado el target -- ver Paso 9)
```

Compiló limpio en el primer intento, incluso con el código de bucle manual en LLVM IR crudo — la
parte más intrincada de C++ escrita en todo Stage 53 hasta ahora.

## Paso 6 — El bug: 11 tests fallando, ninguno relacionado con Environment

```
$ ctest --output-on-failure -j$(nproc)
...
95% tests passed, 11 tests failed out of 212

The following tests FAILED:
	 44 - nsc.stage45.linker-pe-eh (Failed)
	 45 - nsc.stage46.native-libraries (Failed)
	 46 - nsc.stage47.host-c-abi (Failed)
	 47 - nsc.stage48_1.c-header (Failed)
	 48 - nsc.stage48_2.library-metadata (Failed)
	 49 - nsc.stage48_3.c-host (Failed)
	 50 - nsc.stage48_4.cpp-host (Failed)
	 51 - nsc.stage48_5.csharp-pinvoke-host (Failed)
	 52 - nsc.stage48_6.cppcli-stage48-closure (Failed)
	 96 - nsc.driver (Failed)
	205 - nsc.emit-exe-o2 (Failed)
```

Primera reacción importante: NINGUNO de estos nombres de test menciona `Environment` — la lista
mezcla tests de Stage 45 (excepciones/EH), 46 (bibliotecas nativas), 47-48 (interop C/C++/C#),
`driver`, y `emit-exe-o2`. Esto fue la primera pista de que el bug no estaba en la lógica de
`GetEnvironmentVariable`/`SetEnvironmentVariable` en sí, sino en algo compartido por TODOS los
ejecutables.

Reproducción aislada, para tener el mensaje completo sin el ruido de una corrida paralela:

```
$ ctest -R "^nsc.emit-exe-o2$" --output-on-failure
...
nsc: error: Windows executable generation failed: lld-link failed with process status 256:
lld-link: error: undefined symbol: memcpy
>>> referenced by /tmp/nativesharp-stage20-837981916882-0/runtime.obj:(__nativesharp_environment_get_variable)
>>> referenced by /tmp/nativesharp-stage20-837981916882-0/runtime.obj:(__nativesharp_environment_set_variable)
>>> referenced by /tmp/nativesharp-stage20-837981916882-0/runtime.obj:(__nativesharp_environment_set_variable)
```

La pista clave: `memcpy` — un símbolo que mi código nunca llamó explícitamente — referenciado
desde `runtime.obj`, específicamente desde mis dos funciones nuevas. Y el test que reproduce el
error de forma más directa es justamente `nsc.emit-exe-o2` — el nombre ya insinúa la variable
relevante: el nivel de optimización.

## Paso 7 — Diagnóstico: LoopIdiomRecognize + un escaneo que solo mira un módulo

Se confirmó corriendo el mismo test AISLADO (no en paralelo) primero, para descartar una simple
colisión de directorio temporal entre tests paralelos:

```
$ ctest -R "^nsc.stage53-2.environment$" --output-on-failure
...
100% tests passed, 0 tests failed out of 1
```

Pasó solo — pero la suite COMPLETA en paralelo seguía fallando de forma reproducible (confirmado
corriendo `ctest -j$(nproc)` dos veces seguidas, mismo resultado ambas veces), descartando que
fuera flakiness de concurrencia y confirmando que es una condición real, disparada por CUALES
tests corren a qué nivel de optimización (mi propia ronda 53.2 usaba `-O0` para su test
dedicado, por eso nunca lo disparó; `nsc.emit-exe-o2` sí usa `-O2` explícitamente, igual que
varios de los tests de Stage 45-48).

Se grepeó `src/Runtime/WindowsRuntimeBuilder.cpp` en busca de bucles existentes (Paso 3) y se
confirmó que NINGUNO copiaba bytes antes de esta ronda (`buildPuts` solo cuenta, nunca copia) --
mi bucle de `buildNullTerminatedCopy` es el primero en todo el módulo de runtime que copia bytes
uno por uno en un `LLVMBuildStore` dentro de un bucle. Eso llevó a sospechar de
LoopIdiomRecognize (el pase de LLVM que reconoce ese patrón y lo reemplaza por
`@llvm.memcpy`), que corre a partir de `-O1` pero no a `-O0` -- exactamente la asimetría
observada.

Se grepeó el árbol completo por `"@llvm.memcpy"` para encontrar dónde el compilador YA maneja
este intrínseco:

```
$ grep -rl "@llvm.memcpy" src/
src/Linker/WindowsLibraryBuilder.cpp
src/Linker/WindowsExecutableBuilder.cpp
```

Y se leyó el código relevante en `WindowsExecutableBuilder.cpp`:

```cpp
const std::string programText = programModule.text();
...
if (programText.find("@llvm.memcpy") != std::string::npos)
{
    ucrtSymbols.push_back("memcpy");
}
```

Causa raíz confirmada: `programText` viene SOLO de `programModule` (el módulo del código de
usuario) -- nunca de `runtimeModule` (el módulo separado que construye
`WindowsRuntimeBuilder.cpp`, ver `53_1_nativesharp_time.md` sección 4 para por qué son dos
módulos distintos). Cuando LoopIdiomRecognize reescribió mi bucle DENTRO del módulo de runtime,
ese `@llvm.memcpy` nunca entró en el texto que este código escanea, así que `"memcpy"` nunca se
agregó a la lista de símbolos a importar de `ucrtbase.dll` -- y el link falló.

## Paso 8 — El fix, y por qué afectaba tests sin relación con Environment

El fix, en `WindowsExecutableBuilder.cpp`:

```cpp
const std::string programText = programModule.text() + runtimeModule->text();
```

Y el mismo fix, con la misma causa raíz, en `WindowsLibraryBuilder.cpp` (la ruta de construcción
de DLLs, función `buildDynamic`), que tenía el mismo patrón de escaneo con el mismo defecto,
aunque ningún test lo había disparado todavía.

El motivo por el que el bug afectaba tests SIN relación con `Environment` (Stage 45, 46, 47,
48_1-48_6, driver): el módulo de runtime se construye COMPLETO para todo ejecutable, siempre --
`buildInternal()` no construye solo las funciones que el código de usuario efectivamente llama,
construye TODAS (incluidas mis dos funciones nuevas), porque el módulo de runtime es una unidad
de compilación fija, compartida por cualquier programa NativeSharp. Una vez que
`Environment.GetEnvironmentVariable`/`SetEnvironmentVariable` existen en el runtime, CUALQUIER
ejecutable compilado a `-O1` o superior enlaza contra un `runtime.obj` que contiene la llamada a
`memcpy` idiom-reconocida -- use o no use esas dos funciones el código del usuario.

## Paso 9 — Verificación del fix: reproducir el error, después confirmar que desaparece

```
$ cmake --build . -j$(nproc)
...
$ ctest --output-on-failure -j$(nproc)
...
100% tests passed, 0 tests failed out of 212

$ ctest -j$(nproc)        # segunda corrida, para descartar que fuera intermitente
...
100% tests passed, 0 tests failed out of 212
```

212/212 dos veces seguidas -- el fix resuelve la causa raíz, no la esconde.

## Paso 10 — Verificación manual del round-trip real, en O0 y en O2

```
$ nsc --emit-project-oop-exe Probe.nsproj -O0 -o Main_O0.exe
project OOP PE executable emission: ok
$ wine64 Main_O0.exe; echo $?
2

$ nsc --emit-project-oop-exe Probe.nsproj -O2 -o Main_O2.exe
project OOP PE executable emission: ok
$ wine64 Main_O2.exe; echo $?
2
```

El código `2` en el primer intento vino de un chequeo `value.Length != 17` en el propio programa
de prueba -- una investigación rápida (`return value.Length;` en vez de comparar) mostró que
`"hola_nativesharp".Length` es en realidad `16`, no `17`. Era un error de conteo en el PROPIO
guion de prueba, no un bug del compilador -- confirmado independientemente con
`python3 -c "print(len('hola_nativesharp'))"` → `16`. Corregido el número esperado en el script
de prueba (no en el compilador), la verificación completa (variable inexistente, round-trip
Set+Get, y `Exit(77)` como paso final) pasó en ambas configuraciones:

```
$ wine64 Main_O0.exe; echo $?
77
$ wine64 Main_O2.exe; echo $?
77
```

## Paso 11 — Escribir el programa de verificación empaquetado

`examples/project_stage53_2b/Main.ns`: 8 chequeos (variable inexistente; round-trip básico
verificado por longitud Y contenido; sobreescribir con un valor más largo; sobreescribir con uno
más corto; un nombre de variable largo; `Exit(0)` como mecanismo final). Cada longitud esperada
en el archivo se verificó con Python antes de escribirla (`len("un_valor_bastante_mas_largo..."`
→ 43, no 44 como se había escrito la primera vez) -- la misma disciplina de "no confiar en un
conteo mental de caracteres" aplicada consistentemente después del tropiezo del Paso 10.

Verificado aparte, en ambas configuraciones, con el mismo camino real:

```
$ nsc --emit-project-oop-exe Stage53_2bEnvironmentVariables.nsproj -O0 -o Main_O0.exe
$ wine64 Main_O0.exe; echo $?
0
$ nsc --emit-project-oop-exe Stage53_2bEnvironmentVariables.nsproj -O2 -o Main_O2.exe
$ wine64 Main_O2.exe; echo $?
0
```

## Paso 12 — Escribir el test dedicado: la primera vez que se prueban DOS niveles de optimización

`tests/unit/Stage53_2bEnvironmentVariablesTests.cpp` sigue el mismo esqueleto de todos los
archivos de test de Stage 53 (mismos helpers duplicados), pero factoriza la parte de
"compilar + linkear + correr bajo Wine" en una función parametrizada por nivel de optimización
(`testPackagedExampleAtOptimizationLevel`), invocada dos veces -- una con `O0`/`"-O0"`, otra con
`O2`/`"-O2"`. Esto es una desviación deliberada del patrón de todos los tests anteriores de Stage
53 (que solo probaban `-O0`): el propio bug de esta ronda (Paso 6-8) es la prueba de que probar
un solo nivel de optimización no es suficiente cuando el código nuevo puede activar un pase de
optimización que no corre a `-O0`.

`CMakeLists.txt`: mismo bloque de 6 líneas que las rondas anteriores, target
`nsc_stage53_2b_environment_variables_tests`, test `nsc.stage53-2b.environment-variables`.

```
$ cmake .
...
-- Build files have been written to: /tmp/ns51/work/build
$ cmake --build . -j$(nproc) --target nsc_stage53_2b_environment_variables_tests
...
[100%] Built target nsc_stage53_2b_environment_variables_tests
$ ./tests/nsc_stage53_2b_environment_variables_tests
PASS: 41 checks
```

## Paso 13 — Recuento final contra la línea de base

```
$ ctest --output-on-failure -j$(nproc)
...
213/213 Test  #75: nsc.stage53-2b.environment-variables ..............   Passed    4.74 sec

100% tests passed, 0 tests failed out of 213

$ ctest -j$(nproc)        # segunda corrida
...
100% tests passed, 0 tests failed out of 213
```

213/213, confirmado estable en dos corridas -- los 212 de la línea de base del Paso 0 más el
nuevo `nsc.stage53-2b.environment-variables`, sin ninguna regresión.

## Cierre formal de esta ronda

Stage 53.2b queda implementada, con un mecanismo nuevo de asignación dinámica de strings desde
el runtime, verificada con un test dedicado que -- por primera vez en Stage 53 -- prueba dos
niveles de optimización en vez de uno, precisamente porque esta ronda encontró un bug real que
solo se manifestaba en uno de ellos. El bug (escaneo de símbolos `ucrtbase` limitado a un solo
módulo de los dos que efectivamente se linkean) se corrigió de raíz en los dos lugares donde el
mismo patrón existía (ejecutables y DLLs), no solo donde el test lo disparó primero. Dos errores
propios de conteo de caracteres en los scripts de prueba (no del compilador) se encontraron y
corrigieron por el camino, verificados independientemente con Python antes de confiar en ellos.
