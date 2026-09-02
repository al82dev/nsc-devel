# 53.3 — Implementación paso a paso: Filesystem/Paths, ronda 1

Acompaña a `53_3_nativesharp_filesystem_paths.md`. Mismo formato que los documentos paso a paso
anteriores de Stage 53: el "cómo", en el orden real en que sucedió, con comandos y salidas reales.

## Paso 0 — Confirmar la base

Punto de partida: Stage 53.2b cerrado, `ctest` en **213/213**.

## Paso 1 — Confirmar las firmas exactas de `String.Substring`/`Concat` antes de escribir `Path.ns`

Antes de escribir una sola línea de `Path.ns`, se confirmó con `grep` sobre
`lib/NativeSharp.Core/String.ns` las firmas exactas de los métodos que `Path` iba a usar como
bloques de construcción:

```
$ grep -n "public static string Substring\|public static string Concat\|Substring(\|Concat(" \
    lib/NativeSharp.Core/String.ns | head -5
40:        public static owned string Concat(string left, string right)
50:        public static owned string Substring(string value, int startIndex, int length)
```

Dato importante que este grep confirmó y que no era obvio de memoria: el tipo de retorno
DECLARADO es `owned string`, no `string` a secas — la palabra clave `owned` en el tipo de retorno
es la que le dice al compilador que el caller recibe la propiedad del string (y por lo tanto tiene
que liberarlo eventualmente, vía `raii` + destructor automático, o pasándolo `move()` a otro
lado). Cualquier método de `Path` que construya un string nuevo tiene que declarar su propio
retorno como `owned string`, exactamente igual.

También se confirmó, con el mismo `grep` extendido, las firmas de `IndexOf`/`LastIndexOf`/
`LastIndexOfAny`/`EndsWith`/`StartsWith` — todas ya usadas en algún método existente de
`String.ns` (`Insert`, `Remove`, `TrimStart`, `TrimEnd`), lo que además sirvió como referencia
directa del PATRÓN de código a seguir: variables `raii` para resultados intermedios, y
`return Concat(a, b);` (llamada fresca, sin nombrar el intermedio) en vez de
`return move(resultado);` cuando el último paso es una llamada directa — la regla documentada en
el comentario de cabecera de `String.ns` (líneas 33-35 de ese archivo) es que nombrar un `raii`
local y devolverlo requiere `move(x)` explícito, pero devolver una llamada fresca sin nombrarla no
lo requiere.

## Paso 2 — Confirmar el patrón de proyecto satélite: `NativeSharp.Core.Diagnostics` como plantilla

`Path`/`File`/`Directory` iban a vivir en un proyecto nuevo (`NativeSharp.Core.IO`), no dentro de
`NativeSharp.Core` mismo. Se buscó si ya existía un precedente de "proyecto satélite que
referencia a `NativeSharp.Core`" — sí: `lib/NativeSharp.Core.Diagnostics` (que contiene
`Stopwatch`, agregado en Stage 53.1). Su `.nsproj`:

```xml
<Project>
  <PropertyGroup><Name>NativeSharpCoreDiagnostics</Name></PropertyGroup>
  <ItemGroup>
    <Compile Include="**/*.ns" />
    <ProjectReference Include="../NativeSharp.Core/NativeSharp.Core.nsproj" />
  </ItemGroup>
</Project>
```

Se copió esa estructura exacta para `lib/NativeSharp.Core.IO/NativeSharp.Core.IO.nsproj`, solo
cambiando el `Name` a `NativeSharpCoreIO`.

## Paso 3 — Confirmar el mecanismo de char/string literal para `\` y `/`

`Path.Combine`/`LastSeparatorIndex` necesitan literales de string/char para `\` y `/`. Se
confirmó, leyendo `src/Lexer/Lexer.cpp` (`lexCharacter`/`decodeEscapeByte`), que el escape `\\`
está soportado tanto en literales de char (`'\\'`) como de string (`"\\"`) — y se confirmó con
`grep` que el resto del compilador ya usa escapes de un solo backslash en varios lugares
(`'\t'`, `'\n'`, `case '\\':` en `TokenDumper.cpp`), así que no había ninguna duda real, solo se
verificó antes de escribir código en vez de asumir.

## Paso 4 — Diseñar `Path.ns`: pura NativeSharp, sin ninguna operación núcleo

A diferencia de CUALQUIER otra clase agregada en Stage 53 hasta este punto, `Path` no necesitó
tocar ni una sola de las cuatro capas del compilador. Se escribió directamente como código
NativeSharp normal, usando `String.LastIndexOf`/`Substring`/`Concat`/`EndsWith` — el mismo tipo de
código que un usuario final del compilador escribiría. El helper privado `LastSeparatorIndex`
(dos `LastIndexOf` en vez de un `LastIndexOfAny` con array alocado, ver la sección 3.1 del
documento conceptual) fue la única decisión de diseño no trivial de este paso.

## Paso 5 — Diseñar `File.ns`/`Directory.ns`: cuatro operaciones núcleo, mismo patrón que Environment

Se identificaron las tres API de Win32 necesarias (`GetFileAttributesA`, `DeleteFileA`,
`CreateDirectoryA`) y el patrón de cada una (una sola llamada, sin marshaling de structs, ver
sección 4 del documento conceptual). Se escribieron `File.ns`/`Directory.ns` con cuerpos
placeholder (`return false;`/cuerpo vacío), exactamente el mismo patrón que `Environment.ns`
estableció en 53.2/53.2b.

## Paso 6 — Extender las 4 capas del compilador, reutilizando el bloque HIR compartido de string

Se localizaron los 4 puntos de extensión exactos, releyendo el código que Stage 53.2b había
tocado (el `grep` de `EnvironmentGetVariable`/`EnvironmentSetVariable` en cada archivo dio
exactamente los números de línea necesarios):

1. `include/nativesharp/Frontend/ProjectMethodBodyModel.h` — 4 valores nuevos en
   `MethodCoreOperationKind` (`FileExists`, `FileDelete`, `DirectoryExists`, `DirectoryCreate`).
2. `src/Frontend/ProjectMethodBodyModel.cpp` — un bloque de reconocimiento NUEVO (no una extensión
   del bloque de `Environment`, porque son dos clases distintas —
   `NativeSharp.Core.IO.File`/`NativeSharp.Core.IO.Directory` — así que la condición
   `declaringType.identity.fullyQualifiedName() == "..."` tiene que cubrir ambas por separado) más
   2 líneas en `methodCoreOperationKindName()`.
3. `include/nativesharp/Frontend/ProjectOopHirModel.h` — 4 valores nuevos en
   `ProjectOopHirNodeKind` (`CoreFileExists`, `CoreFileDelete`, `CoreDirectoryExists`,
   `CoreDirectoryCreate`).
4. `src/Frontend/ProjectOopHirModel.cpp` — toString() + EXTENSIÓN del bloque compartido de
   `StringConcat`/`StringEquals`/`StringSubstring`/`StringFromCharArray`/
   `EnvironmentGetVariable`/`EnvironmentSetVariable` (el mismo bloque que 53.2b ya había
   extendido una vez) — sin crear ningún bloque nuevo. Este fue el punto donde se decidió
   reescribir la ternaria anidada de cálculo de operandos esperados como un `if`-chain plano (ver
   sección 5 del documento conceptual) — con 8 `MethodCoreOperationKind` distintos pasando por el
   mismo bloque, la ternaria de 2 niveles ya era el límite de legibilidad razonable.
5. `include/nativesharp/Frontend/ProjectOopMirModel.h`/`.cpp` — 4 valores nuevos en
   `ProjectOopMirNodeKind` + toString() + 4 casos de conversión HIR→MIR triviales (los hijos se
   copian genéricamente por el bucle que corre después del `switch`, igual que toda operación
   núcleo desde Stage 51).
6. `src/LLVM/ProjectOopLlvmLowerer.cpp` — 4 constantes de nombre de símbolo, 8 campos
   `LLVMValueRef`/`LLVMTypeRef` (function + type por cada una de las 4 operaciones), extensión de
   `declareRuntime()` (con un detalle nuevo: ver Paso 7 abajo), 4 `case` de lowering.

## Paso 7 — Un error de compilación real: redeclaración de `i1` en `declareRuntime()`

Al agregar la declaración de tipo para `File.Exists`/`Directory.Exists` (que devuelven `i1`, a
diferencia de `GetEnvironmentVariable` que devuelve `ptr`), se escribió inicialmente:

```cpp
LLVMTypeRef i1 = LLVMInt1TypeInContext(context_);
```

`declareRuntime()` es una sola función larga (no bloques anidados con scope propio), y más abajo
en esa MISMA función ya existía:

```cpp
LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
LLVMTypeRef i1 = LLVMInt1TypeInContext(context_);   // <- ya estaba, para memset/memcpy
```

Esto se detectó ANTES de compilar, releyendo el archivo completo tras la edición (disciplina de
"leer el resultado antes de compilar" en vez de descubrir el error por el mensaje del compilador)
— se vio la redeclaración al hacer un `Read` de las líneas insertadas junto con su contexto
siguiente. Se corrigió renombrando la variable local nueva a `boolType` en vez de `i1`, sin tocar
la declaración preexistente más abajo. Resultado: la primera compilación real (Paso 8) no tuvo
ningún error de este tipo — el error se atrapó y corrigió durante la edición, no durante el build.

## Paso 8 — Bump proactivo de la whitelist de `kernel32Symbols` ANTES del primer compile

Lección aplicada desde Stage 53.2 (originalmente descubierta por fallo en 53.1): se agregaron
`GetFileAttributesA`/`DeleteFileA`/`CreateDirectoryA` a los 5 archivos con la whitelist
(`src/Linker/WindowsLibraryBuilder.cpp`, `src/Linker/WindowsExecutableBuilder.cpp`,
`tests/unit/Stage48_3CHostIntegrationTests.cpp`, `tests/unit/Stage47HostCAbiTests.cpp`,
`tests/unit/Stage48_4CppHostIntegrationTests.cpp`) ANTES de compilar por primera vez, bumpeando el
tamaño del array de 17 a 20 en los cinco.

## Paso 9 — Primera compilación completa: sin errores

```
$ cmake --build . --target nsc -- -j$(nproc)
...
[100%] Linking CXX executable bin/nsc
[100%] Built target nsc
```

Compiló limpio en el primer intento (después de la corrección del Paso 7, que se hizo durante la
edición, no como respuesta a un error de build).

## Paso 10 — Probe manual: Path + File/Directory reales bajo Wine, ANTES de escribir el test dedicado

Siguiendo la disciplina establecida desde Stage 52 (verificar manualmente antes de comprometerse a
un test dedicado), se escribió un programa de prueba independiente
(`/tmp/ns51_probe53_3/Main.ns`, fuera del árbol del repositorio) ejercitando las 6 funciones de
`Path` más `File.Exists`/`Delete` y `Directory.Exists`/`CreateDirectory` contra el sistema de
archivos real:

```
$ nsc --emit-project-oop-exe probe.nsproj -O0 -o probe_O0.exe
project OOP PE executable emission: ok
...
$ ls -la nativesharp_stage53_3_probe_existing.txt   # creado con `touch` antes de correr
-rw-r--r-- 1 root root 0 ... nativesharp_stage53_3_probe_existing.txt
$ WINEDEBUG=-all timeout 20 /usr/lib/wine/wine64 probe_O0.exe; echo "EXIT_CODE=$?"
EXIT_CODE=0
```

Confirmado además, DESDE EL LADO LINUX (no solo por el exit code del programa), que los efectos
en disco fueron reales:

```
$ ls -la nativesharp_stage53_3_probe_existing.txt
ls: cannot access 'nativesharp_stage53_3_probe_existing.txt': No such file or directory   # File.Delete SÍ borró el archivo de verdad
$ ls -ld nativesharp_stage53_3_probe_dir
drwxr-xr-x 2 root root 4096 ... nativesharp_stage53_3_probe_dir   # Directory.CreateDirectory SÍ creó el directorio de verdad
```

Repetido a `-O2` (relevante por el precedente de 53.2b: estas cuatro operaciones reutilizan
`buildNullTerminatedCopy`, el mismo helper cuyo bucle manual causó el bug de `memcpy`/`ucrtbase`
en la ronda anterior):

```
$ nsc --emit-project-oop-exe probe.nsproj -O2 -o probe_O2.exe
...
$ WINEDEBUG=-all timeout 20 /usr/lib/wine/wine64 probe_O2.exe; echo "EXIT_CODE=$?"
EXIT_CODE=0
```

Exit code `0` en ambas configuraciones, primer intento, sin ningún error de `memcpy` — confirma
empíricamente que el fix de 53.2b (escanear ambos módulos LLVM, no solo el de programa) cubre a
cualquier operación futura que reutilice un bucle manual dentro del módulo de runtime, incluida
esta.

## Paso 11 — Suite completa tras la primera compilación: 213/213, sin regresiones

```
$ ctest --output-on-failure -j$(nproc)
...
100% tests passed, 0 tests failed out of 213
```

## Paso 12 — Programa de ejemplo empaquetado y test dedicado

Se escribió `examples/project_stage53_3/Main.ns` con 20 chequeos (ver sección 7 del documento
conceptual para el detalle completo de qué cubre cada uno), y
`tests/unit/Stage53_3FilesystemPathsTests.cpp` siguiendo la estructura de
`Stage53_2bEnvironmentVariablesTests.cpp` (fixture mínimo de lowering + programa empaquetado
probado en `-O0` y `-O2`), con una diferencia de diseño nueva respecto a todos los tests
anteriores de Stage 53: el helper `runUnderWine` de este archivo hace `cd` a un directorio de
trabajo DEDICADO antes de invocar Wine, en vez de heredar el directorio de trabajo de `ctest`
directamente.

**Por qué hizo falta este cambio**: ningún test de Stage 53 anterior necesitaba que el programa
NativeSharp bajo prueba escribiera archivos reales al disco — `Environment`/`NativeClock` solo
leen (o, en el caso de `SetEnvironmentVariable`, escriben al bloque de entorno del proceso, no al
sistema de archivos). Esta ronda es la primera cuyo programa empaquetado CREA un directorio y
BORRA un archivo de verdad. Si el proceso de Wine hereda el directorio de trabajo de `ctest` sin
modificarlo, esos efectos quedarían esparcidos en el directorio desde el que `ctest` se invoca
(potencialmente el directorio de build), lo cual no es hermético ni predecible entre corridas.

El fix: el test crea un directorio dedicado por nivel de optimización
(`root / "run_O0"`, `root / "run_O2"`), lo vacía y recrea antes de cada corrida, escribe ahí el
archivo `nativesharp_stage53_3_existing_file.txt` que el programa espera encontrar (necesario
porque esta ronda todavía no tiene `File.WriteAllText` — ver sección 6 del documento conceptual),
y el comando que invoca Wine hace `cd <directorio> && wine ...` en vez de solo `wine ...`.

## Paso 13 — Registro en CMakeLists.txt, build, y verificación final

```
$ cmake --build . --target nsc_stage53_3_filesystem_paths_tests -- -j$(nproc)
...
[100%] Built target nsc_stage53_3_filesystem_paths_tests
$ ./tests/nsc_stage53_3_filesystem_paths_tests
PASS: 47 checks
$ ./tests/nsc_stage53_3_filesystem_paths_tests   # repetido, para descartar intermitencia
PASS: 47 checks
$ ctest --output-on-failure -j$(nproc)
...
100% tests passed, 0 tests failed out of 214
$ ctest --output-on-failure -j$(nproc)   # repetido
...
100% tests passed, 0 tests failed out of 214
```

47/47 en el test dedicado (dos corridas consecutivas), 214/214 en la suite completa (dos corridas
consecutivas, incluyendo el nuevo test `nsc.stage53-3.filesystem-paths` corriendo en 4.69
segundos — la ejecución real bajo Wine de dos ejecutables, dos veces cada uno entre el fixture
mínimo y el programa empaquetado).

## Cierre

Cero errores de compilador nuevos encontrados en esta ronda (a diferencia de 53.2b, que encontró
el bug real de `memcpy`/`ucrtbase`). El único error real de este round fue de C++, detectado y
corregido durante la edición antes de compilar (Paso 7, redeclaración de `i1`) — no un error de
diseño ni de comportamiento del compilador NativeSharp mismo. Ver `53_3_nativesharp_filesystem_
paths.md` sección 8 para el cierre formal completo.
