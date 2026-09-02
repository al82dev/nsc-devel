# 53.3 — NativeSharp Filesystem/Paths, ronda 1: Path, File.Exists/Delete, Directory.Exists/CreateDirectory

## 0. Historial de esta entrega

Continuación directa de Stage 53.2b ("Environment (variables)"), que cerró con **213/213** tests
y un bug real de infraestructura de linking (el escaneo de símbolos `memcpy`/`ucrtbase.dll` que
solo miraba el módulo de programa, nunca el de runtime) encontrado y corregido de raíz. Alberto
dio la señal de encarar el tercer tercio de Stage 53 con **"53.3"**.

Esta ronda cierra "Filesystem/Paths" en su forma más simple y útil: manipulación de paths (pura
NativeSharp, sin tocar Win32 en absoluto) más las cuatro preguntas/acciones más básicas que se le
pueden hacer al sistema de archivos real — ¿existe este archivo?, ¿existe este directorio?,
borrar un archivo, crear un directorio. Deliberadamente NO incluye leer o escribir el
**contenido** de un archivo (`ReadAllText`/`WriteAllText`/`AppendAllText`) — ver la sección 6 para
por qué eso queda para una ronda futura `53.3b`, exactamente el mismo patrón de "empezar simple,
diferir lo complejo" que ya funcionó en 53.2/53.2b.

## 1. Objetivo de esta ronda

1. `NativeSharp.Core.IO.Path` — clase de utilidades de manipulación de **texto** de paths, sin
   ninguna operación núcleo nueva:
   - `Combine(string, string) -> string`
   - `GetFileName(string) -> string`
   - `GetDirectoryName(string) -> string`
   - `GetExtension(string) -> string`
   - `GetFileNameWithoutExtension(string) -> string`
   - `DirectorySeparatorChar() -> char`, `AltDirectorySeparatorChar() -> char`
2. `NativeSharp.Core.IO.File` — dos operaciones núcleo nuevas:
   - `Exists(string path) -> bool`
   - `Delete(string path) -> void`
3. `NativeSharp.Core.IO.Directory` — dos operaciones núcleo nuevas:
   - `Exists(string path) -> bool`
   - `CreateDirectory(string path) -> void`

## 2. Punto de partida verificado

`ctest` con el árbol tal cual quedó al cerrar Stage 53.2b: **213/213**.

## 3. Path: la primera clase de Stage 53 que NO es una operación núcleo

Todas las clases que Stage 53 vino agregando hasta ahora — `NativeClock` (53.1), `Environment`
(53.2/53.2b) — existen exclusivamente para envolver llamadas a Win32 que NativeSharp no puede
expresar de ninguna otra forma: el compilador las reconoce por clase+nombre+firma exactos y
sustituye la llamada entera por lowering nativo a LLVM (ver el comentario de cabecera de
`NativeClock.ns` para la explicación completa del mecanismo).

`Path` rompe ese patrón a propósito: manipular el TEXTO de un path — encontrar el separador,
cortar la extensión, unir dos segmentos — es pura manipulación de strings, y
`NativeSharp.Core.String` ya tiene todo lo necesario desde Stage 51
(`Substring`/`Concat`/`EndsWith`/`LastIndexOf`). No hay ninguna razón para hablar con Win32 para
esto — Win32 recién entra en juego cuando la pregunta es algo que solo el sistema operativo puede
responder de verdad ("¿existe este archivo, ahora mismo, en el disco?"), y esa es exactamente la
frontera que separa `Path` (pura NativeSharp) de `File`/`Directory` (operaciones núcleo, sección
4).

El compilador NO reconoce `Path` por nombre en absoluto — se compila exactamente igual que
cualquier clase que un usuario final de NativeSharp escribiría. Esto se confirma trivialmente:
ninguna de las cuatro capas del compilador (`ProjectMethodBodyModel`, `ProjectOopHirModel`,
`ProjectOopMirModel`, `ProjectOopLlvmLowerer`) tiene una sola línea nueva que mencione `Path`.

### 3.1. El helper `LastSeparatorIndex`: dos `LastIndexOf`, no un `LastIndexOfAny` con array

Windows acepta tanto `\` (separador primario) como `/` (separador alternativo) en cualquier
posición de un path, y pueden aparecer mezclados en el mismo path (`"C:/Users\alberto/x.txt"` es
válido). `String.LastIndexOfAny(string, char[])` existe desde Stage 51 y resolvería esto
directamente — pero requiere alocar un `char[]` con `new char[2]` y liberarlo con `delete[]`
(ver `examples/project_stage51/Main.ns`) en cada llamada. Para dos caracteres fijos y conocidos en
tiempo de compilación, se optó por el camino más simple: dos llamadas independientes a
`String.LastIndexOf(path, "\\")`/`String.LastIndexOf(path, "/")`, devolviendo la que dé el índice
mayor (el separador más a la derecha). Cero asignaciones, cero `delete[]` que olvidar.

### 3.2. Simplificaciones deliberadas frente a `System.IO.Path` real

- **`Combine` solo acepta dos segmentos**, no `params string[]` como el `System.IO.Path.Combine`
  real de .NET. Tampoco reproduce la regla especial de .NET donde un segundo argumento
  "absoluto" (empieza con `"C:\"` o `"\"`) descarta el primero por completo — si algún caller real
  necesita esa regla, es candidata natural para una ronda futura.
- **`GetDirectoryName` de un path sin separador devuelve `""`** (string vacío), no `null` — este
  compilador no tiene un tipo string nullable, así que `""` es la única opción honesta (misma
  simplificación que `Environment.GetEnvironmentVariable` ya aplicó en 53.2b para "la variable no
  existe").

## 4. File / Directory: cuatro operaciones núcleo, una sola llamada a Win32 cada una

Mismo mecanismo de "operación núcleo" que `NativeClock`/`Environment` — ver `File.ns`/
`Directory.ns` para el comentario de cabecera completo. Las cuatro reutilizan
`buildNullTerminatedCopy`, el helper que Stage 53.2b introdujo para convertir un
`NativeSharp.Core.String` (sin terminador nulo, layout `{ i64 length; i8 data[length] }`) en un
`LPCSTR` terminado en null sobre el stack — exactamente la misma necesidad que
`GetEnvironmentVariableA`/`SetEnvironmentVariableA` tenían para su argumento `name`.

### 4.1. `File.Exists` / `Directory.Exists`: una sola llamada a `GetFileAttributesA`

A diferencia de `Environment.GetEnvironmentVariable` (que necesitó DOS llamadas a Win32 — pedir
el tamaño, después llenar el buffer), `Exists` necesita una sola: `GetFileAttributesA` devuelve un
único `DWORD` de bits de atributos, sin marshaling de structs. Dos constantes de Win32 deciden la
respuesta:

- `INVALID_FILE_ATTRIBUTES` (`0xFFFFFFFF`): el path no existe (o `GetFileAttributesA` falló por
  cualquier otro motivo — esta ronda no distingue esos dos casos, la misma simplificación
  deliberada que 53.2b ya aplicó para una variable de entorno faltante).
- `FILE_ATTRIBUTE_DIRECTORY` (`0x10`): el bit está prendido cuando el path ES un directorio.

`File.Exists` responde `true` solo cuando el path resolvió a algo real Y ese algo NO es un
directorio (igual que `System.IO.File.Exists` real — un directorio existente hace que
`File.Exists` devuelva `false`, no que crashee ni que tire una excepción). `Directory.Exists` es
el espejo exacto: `true` solo cuando el path existe Y el bit de directorio está prendido. Ambos
wrappers (`buildFileExists`/`buildDirectoryExists`, `WindowsRuntimeBuilder.cpp`) comparten
prácticamente el mismo cuerpo — la única diferencia es qué combinación booleana de
`isInvalid`/`isDirectory` arma el resultado final.

### 4.2. `File.Delete` / `Directory.CreateDirectory`: una sola llamada, `BOOL` descartado

`DeleteFileA(path)` y `CreateDirectoryA(path, NULL)` (el segundo argumento `NULL` significa
"atributos de seguridad por defecto" — lo mismo que cualquier otra asignación de este runtime ya
asume implícitamente, porque el heap de proceso de `HeapAlloc` no tiene una elección de seguridad
por objeto en primer lugar). Ambas API de Win32 devuelven un `BOOL` de éxito/fracaso, que se
descarta — misma justificación documentada que `SetEnvironmentVariableA` en 53.2b: ni "el archivo
no existía" ni "el directorio padre no existe" son errores que esta ronda intente exponer como
visibles desde NativeSharp. Un futuro `TryDelete`/`TryCreateDirectory` que devuelva `bool` puede
agregarse sin romper estas firmas si algún caller real lo necesita.

`Directory.CreateDirectory` NO es recursivo — `CreateDirectoryA` de Win32 crudo falla si el
directorio padre no existe todavía, y esta ronda no reproduce a mano la lógica de
`SHCreateDirectoryEx` (crear todos los componentes intermedios que falten). Documentado
explícitamente en el comentario de cabecera de `Directory.ns` como alcance fuera de esta ronda.

## 5. La extensión del bloque HIR compartido de string, otra vez

`ProjectOopHirModel.cpp` ya tenía un bloque compartido que maneja
`StringConcat`/`StringEquals`/`StringSubstring`/`StringFromCharArray`, y que Stage 53.2b extendió
para `EnvironmentGetVariable`/`EnvironmentSetVariable` sin duplicarlo (ver `53_2b_...md` sección
Paso 4). Esta ronda lo extiende una vez más, con las cuatro operaciones nuevas
(`FileExists`/`FileDelete`/`DirectoryExists`/`DirectoryCreate`), todas con exactamente un operando
string, sin receptor.

Con ocho `MethodCoreOperationKind` distintos pasando por el mismo bloque, el cálculo del número de
operandos esperado (`expected`) — que en 53.2b ya era una ternería anidada de dos niveles — se
reescribió como una cadena de `if`/`else if` plana en vez de seguir anidando ternarias:

```cpp
std::size_t expected = 2U;
if (core->kind == MethodCoreOperationKind::StringSubstring)
{
    expected = 3U;
}
else if (core->kind == MethodCoreOperationKind::StringFromCharArray ||
    core->kind == MethodCoreOperationKind::EnvironmentGetVariable ||
    core->kind == MethodCoreOperationKind::FileExists ||
    core->kind == MethodCoreOperationKind::FileDelete ||
    core->kind == MethodCoreOperationKind::DirectoryExists ||
    core->kind == MethodCoreOperationKind::DirectoryCreate)
{
    expected = 1U;
}
```

Es un refactor puramente de legibilidad — el comportamiento es idéntico al de la ternaria
anidada equivalente — hecho en el momento en que agregar un noveno o décimo caso a la ternaria
hubiera sido claramente peor que agregarlo a este `if`-chain. La misma extensión (agregar los
cuatro casos nuevos) se aplicó también a la condición OR grande que decide si el bloque entra en
juego, y al `if`/`else if` que elige el `ProjectOopHirNodeKind` final.

## 6. Fuera de alcance de esta ronda (candidatas a `53.3b` y más allá)

- **`File.ReadAllText`/`WriteAllText`/`AppendAllText`**: necesitan `CreateFileA` +
  `GetFileSizeEx` + `ReadFile`/`WriteFile` + `CloseHandle` — una secuencia de varias llamadas a
  Win32 con manejo de handle, bastante más involucrada que "una sola llamada" (el patrón que toda
  operación núcleo de Stage 53 viene siguiendo hasta ahora). Candidata natural para `53.3b`,
  siguiendo el mismo patrón de "empezar simple, diferir lo complejo" que separó 53.2 de 53.2b.
- **Enumerar el contenido de un directorio** (`Directory.GetFiles`/`GetDirectories`): necesitaría
  `FindFirstFileA`/`FindNextFileA`/`FindClose`, una API de iteración con su propio ciclo de vida
  de handle — otra pieza de tamaño comparable a la lectura/escritura de contenido de archivo.
- **`Directory.Delete`** (`RemoveDirectoryA`): trivialmente agregable en una ronda futura con el
  mismo patrón exacto que `File.Delete`/`Directory.CreateDirectory` de esta ronda — simplemente no
  entró en el alcance conservador que se decidió para esta entrega.
- **`Path.GetFullPath` / resolución de paths relativos a absolutos**: necesitaría
  `GetFullPathNameA`, out de alcance por la misma razón de "mantener el alcance de cada ronda
  chico y verificable" que rige todo Stage 53.

## 7. Verificación real, sin inventar cifras

- Probado manualmente ANTES de escribir el test dedicado (disciplina establecida desde Stage 52):
  un programa de prueba independiente ejercitando `Path.Combine`/`GetFileName`/`GetDirectoryName`/
  `GetExtension`/`GetFileNameWithoutExtension` más `File.Exists`/`Delete` y
  `Directory.Exists`/`CreateDirectory` contra el sistema de archivos REAL bajo Wine, en `-O0` y en
  `-O2` — exit code `0` (todos los chequeos pasaron) en ambas configuraciones, confirmado además
  desde el lado Linux que el directorio realmente se creó y el archivo realmente se borró en
  disco.
- `nsc_stage53_3_filesystem_paths_tests`: **PASS, 47/47 chequeos**, confirmado estable en dos
  corridas consecutivas — incluye el fixture mínimo de lowering (los cuatro `case` de
  `File`/`Directory` emiten la llamada exacta a su símbolo de runtime) y la ejecución real bajo
  Wine del programa empaquetado en **ambas** configuraciones de optimización (`-O0` y `-O2`,
  siguiendo la misma disciplina que 53.2b estableció porque estas cuatro operaciones reutilizan
  `buildNullTerminatedCopy`, el mismo helper cuyo bucle causó el bug de `memcpy`/`ucrtbase` de la
  ronda anterior).
- El test dedicado, a diferencia de todos los anteriores de Stage 53, corre Wine con un
  **directorio de trabajo dedicado** (no el directorio donde corre `ctest`) — necesario porque
  esta es la primera ronda de Stage 53 cuyo programa empaquetado toca el sistema de archivos de
  verdad (crea un directorio, borra un archivo). Ver la sección "3." del documento paso a paso
  para el detalle de por qué y cómo.
- El programa de verificación empaquetado (`examples/project_stage53_3/Main.ns`) cubre 20
  chequeos: cinco variantes de `Path.Combine` (básico, separador primario/alternativo ya presente,
  ambos segmentos vacíos), `GetFileName`/`GetDirectoryName` (con y sin separador),
  `GetExtension`/`GetFileNameWithoutExtension` (con y sin punto),
  `DirectorySeparatorChar`/`AltDirectorySeparatorChar`, y seis chequeos de `File`/`Directory` que
  tocan el sistema de archivos real (path inexistente, crear y confirmar un directorio, confirmar
  que ese mismo path NO es un archivo, confirmar un archivo preexistente y borrarlo).
- Suite completa final: **214/214** (`ctest --output-on-failure -j$(nproc)`), confirmado estable
  en dos corridas repetidas (213 previos + 1 nuevo).

## 8. Cierre formal

Stage 53.3 ronda 1 queda cerrada en este estado: una clase de utilidades de paths 100% pura
NativeSharp (`Path`, cero operaciones núcleo nuevas), cuatro operaciones núcleo nuevas
(`File.Exists`/`Delete`, `Directory.Exists`/`CreateDirectory`), reutilización completa del
mecanismo `buildNullTerminatedCopy` de 53.2b (sin necesidad de tocarlo), un refactor de
legibilidad en el bloque HIR compartido (ternaria anidada → `if`-chain), un programa de ejemplo
empaquetado con 20 chequeos que toca el sistema de archivos real, y un test dedicado que verifica
tanto el lowering estructural como la ejecución real bajo Wine en dos configuraciones de
optimización, con confirmación independiente desde el lado Linux de que los efectos en disco
fueron reales. Cero errores de compilador encontrados en esta ronda (a diferencia de 53.2b, que
encontró el bug real de `memcpy`/`ucrtbase`) — la infraestructura que esa ronda corrigió ya cubre
a cualquier operación futura, incluida esta, que reutilice un bucle manual dentro del módulo de
runtime. Suite completa: 214/214.

Con esto, "Filesystem/Paths" (el tercer tercio de Stage 53) queda funcionalmente cerrado para el
alcance conservador que esta ronda se propuso: manipulación de paths, existencia y borrado de
archivos, existencia y creación de directorios. Queda pendiente, deliberadamente fuera (sección
6): lectura/escritura de contenido de archivo (`53.3b`), enumeración de directorios, borrado de
directorios, y resolución de paths relativos a absolutos. Con Stage 53.3 cerrado, los tres tercios
de Stage 53 (Time, Environment, Filesystem/Paths) están en su forma mínima funcional. Queda Stage
54 (threading, con especial cuidado — ver el precedente de RAII/Wine documentado en
`claude/13_bug_raii_trycatch_confirmado_bug_de_wine.md`). Ninguna ronda futura arranca sin señal
explícita de Alberto.
