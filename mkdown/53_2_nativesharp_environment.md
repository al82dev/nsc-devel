# 53.2 — NativeSharp Environment, ronda 1 (TickCount64, ProcessorCount, Exit)

## 0. Historial de esta entrega

Continuación directa de Stage 53.1 "Time" (ver `53_1_nativesharp_time.md` para el contexto
completo de por qué Stage 53 se está haciendo sub-ronda por sub-ronda). Con Time cerrado y
verificado (211/211, luego 211/211 de nuevo tras la reconfirmación), Alberto dio la señal para
seguir con un simple **"continua"** — la instrucción implícita, ya establecida en la ronda
anterior, sigue siendo la misma: una etapa por vez, con el mismo enfoque didáctico
ultra-verbose, y sin arrancar Stage 53.3 (Filesystem/Paths) ni Stage 54 (threading) sin nueva
señal explícita.

Dentro de "Environment" (el segundo tercio de Stage 53 según el roadmap), esta ronda se recorta
deliberadamente a un subconjunto todavía más chico que el de Time: **TickCount64,
ProcessorCount, Exit**. Las dos operaciones de variables de entorno
(`GetEnvironmentVariable`/`SetEnvironmentVariable`) quedan explícitamente fuera — ver sección 3
para el motivo técnico exacto, descubierto durante la investigación de esta misma ronda.

## 1. Objetivo de esta ronda

1. `Environment.TickCount64()`: milisegundos desde que arrancó el sistema operativo — un reloj
   monotónico barato, distinto del reloj de pared de `DateTime` y distinto del contador de alta
   resolución de `Stopwatch`.
2. `Environment.ProcessorCount()`: cantidad de procesadores lógicos de la máquina — la pregunta
   que cualquier diseño futuro de pool de threads (Stage 54) va a necesitar responder, y que se
   puede implementar y verificar hoy, sin que exista threading todavía.
3. `Environment.Exit(int)`: terminación inmediata del proceso con un código de salida dado —
   equivalente a `Environment.Exit` en C# real.

## 2. Punto de partida verificado

Antes de escribir código de esta ronda: `ctest` con el árbol tal cual quedó al cerrar Stage
53.1 — **211/211** (reconfirmado con traza fresca, no reconstruida, justo antes de empezar).

## 3. Por qué `GetEnvironmentVariable`/`SetEnvironmentVariable` NO están en esta ronda

Esta fue la decisión de alcance más importante de la ronda, y se tomó **durante** la
investigación, no antes: al diseñar la superficie completa de "Environment", la primera
pregunta fue si `GetEnvironmentVariable(string name) -> string` se podía agregar con el mismo
esfuerzo que las otras tres.

La respuesta es no, y la razón es arquitectónica, no de tiempo: **todas las operaciones núcleo
que existen hasta ahora en NativeSharp (String, Console, NativeClock, y las tres nuevas de esta
ronda) devuelven, como máximo, un escalar de tamaño fijo** (`long`, `int`, `bool`, `void`) o —
en el caso de `String.Concat`/`Substring` — construyen un `NativeSharp.Core.String` cuya
**longitud de salida se puede calcular en tiempo de COMPILACIÓN**, a partir de las longitudes de
las entradas (`Concat`: suma de longitudes; `Substring`: longitud pedida por el caller).

`GetEnvironmentVariable` es distinto: la longitud del valor de la variable de entorno **no se
sabe hasta que Win32 responde en tiempo de EJECUCIÓN** (el patrón real de la API es "llamar una
vez con buffer nulo para que te diga el tamaño necesario, después llamar de nuevo con un buffer
de ese tamaño"). Devolver un `NativeSharp.Core.String` con esa longitud requiere que el
**runtime module** (no el código NativeSharp del usuario) llame a `__nativesharp_alloc` (el
heap allocator crudo que ya existe, confirmado leyendo `WindowsRuntimeBuilder.cpp`, usado
internamente para asignar el layout `{ i64 length; T data[...] }` de cualquier string/array) con
un tamaño que él mismo calculó en tiempo de ejecución, llenar esa memoria con el resultado real
de Win32, y devolver el puntero como si fuera un `String` de NativeSharp recién creado.

Esto es **técnicamente viable** (el allocator ya existe, el layout de `String` ya está
documentado y confirmado — ver `51_nativesharp_complete_string.md`), pero es un tipo de
marshaling genuinamente **nuevo**: ninguna operación núcleo existente construye un objeto
NativeSharp completo (no solo un escalar) directamente desde el runtime module. Meterlo en esta
misma ronda, junto con tres operaciones triviales, habría sido exactamente el tipo de batching
que Alberto pidió evitar desde el principio de Stage 53. Se documenta acá como recorte
deliberado, con el diseño ya investigado, para una ronda futura (53.2b) que le dé a esa pieza el
cuidado dedicado que merece.

Por el mismo motivo tampoco entra `GetCommandLineArgs`: además de la misma asignación dinámica
de strings, requiere parsear las reglas de quoting de la línea de comandos de Windows — el
mismo tipo de problema que ya generó un bug real y documentado en este proyecto
(`claude/15_fix_cmdexe_quoting_tests_host_abi_windows.md`). Dos motivos independientes para
tratarlo con cuidado dedicado, no de apuro.

## 4. Diseño de las tres operaciones de esta ronda

Las tres siguen exactamente el mecanismo de 4 capas ya establecido en Stage 53.1 (frontend → HIR
→ MIR → LLVM, más la implementación real en `WindowsRuntimeBuilder.cpp` — ver
`53_1_nativesharp_time.md` sección 4 para la explicación completa de esas capas, no repetida
acá). Lo nuevo de esta ronda son las formas concretas:

- **`TickCount64()`**: cero argumentos, devuelve `long`. Forma **idéntica** a los tres wrappers
  de `NativeClock` en Stage 53.1 — el mismo molde, reutilizado sin cambios.
- **`ProcessorCount()`**: cero argumentos, devuelve `int` (no `long`). Primera vez que una
  operación núcleo de cero argumentos devuelve un entero de 32 bits en vez de 64 — un cambio
  chico pero real en la plomería (un nuevo par de campos `LLVMValueRef`/`LLVMTypeRef`, un tipo
  de función `i32()` en vez de `i64()`).
- **`Exit(int exitCode)`**: un argumento, devuelve `void`. Misma forma que
  `Console.WriteLine(string)` (Stage 41) — un argumento, sin retorno — con la diferencia de que
  el argumento es un `int` que viaja como valor plano, sin ningún layout de string de por medio.

### La elección de `GetActiveProcessorCount` sobre `GetSystemInfo`

La forma "de manual" de obtener la cantidad de procesadores en Win32 es llamar a `GetSystemInfo`,
que llena un struct `SYSTEM_INFO` de 48 bytes, y leer el campo `dwNumberOfProcessors` al offset
32. Eso se descartó a propósito: hubiera requerido reproducir el layout completo del struct en
LLVM IR (offsets, tamaños, alineación) solo para leer un único campo — la misma clase de
complejidad que el "truco del out-parameter" de Stage 53.1 evitó para `FILETIME`/`LARGE_INTEGER`
al elegir leer 8 bytes como un `i64` liso.

En cambio, `GetActiveProcessorCount(WORD GroupNumber)` — con `GroupNumber = ALL_PROCESSOR_GROUPS`
(`0xFFFF`) — devuelve el conteo total **directamente** como valor de retorno, sin ningún struct
de por medio. El parámetro `GroupNumber` ni siquiera se expone como argumento NativeSharp: se
hardcodea `0xFFFF` dentro del wrapper de runtime, porque los "grupos de procesadores" de Windows
solo importan por encima de 64 procesadores lógicos en una sola máquina — fuera del alcance real
de esta pregunta ("¿cuántos procesadores tiene esta máquina, en total?").

### Por qué `Exit` reutiliza el `ExitProcess` ya declarado, y termina en "unreachable"

`ExitProcess` ya estaba declarado e importado en `WindowsRuntimeBuilder.cpp` desde antes de esta
ronda — lo usan internamente los caminos de aborto/trap del runtime (`buildTrap`, el manejo de
excepciones no capturadas, el final de `main`). No hizo falta declararlo de nuevo: el nuevo
wrapper `buildEnvironmentExit` simplemente reutiliza esa misma función y su tipo, ya presentes
como parámetros de `buildInternal()`.

El wrapper termina con `LLVMBuildUnreachable`, no con un `ret` — exactamente la misma forma que
ya usa `buildTrap` (mismo archivo) para su propio camino de salida. La razón es honesta: Win32
documenta que `ExitProcess` **no retorna control al llamador** — termina el proceso completo ahí
mismo. Emitir un `ret` después de esa llamada describiría una posibilidad que nunca ocurre en la
práctica; `unreachable` es la forma correcta en LLVM de decir "el control nunca llega a este
punto", y permite que el optimizador lo aproveche sin generar código muerto.

## 5. Por qué `Environment` no tiene una capa "amigable" separada como NativeClock

En Stage 53.1, `NativeClock` expone primitivas crudas y `TimeSpan`/`DateTime`/`Stopwatch`
construyen una API más cómoda encima (conversión de ticks a segundos, aritmética, etc.). En esta
ronda de `Environment` no hay esa segunda capa: `TickCount64()`, `ProcessorCount()` y `Exit(int)`
**ya son** la forma final que un caller quiere usar — no hay conversión de unidades ni
aritmética adicional que aporte valor entre la primitiva cruda y la API pública. Agregar una
indirección extra ahí habría sido complejidad sin beneficio, así que las tres operaciones núcleo
viven directamente en la clase pública `NativeSharp.Core.Environment`, sin una clase "Raw" por
debajo.

## 6. Verificación manual antes de escribir el test dedicado

Antes de escribir el archivo de test C++, se verificaron las tres operaciones a mano, compilando
un programa de prueba directamente con `nsc --emit-project-oop-exe` y corriéndolo bajo Wine —
exactamente el mismo camino que corre un usuario real de `nsc`:

```
$ ./bin/nsc --emit-project-oop-exe /tmp/probe/Probe.nsproj -O0 -o /tmp/probe/Main.exe
project OOP PE executable emission: ok
...
imports: KERNEL32.dll KERNELBASE.dll ucrtbase.dll

$ WINEDEBUG=-all timeout 20 /usr/lib/wine/wine64 /tmp/probe/Main.exe
$ echo $?
42
```

El programa de prueba llamaba `Environment.Exit(42)` como último paso — el código de salida `42`
observado bajo Wine confirma, en un solo número, que las tres operaciones funcionan de punta a
punta: `TickCount64()`/`ProcessorCount()` no lanzaron ningún error (si hubieran fallado, el
programa habría retornado 1 o 2 antes de llegar a `Exit`), y `Exit(42)` efectivamente terminó el
proceso con el código pedido (si `Exit` no hubiera funcionado, el programa habría seguido a la
línea `return 99;`, y el código de salida observado habría sido 99, no 42).

## 7. Superficie completa de esta ronda

### `NativeSharp.Core.Environment` (class)

- `static long TickCount64()` → `GetTickCount64`.
- `static int ProcessorCount()` → `GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)`.
- `static void Exit(int exitCode)` → `ExitProcess` (reutiliza el símbolo ya importado).

## 8. Qué queda explícitamente fuera de esta ronda, y por qué

- **`GetEnvironmentVariable`/`SetEnvironmentVariable`**: requieren asignación dinámica de un
  `NativeSharp.Core.String` desde el runtime module — un tipo de marshaling nuevo, viable pero
  no trivial, que merece su propia ronda (53.2b). Ver sección 3.
- **`GetCommandLineArgs`**: además de lo anterior, requiere parsear el quoting de línea de
  comandos de Windows — el mismo tipo de problema que ya generó un bug real documentado en este
  proyecto. Ver sección 3.
- **`MachineName`/`UserName`/`OSVersion`**: todas devuelven strings de longitud dinámica —
  mismo motivo que `GetEnvironmentVariable`, mismo recorte.
- **Stage 53.3 (Filesystem/Paths) y Stage 54 (threading)**: fuera de alcance de Stage 53.2 por
  completo, sin cambios respecto a lo ya acordado en la ronda de Time.

## 9. Verificación real, sin inventar cifras

- Compilación completa del árbol: exitosa en el primer intento, sin ningún error de compilación
  en las 8 capas del compilador tocadas (a diferencia de Stage 53.1, que tuvo 5 errores reales
  durante el desarrollo — esta ronda no tuvo ninguno, ver el documento paso a paso para el
  detalle de por qué).
- `nsc_stage53_2_environment_tests`: **PASS, 23/23 chequeos**, incluyendo ejecución real bajo
  Wine del ejecutable PE completo generado por el pipeline real
  (`Driver::run` con `--emit-project-oop-exe`), con código de salida `0`.
- Suite completa: **100% tests passed, 0 tests failed out of 212** (211 previos + 1 nuevo de
  esta ronda). Cero regresiones, incluida la ampliación de la whitelist `kernel32Symbols` (13→15
  entradas en los 5 archivos ya tocados en Stage 53.1) para agregar `GetTickCount64` y
  `GetActiveProcessorCount` (`ExitProcess` ya estaba en la lista desde antes de Stage 53).

## 10. Cierre formal

Stage 53.2 ronda 1 ("Environment: hechos del proceso") queda cerrada en este estado: 3
operaciones núcleo nuevas (`Environment.TickCount64/ProcessorCount/Exit`), sin capa de API
adicional (las primitivas ya son la API final), un programa de ejemplo empaquetado con 6
chequeos en tiempo de ejecución donde el último chequeo es el propio mecanismo de salida
(`Exit(0)` en vez de `return 0`), y un test dedicado que corre ese programa bajo Wine de extremo
a extremo. Suite completa: 212/212.

Quedan pendientes, en el orden que dicta la disciplina de "una etapa por vez": Stage 53.2b
(variables de entorno y argumentos de línea de comandos, con el diseño de marshaling dinámico ya
investigado en la sección 3 de este documento), Stage 53.3 (Filesystem/Paths), y Stage 54
(threading, con especial cuidado). Ninguna arranca sin señal explícita de Alberto.
