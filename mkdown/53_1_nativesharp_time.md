# 53.1 — NativeSharp Time (TimeSpan, DateTime, Stopwatch, NativeClock)

## 0. Historial de esta entrega (por qué este documento tiene dos capas)

Después de cerrar Stage 52 de forma definitiva — confirmado no solo en Linux/Wine sino en un
build nativo real sobre Windows por Alberto (`report2.txt`, 210/210) — la pregunta obvia fue
"¿ahora sí que sigue?". El roadmap (`claude/01_roadmap_nativesharp_0_2.md`) apunta a Stage 53:
Time / Filesystem / Environment, un bloque grande que toca el sistema operativo en tres frentes
distintos.

Mi primera propuesta fue encarar Stage 53 completo (53+54+55, incluyendo threading) en una sola
entrega. Alberto frenó eso explícitamente:

> "sabes, es muy osado generar en una entrega arga 53 54 y 55? con sus tests comprobaciones y
> validaciones ademas de lo didactico"

Tenía razón, y en particular threading (Stage 54) es el caso donde ese riesgo es más agudo: los
bugs de concurrencia se esconden atrás de tests que pasan — es exactamente la misma lección que
ya dejó el bug de RAII/try-catch que en su momento parecía un bug del compilador y resultó ser un
bug de Wine (`claude/13_bug_raii_trycatch_confirmado_bug_de_wine.md`). Un test verde bajo Wine no
es prueba suficiente cuando hay concurrencia real de por medio.

Acordamos ir de a una etapa por vez, empezando por 53 sola. Y todavía dentro de "Stage 53 sola",
la investigación mostró que el bloque completo (Time + Filesystem + Environment) suma unas 18+
operaciones núcleo nuevas — otra vez, más de lo que conviene entregar sin verificar en un solo
paso. Siguiendo el propio patrón que ya usó este proyecto para Stage 50 (50.1 a 50.12) y Stage 52
(52-1 a 52-6), esta ronda se recorta a **Stage 53.1: Time únicamente**. Filesystem/Paths (53.3) y
Environment (53.2) quedan para rondas siguientes.

Alberto pidió explícitamente el enfoque de documentación para esta ronda:

> "si, pero enfoque didactico ultra verbosity"

Esa es la instrucción que gobierna este documento y su compañero paso a paso
(`53_1_nativesharp_time_implementacion_paso_a_paso.md`): nada se da por sabido, cada decisión de
diseño se explica con su razón, y cada afirmación de que "esto funciona" está respaldada por una
ejecución real (Wine), no por una suposición.

## 1. Objetivo de Stage 53.1

Dar a NativeSharp la capacidad de:

1. Medir tiempo transcurrido con precisión — el caso de uso central para un motor de juegos
   (delta time por frame, cuánto tarda una fase de carga, profiling).
2. Representar intervalos de tiempo (`TimeSpan`) y hacer aritmética con ellos.
3. Leer el reloj de pared del sistema (`DateTime.UtcNow()`), con la conversión de época correcta.

Deliberadamente **no** es objetivo de esta ronda: descomposición a calendario (año/mes/día/hora),
zonas horarias, ni ningún tipo de "environment" o "filesystem" — eso es 53.2 y 53.3.

## 2. Punto de partida verificado

Antes de escribir una sola línea de Stage 53.1 se confirmó el estado exacto de la base:

- `ctest` en el árbol tal cual quedó tras el cierre de Stage 52: **210/210 passed**.
- Se investigó el mecanismo de "core operation" existente (String, Console) leyendo el código
  real, no la documentación de más alto nivel — para no arrastrar una idea equivocada de cómo
  funciona a una pieza nueva de la arquitectura.
- Se confirmó, leyendo `Parser.cpp`, que **todo método requiere cuerpo salvo los `abstract`** —
  incluidos los marcados `extern`. Esto es la pieza clave que define todo el diseño de abajo.

## 3. Por qué no existe un atajo de FFI genérico (y qué significa eso para Time)

La primera pregunta al empezar Stage 53 fue: "¿puedo simplemente declarar
`[DllImport("kernel32.dll")] static extern long QueryPerformanceCounter();` y listo, como en C#
real?"

La respuesta, confirmada leyendo el compilador (no asumida), es **no**. En NativeSharp:

- El modificador `extern` existe desde Stage 47/48, pero es **unidireccional**: sirve para
  **exportar** una función NativeSharp compilada hacia un host C/C++/C# (`extern "C"` +
  `export`), siempre acompañado de un cuerpo real. No existe la dirección inversa — no hay
  ningún mecanismo de "extern" de **importación** que le diga al compilador "esta función vive en
  una DLL externa, generame el thunk de llamada".
- Por lo tanto, **no hay forma genérica de llamar una API de Win32 arbitraria** desde código
  NativeSharp. Cada primitiva nueva que necesita tocar el sistema operativo tiene que agregarse a
  mano al compilador, una por una, a través del mecanismo de "operación núcleo" que ya usan
  `String` y `Console`.

Esto no es una limitación accidental — es consistente con las restricciones arquitectónicas del
proyected (`claude/04_restricciones_arquitectonicas.md`): NativeSharp prioriza superficie
controlada y verificada por sobre flexibilidad sin fricción. El costo es que agregar "tres
funciones de reloj" implica tocar 8 archivos del compilador; el beneficio es que cada primitiva
que existe está genuinamente probada, no es una superficie sin verificar heredada de una DLL de
terceros.

## 4. El mecanismo de "operación núcleo", explicado capa por capa

Este es el mecanismo central reutilizado (no reinventado) para Stage 53.1. Son 4 capas, cada una
con su propio archivo, que convierten una llamada a un método con cuerpo placeholder en código
máquina real:

### Capa 1 — Reconocimiento en el frontend (`ProjectMethodBodyModel`)

`include/nativesharp/Frontend/ProjectMethodBodyModel.h` declara un enum
`MethodCoreOperationKind` con un valor por cada operación reconocida
(`ClockQueryCounter, ClockQueryFrequency, ClockQuerySystemFileTime` para esta ronda, agregados
junto a los ya existentes `StringConcat`, `ConsoleWriteLine`, etc.).

`src/Frontend/ProjectMethodBodyModel.cpp` hace el reconocimiento real: cuando el compilador
analiza un método, compara el nombre completamente calificado de la clase declarante
(`declaringType.identity.fullyQualifiedName()`) contra el string exacto
`"NativeSharp.Core.Diagnostics.NativeClock"`, exige que sea `static`, que devuelva
`builtin:long`, que no tenga parámetros, y recién ahí despacha por el nombre del método a uno de
los tres `MethodCoreOperationKind` de arriba. Si CUALQUIERA de esos detalles no coincide
exactamente (nombre de clase, `static`, tipo de retorno, cantidad de parámetros), el método se
trata como código NativeSharp normal — y como su cuerpo real es literalmente `return 0;`, eso es
lo que haría en tiempo de ejecución. Esta es la razón por la que la firma tiene que ser exacta:
no hay ningún otro control de sanidad tapando un error de tipeo en la firma.

### Capa 2 — Nodo HIR (`ProjectOopHirModel`)

`include/nativesharp/Frontend/ProjectOopHirModel.h` agrega tres variantes al enum
`ProjectOopHirNodeKind` (`CoreClockQueryCounter`, etc.). `src/Frontend/ProjectOopHirModel.cpp`
agrega el `toString()` correspondiente y, en el punto donde ya existe el bloque que construye el
nodo de `ConsoleWriteLine`, un bloque hermano que construye un nodo HIR sin hijos (estas
operaciones no toman parámetros) con tipo de resultado `"builtin:long"`.

### Capa 3 — Nodo MIR (`ProjectOopMirModel`)

Mismo patrón un nivel más abajo: `include/nativesharp/Frontend/ProjectOopMirModel.h` agrega tres
variantes a `ProjectOopMirNodeKind`; `src/Frontend/ProjectOopMirModel.cpp` agrega el `toString()`
y tres líneas de conversión HIR→MIR (una por operación, cada una trivial: nodo HIR de esta forma
→ nodo MIR de esta forma).

### Capa 4 — Lowering a LLVM (`ProjectOopLlvmLowerer`)

`src/LLVM/ProjectOopLlvmLowerer.cpp` es donde el nodo MIR se convierte en instrucciones LLVM
reales. Para cada operación:

- Se declaran constantes con el nombre exacto del símbolo runtime
  (`__nativesharp_clock_query_counter`, etc.).
- Se agregan campos `LLVMValueRef`/`LLVMTypeRef` para guardar la función declarada.
- `declareRuntime()` declara las tres como funciones externas `i64()` (sin parámetros, devuelven
  `i64`) — esta declaración pasa a formar parte de la cadena de verificaciones de null que ya
  existe (`built = built && ClockQueryCounterFunction_ != nullptr && ...`), así que si el símbolo
  no se puede declarar el compilador falla temprano y explícito, no con un crash tardío.
- El `switch` de lowering agrega un `case` por operación: cada uno simplemente emite una llamada
  sin unwind (`buildNoUnwindCall`) a la función declarada, con un guard que produce un error
  didáctico (`"Stage 53.1 NativeClock.<Nombre> requires its runtime hook"`) si por algún motivo
  el símbolo no estuviera disponible.

### La capa "0" — la implementación real (`WindowsRuntimeBuilder`)

Las cuatro capas de arriba solo emiten una **llamada** a un símbolo externo — la implementación
real de ese símbolo vive en `src/Runtime/WindowsRuntimeBuilder.cpp`, un módulo LLVM **separado**
que se construye de forma independiente y se linkea junto con el código de usuario. Ahí es donde
efectivamente se invoca la API de Win32 real. Ver la sección 5 para el detalle de esa pieza,
porque tiene una decisión de diseño no trivial (el truco del out-parameter).

## 5. El truco de `QueryPerformanceCounter`/`Frequency`: leer un out-parameter como i64

`QueryPerformanceCounter` y `QueryPerformanceFrequency` no son funciones que **devuelven** un
`i64` — son funciones Win32 clásicas que reciben un puntero a `LARGE_INTEGER` (una unión de
`i64`/dos `i32`) y escriben el resultado ahí, devolviendo un `BOOL` de éxito/fracaso.
`GetSystemTimeAsFileTime` tiene la misma forma: recibe un puntero a `FILETIME` (dos `DWORD`
consecutivos, sin padding) y no devuelve nada.

En vez de escribir tres wrappers distintos con manejo de punteros ad-hoc, se factorizó un único
helper compartido:

```
LLVMValueRef buildOutParamI64Call(context, builder, win32Function, win32FunctionType)
```

que hace exactamente esto: `alloca i64` → llama a la función Win32 pasando la dirección de ese
`i64` como el puntero de salida → `load i64` de esa misma dirección → devuelve ese valor. Esto
funciona sin ninguna manipulación de bits porque tanto `LARGE_INTEGER` (unión con un campo `i64`)
como `FILETIME` (dos `DWORD` consecutivos sin padding, que en little-endian x64 son exactamente
los 64 bits bajos+altos de un `i64`) tienen el mismo layout de memoria que un `i64` liso — leer
esos 8 bytes como un entero de 64 bits da directamente el valor combinado correcto.

Sobre el valor de retorno `BOOL` de `QueryPerformanceCounter`/`Frequency`: se ignora
deliberadamente, no por descuido. Microsoft documenta que en Windows XP o posterior estas
funciones **siempre tienen éxito** — no existe, en el hardware moderno soportado, un camino de
fallo real. Agregar una rama de manejo de error para un caso que la propia documentación de
Microsoft garantiza que no ocurre habría sido complejidad sin beneficio. Esto está documentado
explícitamente como decisión, con la cita de la garantía de Microsoft, en el comentario del
código — no es una omisión silenciosa.

## 6. La derivación de la constante de desfase de época (504,911,232,000,000,000)

Win32 mide el tiempo desde 1601-01-01 00:00:00 UTC (la "época FILETIME"). C#/.NET —y NativeSharp,
que replica la misma convención— mide `DateTime.Ticks` desde 0001-01-01 00:00:00 UTC. La
constante que conecta ambas épocas se derivó a mano, sin copiarla de ninguna fuente, para poder
verificarla de forma independiente:

```
Años completos entre 0001-01-01 y 1601-01-01: 1600

Años bisiestos en ese tramo (regla Gregoriana: bisiesto si divisible por 4,
excepto los divisibles por 100 salvo que también sean divisibles por 400):
    floor(1600/4)   = 400   (múltiplos de 4)
  - floor(1600/100) =  16   (excepción: múltiplos de 100 NO son bisiestos...)
  + floor(1600/400) =   4   (...salvo que también sean múltiplos de 400)
  ------------------------
    = 400 - 16 + 4 = 388 años bisiestos

Días totales = 1600 años * 365 días + 388 días bisiestos extra
             = 584,000 + 388
             = 584,388 días

Ticks = días * segundos/día * ticks/segundo
      = 584,388 * 86,400 * 10,000,000
      = 584,388 * 864,000,000,000
      = 504,911,232,000,000,000
```

Ese valor coincide exactamente con la constante interna que .NET real usa para esta misma
conversión (`DateTime.FromFileTimeUtc`) — la coincidencia es la confirmación independiente de que
la cuenta está bien hecha, no una casualidad ni un valor copiado a ciegas.

`DateTime.ns` implementa la ida y la vuelta con esta constante:

- `FromFileTimeUtc(fileTimeTicks)` → `FromTicks(fileTimeTicks + FileTimeEpochOffsetTicks())`
- `ToFileTimeUtc()` → `Ticks - FileTimeEpochOffsetTicks()`

## 7. Decisiones de diseño: struct vs. class, y por qué

**`TimeSpan` y `DateTime` son `struct`** (tipo valor, 8 bytes — un único `long`). Esto encaja
exacto con la regla de ABI de valor de Stage 40 (Windows x64 solo pasa/devuelve por valor
agregados de 1, 2, 4 u 8 bytes sin necesitar el mecanismo de arreglo de salida / `sret`) — viajan
como parámetro y como retorno de la misma forma que cualquier entero nativo, sin ningún costo ni
truco adicional.

**`Stopwatch` es `class`** (tipo referencia), deliberadamente, igual que
`System.Diagnostics.Stopwatch` en C# real. La razón no es estética: si `Stopwatch` fuera struct,
cada copia — pasarlo por valor a una función, asignarlo a otra variable — tendría **su propia
cuenta independiente**, exactamente lo opuesto de la semántica esperada de "el cronómetro que le
paso a esta función para que lo pare cuando termine". Ser `class` evita esa trampa de raíz.

**Métodos, no propiedades get-only, para `TotalSeconds`/`TotalMilliseconds`/etc. en `TimeSpan`.**
La sintaxis `public int X { get { ... } }` está confirmada funcionando sobre **clases** desde
Stage 38 (`examples/project_stage38/App/PropertiesIndexersOperators.ns`), pero ningún ejemplo o
test existente la ejercita sobre un **struct**. En vez de asumir que también funciona ahí y
arriesgar un fallo de compilación tardío en medio de esta ronda, se usó el patrón ya confirmado
para métodos comunes sobre structs (`examples/project_stage40`'s `Word.Sum()`). Queda como
candidato a probar en una ronda futura, sin bloquear esta.

## 8. Por qué `NativeClock` vive en el proyecto `NativeSharp.Core` (y no en uno propio)

`NativeClock` está en el **namespace** `NativeSharp.Core.Diagnostics`, pero físicamente el
archivo `NativeClock.ns` vive en la carpeta/proyecto `lib/NativeSharp.Core/` — junto con
`TimeSpan.ns` y `DateTime.ns`, no junto a `Stopwatch.ns`.

La razón es evitar una referencia circular entre proyectos `.nsproj`, que este proyecto no
soporta (referencias entre `.nsproj` deben ser acíclicas, por diseño). Si `NativeClock` viviera
en un proyecto `NativeSharp.Core.Diagnostics` separado:

- `DateTime.ns` (en `NativeSharp.Core`) necesita llamar a `NativeClock.QuerySystemFileTimeRaw()`
  → `NativeSharp.Core` referenciaría a `NativeSharp.Core.Diagnostics`.
- `Stopwatch.ns` (en `NativeSharp.Core.Diagnostics`) necesita usar `TimeSpan` (en
  `NativeSharp.Core`) → `NativeSharp.Core.Diagnostics` referenciaría a `NativeSharp.Core`.

Eso es un ciclo. La solución fue reconocer que **el namespace de C# y la carpeta/proyecto física
de NativeSharp no tienen por qué coincidir** — exactamente igual que en .NET real, donde
namespace y assembly son conceptos independientes. `NativeClock.ns` se colocó físicamente en el
proyecto `NativeSharp.Core` (que ya existe y ya es una dependencia aceptada de todo lo demás),
mientras que su namespace sigue siendo `NativeSharp.Core.Diagnostics` para mantener la
organización lógica esperada. Con esto, la dependencia queda estrictamente en una sola dirección:
`NativeSharp.Core.Diagnostics` (solo `Stopwatch.ns`) depende de `NativeSharp.Core` (que ahora
incluye `NativeClock`, `TimeSpan`, `DateTime`), y nunca al revés.

## 9. Superficie completa de Stage 53.1

### `NativeSharp.Core.Diagnostics.NativeClock` (3 operaciones núcleo, las únicas que tocan Win32)

- `static long QueryCounterRaw()` → `QueryPerformanceCounter`
- `static long QueryFrequencyRaw()` → `QueryPerformanceFrequency`
- `static long QuerySystemFileTimeRaw()` → `GetSystemTimeAsFileTime`

### `NativeSharp.Core.TimeSpan` (struct, 8 bytes, `Ticks` de 100ns)

Fábricas estáticas: `FromTicks`, `Zero`, `FromMilliseconds`, `FromSeconds`, `FromMinutes`,
`FromHours`, `FromDays`.
Conversión: `TotalMilliseconds`, `TotalSeconds`, `TotalMinutes`, `TotalHours`, `TotalDays`.
Aritmética/comparación: `Add`, `Subtract`, `Negate`, `Equals`, `CompareTo`.

### `NativeSharp.Core.DateTime` (struct, 8 bytes, `Ticks` de 100ns desde 0001-01-01 UTC)

`FromTicks`, `FromFileTimeUtc`, `UtcNow` (estáticos). `ToFileTimeUtc`, `Add`, `Subtract`,
`DifferenceFrom`, `Equals`, `CompareTo` (instancia).

### `NativeSharp.Core.Diagnostics.Stopwatch` (class, cronómetro de alta resolución)

Constructor (cachea `QueryFrequencyRaw()`). `Start`, `Stop`, `Reset`, `Restart`, `IsRunning`,
`Elapsed` (devuelve `TimeSpan`), `ElapsedMilliseconds`, `ElapsedSeconds`, `Frequency`.
Fábrica: `static owned Stopwatch StartNew()`.

## 10. Qué queda explícitamente fuera de esta ronda, y por qué

- **Descomposición a calendario de `DateTime`** (Year/Month/Day/Hour/Minute/Second): requiere
  aritmética de calendario Gregoriano completa (años bisiestos, longitud de mes variable) — una
  pieza de ingeniería separada que merece su propia revisión cuidadosa, no algo agregado de apuro
  en la ronda de Time. Para el propósito real de NativeSharp (motores de juego: delta time, frame
  pacing, profiling) lo que importa es `Stopwatch`/`TimeSpan`, no el calendario — por eso ese es
  el foco real de esta ronda.
- **Zonas horarias**: fuera de alcance, `DateTime` es UTC únicamente.
- **`Environment` (53.2) y `Filesystem`/`Paths` (53.3)**: quedan para rondas siguientes, por la
  misma decisión de no batchear que motivó separar Stage 53.1 del resto de Stage 53.
- **Threading (Stage 54)**: fuera de alcance de Stage 53 por completo; es la etapa que requiere
  el mayor cuidado de todas (ver sección 0).

## 11. Verificación real, sin inventar cifras

- Compilación completa del árbol tras aplicar todos los cambios: exitosa.
- Se encontraron y corrigieron 5 errores reales durante el desarrollo (detalle completo en el
  documento paso a paso): un timeout de build, un símbolo no definido de linker (whitelist de
  `kernel32Symbols` desactualizada en 5 archivos), dos violaciones de la regla de receptor
  direccionable de Stage 40, una aserción de test con falso positivo, y un bug de
  `std::string_view` colgante en el propio código del test.
- `nsc_stage53_1_time_tests` (el test dedicado de esta ronda): **PASS, 25/25 chequeos**, incluyendo
  ejecución real bajo Wine del ejecutable PE completo generado por el pipeline real
  (`Driver::run({"--emit-project-oop-exe", ...})`), con código de salida `0` (todos los chequeos
  pasaron).
- Suite completa: `ctest --output-on-failure -j$(nproc)` → **100% tests passed, 0 tests failed
  out of 211** (210 preexistentes + 1 nuevo de esta ronda). Ninguna regresión: los 210 tests
  preexistentes siguen pasando exactamente igual que antes, incluidos los que compilan
  transitivamente `NativeSharp.Core` (el glob `**/*.ns` de cada `.nsproj` que ya referenciaba ese
  proyecto ahora también compila `NativeClock.ns`/`TimeSpan.ns`/`DateTime.ns`, y ninguno se
  rompió).
- Los 25 chequeos del programa de verificación cubren: aritmética pura de `TimeSpan` (suma, resta,
  comparación, negación, igualdad — checks 1-9); monotonicidad y positividad de
  `NativeClock.QueryCounterRaw`/`QueryFrequencyRaw` (checks 20-21); exactitud de la conversión
  FILETIME↔Ticks de `DateTime`, acorralando `UtcNow()` entre dos lecturas crudas tomadas justo
  antes y después en vez de comparar contra una fecha fija escrita a mano — así el test no
  depende de qué día se corre (checks 30-33); y la máquina de estados completa de `Stopwatch`
  (Start/Stop/Reset/Restart/IsRunning, más una medición real de tiempo transcurrido positivo tras
  un bucle ocupado — checks 40-47).

## 12. Cierre formal

Stage 53.1 "Time" queda cerrado en este estado: 3 operaciones núcleo nuevas
(`NativeClock.QueryCounterRaw/QueryFrequencyRaw/QuerySystemFileTimeRaw`), `TimeSpan` y `DateTime`
como structs de 8 bytes con superficie completa de aritmética/conversión, `Stopwatch` como clase
de cronómetro de alta resolución, un programa de ejemplo empaquetado con 25 chequeos de
verificación en tiempo de ejecución, y un test dedicado que corre ese programa bajo Wine de
extremo a extremo (compilación real → PE real → ejecución real). Suite completa: 211/211.

Quedan pendientes para rondas futuras, en el orden que dicta el roadmap: Stage 53.2 (Environment),
Stage 53.3 (Filesystem/Paths), y — con especial cuidado, según lo acordado — Stage 54 (Threading).
Ninguna de estas se empieza sin señal explícita de Alberto, siguiendo la misma disciplina de "una
etapa por vez" que motivó este recorte.
