# Stage 53.4 "Filesystem/Paths (cierre)" — `Directory.Delete` y `Path.GetFullPath`

## 0. Historial de esta entrega

Esta ronda continúa directamente el cierre de Stage 53.3b (`claude/20_stage53_3b_filesystem_paths_contenido.md`), que dejó **todo Stage 53** funcionalmente cerrado en su forma mínima suficiente, con tres piezas explícitamente identificadas como "deliberadamente fuera": `GetCommandLineArgs`, la enumeración/borrado de directorios, y `Path.GetFullPath`.

Alberto preguntó, en una conversación puramente exploratoria (sin implementación), qué impedía tener esas tres piezas hoy. La respuesta distinguió dos categorías: `Directory.Delete` y `Path.GetFullPath` son "casi gratis" (mismo patrón de una sola llamada Win32 ya establecido, sin ninguna decisión de diseño real pendiente); `GetCommandLineArgs` (quoting de línea de comandos, ver `claude/15_fix_cmdexe_quoting_tests_host_abi_windows.md`) y la enumeración de directorios (decisión de diseño real: array de dos pasadas vs. enganchar el `foreach`/enumerador de Stage 52) merecen su propia ronda dedicada.

Alberto confirmó: no se arranca Stage 54 (threading) sin tener cerrado el resto de Stage 53. Esta ronda cierra las dos piezas "casi gratis". `GetCommandLineArgs` y la enumeración de directorios siguen pendientes, cada una para su propia ronda futura, antes de Stage 54.

## 1. Objetivo de esta ronda

Dos primitivas, en dos clases distintas de `NativeSharp.Core.IO`:

- **`Directory.Delete(string path) -> void`** — `RemoveDirectoryA`. Calco letra por letra de `File.Delete` (misma forma: un string, void, BOOL descartado), solo cambia la función Win32 real.
- **`Path.GetFullPath(string path) -> string`** — `GetFullPathNameA`. La **primera** operación núcleo de `NativeSharp.Core.IO.Path`, que hasta esta ronda era NativeSharp puro de punta a punta (ver el comentario de cabecera original de `Path.ns`, escrito en Stage 53.3). Reutiliza el mismo patrón de dos llamadas "pedir tamaño, después llenar el buffer" que `Environment.GetEnvironmentVariable` estableció en 53.2b.

Ninguna de las dos necesitó una decisión de diseño nueva: ambas son, literalmente, "el mismo patrón ya usado en esta clase (o en una hermana), con una función Win32 distinta". Eso es exactamente lo que las hacía "casi gratis" frente a `GetCommandLineArgs` y la enumeración de directorios.

## 2. Punto de partida verificado

Antes de tocar código se releyeron `File.ns` (para el patrón exacto de `Delete`) y `Environment.ns` (para el patrón exacto de `GetEnvironmentVariable`), además de las funciones reales en `src/Runtime/WindowsRuntimeBuilder.cpp`: `buildFileDelete` (28 líneas, la plantilla para `Directory.Delete`) y `buildEnvironmentGetVariable` (85 líneas, la plantilla para `Path.GetFullPath`). Ambas se transcriben casi sin cambios — la disciplina de este proyecto desde 53.1 es "cuando ya existe el patrón exacto, no inventar uno nuevo".

## 3. `Directory.Delete`: calco exacto de `File.Delete`

```cpp
bool buildDirectoryDelete(
    LLVMContextRef context, LLVMModuleRef module,
    LLVMValueRef removeDirectoryA, LLVMTypeRef removeDirectoryAType)
{
    // ... idéntico a buildFileDelete, cambiando deleteFileA por removeDirectoryA ...
}
```

La única diferencia real frente a `buildFileDelete` es de vocabulario, no de forma: `DeleteFileA` se convierte en `RemoveDirectoryA`. El BOOL de retorno se descarta con la misma justificación de siempre (ningún caller de esta ronda necesita distinguir "no existía" de "falló por otro motivo").

Un detalle real de Win32, **no una simplificación de este compilador**: `RemoveDirectoryA` solo borra directorios **vacíos**. Un directorio con contenido no se borra — la llamada simplemente no hace nada, sin que haya manera de distinguir ese caso de "no existía" bajo la simplificación actual (BOOL descartado). El programa de ejemplo de esta ronda verifica exactamente este comportamiento (chequeos 4 y 5 de `Main.ns`, ver sección 7).

En las 4 capas del compilador, `DirectoryDelete`/`CoreDirectoryDelete` se agregó exactamente donde vive `FileDelete`/`CoreFileDelete`: mismo `expected = 1U` en el bucket de operandos de `ProjectOopHirModel.cpp`, mismo shape de lowering ("llamar al hook, descartar el resultado, devolver un `i1` dummy") en `ProjectOopLlvmLowerer.cpp`.

## 4. `Path.GetFullPath`: la primera operación núcleo de `Path`, y el patrón de dos llamadas

Desde que `Path.ns` se escribió en Stage 53.3, su comentario de cabecera decía explícitamente: *"Path NO necesita ni una sola operación núcleo nueva. Es NativeSharp puro, 100% de punta a punta"*. Eso dejó de ser cierto con esta ronda — pero solo para **un** método. `Combine`, `GetFileName`, `GetDirectoryName`, `GetExtension`, `GetFileNameWithoutExtension` siguen siendo exactamente lo que eran: manipulación de texto pura sobre `NativeSharp.Core.String`, sin hablar con Win32 para nada.

`GetFullPath` es distinto porque "convertir un path relativo en absoluto" necesita un dato que **solo el sistema operativo tiene**: el directorio de trabajo actual del proceso. Ningún método de `String` puede responder esa pregunta — por eso, y solo por eso, esta operación sí necesita hablar con Win32.

La firma real de `GetFullPathNameA` tiene **cuatro** parámetros, no tres como `GetEnvironmentVariableA`:

```c
DWORD GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR *lpFilePart);
```

`lpFilePart` es un puntero de salida opcional al componente "nombre de archivo" dentro del resultado ya resuelto — se pasa siempre `NULL`, porque `Path.GetFileName` ya resuelve esa misma pregunta con manipulación de texto pura sobre el resultado.

El valor de retorno tiene la **misma forma de dos casos** que `GetEnvironmentVariableA` ya usaba en 53.2b:

- Si el buffer pasado es demasiado chico (el caso de la primera llamada, con `lpBuffer=NULL`/`nBufferLength=0`), devuelve el tamaño que hace falta **incluyendo** el terminador nulo.
- Si la segunda llamada (con un buffer ya del tamaño correcto) tiene éxito, devuelve la longitud copiada, **sin** contar el terminador — pero ese valor se ignora, exactamente como `buildEnvironmentGetVariable` ya ignoraba el resultado de su segunda llamada, porque `charCount` ya se deriva de `requiredSize` en la primera.
- 0 significa fallo. Stage 53.4 no distingue el motivo (path con caracteres inválidos, etc.) — misma simplificación deliberada de siempre, colapsando a `""`.

`buildPathGetFullPath` es, línea por línea, `buildEnvironmentGetVariable` con la llamada Win32 cambiada y el argumento extra (`lpFilePart=NULL`) agregado a ambas llamadas.

## 5. La sorpresa real: `""` no es sinónimo de `"."`

Al diseñar esta ronda, la intuición inicial fue: *"un path vacío es una entrada trivialmente válida para `GetFullPathNameA` — Windows lo resuelve como 'ningún componente relativo que agregar', así que `GetFullPath("")` debería devolver el directorio de trabajo actual, igual que `GetFullPath(".")`"*. Esa intuición se escribió primero en el comentario de cabecera de `Path.GetFullPath` y en `Main.ns`.

**Es incorrecta.** Se verificó con un probe aislado (un `Main.ns` de una sola línea, devolviendo `Path.GetFullPath("").Length` como código de salida) corrido bajo Wine, **antes** de empaquetar el ejemplo completo de esta ronda:

```csharp
public static int Main()
{
    string cwd = Path.GetFullPath("");
    return cwd.Length;
}
```

```
$ wine probe_gfp.exe; echo "EXIT_CODE=$?"
EXIT_CODE=0
```

Código de salida `0` — la longitud del string es cero. `GetFullPathNameA("")` bajo Wine devuelve `0` (fallo), no el tamaño del directorio de trabajo actual. Un segundo probe, sustituyendo `""` por `"."`, confirmó que `"."` sí funciona como se esperaba:

```csharp
public static int Main()
{
    string cwd = Path.GetFullPath(".");
    if (cwd.Length == 0) { return 1; }
    string cwdAgain = Path.GetFullPath(cwd);
    if (cwdAgain.Length != cwd.Length) { return 2; }
    if (!cwdAgain.Equals(cwd)) { return 3; }
    string empty = Path.GetFullPath("");
    if (empty.Length != 0) { return 4; }
    return 0;
}
```

```
EXIT_CODE=0
```

Los cuatro chequeos pasan: `"."` resuelve al directorio de trabajo actual, resolver un path ya absoluto es idempotente, y `""` sigue devolviendo `""` (colapsando a la rama de fallo, tal como cualquier otro fallo de esta operación núcleo). El comentario de cabecera de `Path.GetFullPath` en `Path.ns`, el comentario de `buildPathGetFullPath` en `WindowsRuntimeBuilder.cpp`, y `Main.ns` del ejemplo empaquetado se corrigieron para reflejar el comportamiento **real observado**, no la suposición inicial — la misma disciplina de "verificar antes de documentar" que ya atrapó el error de longitud de string en 53.2b/53.3b, aplicada esta vez a una suposición semántica en lugar de una cuenta de caracteres.

Para pedir el directorio de trabajo actual con `Path.GetFullPath`, la forma correcta en NativeSharp es `Path.GetFullPath(".")`, no `Path.GetFullPath("")`.

## 6. Decisiones de diseño de la superficie

- **`Path.GetFullPath` NO está declarado `owned string`.** Exactamente el mismo patrón que `Environment.GetEnvironmentVariable` (53.2b) y `File.ReadAllText` (53.3b) establecieron: el tipo de retorno declarado es `string` a secas, así que un caller NativeSharp guarda el resultado en una variable `string` común, nunca `raii string`. Contraste directo con `Path.GetFileName`/`Path.Combine` (usadas en el mismo `Main.ns`): esas SÍ están declaradas `owned string`, por ser NativeSharp puro siguiendo la disciplina original de `Path.ns`. Las dos reglas conviven en el mismo archivo desde esta ronda.
- **`GetFullPathNameA`, no `_fullpath` ni ninguna variante de la CRT.** Este runtime no enlaza contra la CRT completa (ver `claude/04_restricciones_arquitectonicas.md`) — todas las operaciones núcleo de Stage 53 hablan Win32 directamente, sin excepción, y esta ronda no rompe esa disciplina.
- **`RemoveDirectoryA` no distingue "no existía" de "no estaba vacío".** Ambos casos son la misma llamada, sin resultado que consultar — la misma simplificación que `File.Delete`/`DirectoryCreate` ya aplicaban desde 53.3. Un borrado recursivo (el equivalente a `Directory.Delete(path, recursive: true)` de .NET real) queda fuera de alcance: necesitaría la enumeración de directorios que este proyecto todavía no tiene.
- **`lpFilePart` siempre `NULL`.** `Path.GetFileName` ya resuelve esa pregunta con manipulación de texto pura — pedirle a Win32 que la resuelva de nuevo sería redundante.

## 7. Verificación real, sin inventar cifras

El programa empaquetado (`examples/project_stage53_4/Main.ns`) tiene 14 chequeos:

1–2. `Directory.CreateDirectory` + `Directory.Exists` (true) + `Directory.Delete` + `Directory.Exists` (false) sobre un directorio vacío — confirma que `Delete` realmente borra, no solo que no crashea.
3\. `Directory.Delete` sobre un directorio que no existe — no crashea.
4–5. `Directory.Delete` sobre un directorio **no vacío** (con un archivo adentro, creado vía `Path.Combine` + `File.WriteAllText`) — el directorio debe seguir existiendo después del primer intento (`RemoveDirectoryA` falla en silencio); recién tras vaciarlo, el segundo intento lo borra de verdad.
6–8. `Path.GetFullPath(".")` no vacío, e idempotente al aplicarlo sobre su propio resultado.
9\. `Path.GetFullPath("")` — confirma el comportamiento real de la sección 5: el resultado es `""`.
10–11. `Path.GetFullPath` sobre un nombre de archivo relativo — el resultado, pasado por `Path.GetFileName`, debe terminar exactamente en ese nombre.
12–14. Interoperabilidad real con `File.*`: escribir con el nombre relativo, leer/borrar con el path absoluto que `Path.GetFullPath` calculó **antes** de que el archivo existiera (resolución puramente léxica, no consulta el disco) — deben ser el mismo archivo en ambos casos.

Ejecución real bajo Wine, en un directorio de trabajo dedicado (mismo patrón que todo Stage 53 desde 53.3), a `-O0` y `-O2`:

```
O0 EXIT_CODE=0
O2 EXIT_CODE=0
```

Confirmado estable en dos corridas consecutivas por nivel. Confirmación independiente desde el lado Linux: después de cada corrida, el directorio de trabajo dedicado queda **completamente vacío** — a diferencia de 53.3b (que dejaba un archivo con contenido final para confirmar), este programa crea y borra todo lo que usa, así que "el directorio terminó vacío" es en sí mismo el testigo independiente de que `Directory.Delete`/`File.Delete` realmente borraron en disco, no solo que el código de salida de NativeSharp lo afirma.

Test dedicado: `nsc_stage53_4_filesystem_paths_cierre_tests` → **43/43 chequeos**, estable en dos corridas. Suite completa: `ctest` → **216/216** (215 previos + 1 nuevo), estable en dos corridas consecutivas.

## 8. Cierre formal

`Directory.Delete` y `Path.GetFullPath` quedan cerrados: implementados en las 4 capas del compilador + runtime + whitelist de `kernel32`, verificados con ejecución real bajo Wine en ambos niveles de optimización, documentados (este documento + el paso a paso), empaquetados y entregados.

Pendiente, deliberadamente fuera de esta ronda (y de todo Stage 53): `GetCommandLineArgs` (quoting de línea de comandos) y la enumeración de directorios (`GetFiles`/`GetDirectories`, decisión de diseño real pendiente). Ninguna ronda de esas dos arranca sin señal explícita de Alberto — y, por lo acordado en esta misma conversación, Stage 54 (threading) tampoco arranca hasta que Alberto decida qué hacer con esas dos piezas restantes.
