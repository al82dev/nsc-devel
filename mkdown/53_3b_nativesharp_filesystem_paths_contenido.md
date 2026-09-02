# 53.3b — NativeSharp Filesystem/Paths (contenido de archivo): ReadAllText, WriteAllText, AppendAllText

## 0. Historial de esta entrega

Continuación directa de Stage 53.3 ronda 1 ("Filesystem/Paths"), que cerró con **214/214** tests
y dejó explícitamente fuera de alcance `File.ReadAllText`/`WriteAllText`/`AppendAllText` — el
motivo documentado en `File.ns` y en `53_3_nativesharp_filesystem_paths.md` sección 6: estas tres
operaciones necesitan una secuencia de Win32 más larga (`CreateFileA` + `GetFileSizeEx` +
`ReadFile`/`WriteFile` + `CloseHandle`) que "una sola llamada", el patrón que toda operación
núcleo de Stage 53 venía siguiendo hasta ese punto.

Esta entrega es esa ronda diferida: cierra "Filesystem/Paths" en su forma completa, sin dejar
ninguna pieza de contenido de archivo pendiente para el alcance actual del proyecto.

## 1. Objetivo de esta ronda

1. `File.ReadAllText(string path) -> string`: leer el contenido completo de un archivo.
2. `File.WriteAllText(string path, string content) -> void`: escribir contenido, reemplazando
   cualquier contenido anterior.
3. `File.AppendAllText(string path, string content) -> void`: escribir contenido, agregándolo al
   final del contenido existente (o creando el archivo si no existía).

## 2. Punto de partida verificado

`ctest` con el árbol tal cual quedó al cerrar Stage 53.3 ronda 1: **214/214**.

## 3. La secuencia de Win32: cuatro llamadas, no una

A diferencia de `File.Exists`/`Directory.Exists` (una sola llamada a `GetFileAttributesA`) o de
`File.Delete`/`Directory.CreateDirectory` (una sola llamada cada una), leer o escribir el
**contenido** de un archivo en Win32 crudo requiere abrir un handle, hacer algo con él, y
cerrarlo — el mismo patrón "open/use/close" que cualquier lenguaje con acceso a archivos de bajo
nivel necesita.

### 3.1. `ReadAllText`: `CreateFileA` + `GetFileSizeEx` + `ReadFile` + `CloseHandle`

1. `CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
   NULL)` — abre el archivo para lectura. `OPEN_EXISTING` es la elección correcta aquí:
   `ReadAllText` nunca debería CREAR un archivo, solo leer uno que ya existe.
2. Si el `HANDLE` devuelto es `INVALID_HANDLE_VALUE` (`(HANDLE)-1`, todos los bits en uno), el
   archivo no existe (o `CreateFileA` falló por cualquier otro motivo) — misma simplificación
   deliberada que `Environment.GetEnvironmentVariable` (53.2b) y `File.Exists`/`Directory.Exists`
   (53.3) ya aplicaron: colapsar todo fallo en un resultado vacío, no distinguir causas.
3. `GetFileSizeEx(handle, &size)` — pregunta el tamaño exacto del archivo, en un `i64` (a
   diferencia del `GetFileSizeEx` de por sí, este SÍ usa un entero de 64 bits genuino, no un
   `DWORD` de 32 — la única de las llamadas de esta ronda con ese detalle). El slot de tamaño se
   inicializa en `0` ANTES de la llamada, así que si `GetFileSizeEx` falla (su `BOOL` de retorno
   no se verifica, misma justificación de siempre), el resultado degrada a "leer cero bytes", no
   a memoria sin inicializar.
4. Se aloca un `NativeSharp.Core.String` nuevo de exactamente `8 + tamaño` bytes — el mismo
   mecanismo `GetProcessHeap` + `HeapAlloc` que `buildEnvironmentGetVariable` ya usaba en 53.2b.
5. `ReadFile(handle, bufferDeDatos, tamaño, &bytesLeídos, NULL)` — llena el string recién alocado
   directamente en su posición de datos (offset +8), sin ningún paso intermedio.
6. `CloseHandle(handle)`.

### 3.2. `WriteAllText`/`AppendAllText`: la diferencia es UN flag y UN paso extra

Ambas comparten casi todo el cuerpo con una sola diferencia real de comportamiento:

- `WriteAllText` abre con `CREATE_ALWAYS` (crea el archivo, o lo trunca a longitud cero primero
  si ya existía) y escribe directamente — el archivo termina conteniendo SOLO el contenido nuevo.
- `AppendAllText` abre con `OPEN_ALWAYS` (abre el archivo conservando su contenido, o lo crea si
  no existía) y hace un paso extra ANTES de escribir: `SetFilePointer(handle, 0, NULL,
  FILE_END)` — mueve la posición de escritura al final del archivo. Sin este paso,
  `OPEN_ALWAYS` deja la posición en el byte 0, y la escritura reemplazaría el contenido existente
  en vez de agregarse después — exactamente el bug que este paso existe para evitar.

Ambas terminan igual: `WriteFile(handle, datosDelContenido, longitud, &bytesEscritos, NULL)` +
`CloseHandle(handle)`. El `BOOL` de retorno de `WriteFile` se descarta, misma justificación
documentada desde 53.2b.

## 4. El detalle que casi pasa desapercibido: el argumento CONTENT nunca necesita copia null-terminada

Toda operación núcleo de Stage 53 que recibe un argumento de tipo `string` y necesita pasárselo a
una API de Win32 que espera `LPCSTR` (`GetEnvironmentVariableA`, `SetEnvironmentVariableA`,
`GetFileAttributesA`, `DeleteFileA`, `CreateDirectoryA`, y ahora `CreateFileA` para el argumento
PATH) pasa por `buildNullTerminatedCopy` — el layout nativo de `NativeSharp.Core.String`
(`{ i64 length; i8 data[length] }`, sin terminador nulo) nunca es directamente compatible con lo
que esas API esperan.

`WriteFile` es diferente: su firma real es `WriteFile(HANDLE, LPCVOID lpBuffer, DWORD
nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED)` — un puntero a bytes CRUDOS
más un CONTADOR explícito de cuántos escribir, sin ningún requisito de terminador nulo en
absoluto. Esto significa que el argumento CONTENT de `WriteAllText`/`AppendAllText` puede
pasarse a `WriteFile` usando directamente la posición de datos del `NativeSharp.Core.String` (el
mismo offset +8 de siempre, sin ninguna copia intermedia) — solo el argumento PATH (que sí va a
`CreateFileA`, que sí espera `LPCSTR`) necesita el tratamiento de `buildNullTerminatedCopy`.

Esto no es una optimización prematura: es simplemente la firma real de `WriteFile`, ya declarada
en este runtime desde Stage 41 para `Console.WriteLine` (`buildConsoleWriteLine`/`buildPuts`
reutilizan la misma variable `writeFile`/`writeFileType` — no hizo falta declarar `WriteFile` de
nuevo para esta ronda, solo reutilizar la declaración existente).

## 5. `INVALID_HANDLE_VALUE`: por qué se compara como entero, no como puntero

Las tres funciones nuevas comparan el `HANDLE` que devuelve `CreateFileA` contra
`INVALID_HANDLE_VALUE` (`(HANDLE)-1` en C, es decir, todos los 64 bits en uno) para saber si la
apertura falló. En el API C de LLVM, un `HANDLE` se modela como `LLVMValueRef` de tipo puntero
(`ptr`) — comparar directamente un puntero contra una constante "todos los bits en uno" es
awkward en modo de punteros opacos. La solución, uniforme en las tres funciones: convertir el
handle a `i64` con `LLVMBuildPtrToInt` primero, y comparar ese entero contra la constante entera
`0xFFFFFFFFFFFFFFFF` — sin ambigüedad, sin depender de cómo LLVM represente internamente un
puntero "todos unos".

## 6. Decisiones de diseño de la superficie

- **`ReadAllText` de un archivo inexistente devuelve `""`, no lanza una excepción.** Este
  compilador SÍ tiene excepciones (desde Stage 45) — pero traducir un código de error de Win32 a
  una excepción de NativeSharp es su propia pieza de diseño (¿qué tipo de excepción? ¿qué mensaje?
  ¿cómo se relaciona con el sistema de `try`/`catch` ya existente?), deliberadamente fuera de
  alcance de esta ronda. Misma simplificación que toda operación núcleo de Stage 53 viene
  aplicando desde 53.2b.
- **Truncamiento a 32 bits del tamaño del archivo antes de pasarlo a `ReadFile`/`WriteFile`.**
  `nNumberOfBytesToRead`/`nNumberOfBytesToWrite` son `DWORD` (32 bits) en la firma real de Win32,
  mientras que la longitud de un `NativeSharp.Core.String` es un `i64` completo. Un archivo de
  texto de más de ~4 GiB se leería (o escribiría) solo parcialmente. No se implementó un bucle de
  lectura/escritura en trozos para cubrir ese caso: el dominio real de este compilador (ver
  `claude/03_posicionamiento.md`) hace que un archivo de texto multi-gigabyte sea un input poco
  realista para `ReadAllText`/`WriteAllText` específicamente.
- **`File.ReadAllText` NO está declarado `owned string`.** Exactamente el mismo patrón que
  `Environment.GetEnvironmentVariable` estableció en 53.2b: el tipo de retorno declarado es
  `string` a secas. Esto significa que un caller NativeSharp guarda el resultado en una variable
  `string` común, nunca `raii string` — intentar lo segundo es un error de compilación,
  verificado directamente (ver la sección 3 del documento paso a paso). Documentado explícitamente
  en el comentario de cabecera de `examples/project_stage53_3b/Main.ns`, con el contraste directo
  frente a `Path.Combine` (que SÍ es `owned string`, por ser NativeSharp puro siguiendo la
  disciplina de `String.ns`).
- **`SetFilePointer`, no `SetFilePointerEx`.** La versión moderna de 64 bits (`SetFilePointerEx`)
  existiría como alternativa más "correcta" para archivos gigantes, pero dado que este runtime ya
  trunca tamaños a 32 bits en `ReadFile`/`WriteFile` (punto anterior), usar la versión de 32 bits
  de `SetFilePointer` es consistente con el mismo límite práctico ya aceptado, no una
  inconsistencia nueva.

## 7. Verificación real, sin inventar cifras

- Probado manualmente ANTES de escribir el test dedicado: un programa de prueba independiente
  ejercitando `ReadAllText` de un archivo inexistente, un round-trip básico de
  `WriteAllText`+`ReadAllText`, sobreescribir con contenido más corto (confirma que
  `CREATE_ALWAYS` trunca de verdad), y dos llamadas consecutivas de `AppendAllText` (confirma que
  la segunda agrega después de la primera, no la reemplaza) — bajo Wine, en `-O0` y `-O2`, exit
  code `0` en ambas configuraciones tras limpiar el estado entre corridas (ver Paso 5 del
  documento paso a paso para el detalle de un falso positivo de estado NO limpiado entre corridas
  manuales, que no era un bug del compilador).
- `nsc_stage53_3b_filesystem_paths_content_tests`: **PASS, 44/44 chequeos**, confirmado estable
  en dos corridas consecutivas — incluye el fixture mínimo de lowering (los tres `case` de `File`
  emiten la llamada exacta a su símbolo de runtime) y la ejecución real bajo Wine del programa
  empaquetado en **ambas** configuraciones de optimización, con confirmación independiente desde
  el lado Linux de que `AppendAllText` realmente agregó contenido en disco (no lo reemplazó).
- El programa de verificación empaquetado (`examples/project_stage53_3b/Main.ns`) cubre 12
  chequeos: `ReadAllText` de un archivo inexistente; un round-trip básico verificado por longitud
  Y contenido; sobreescribir con contenido más largo, luego más corto (confirma que
  `CREATE_ALWAYS` trunca correctamente en ambas direcciones); escribir contenido vacío (el
  archivo termina en longitud cero, no en error); `AppendAllText` sobre un archivo inexistente
  (se comporta como `WriteAllText`); `AppendAllText` sobre un archivo existente (agrega, no
  reemplaza — el chequeo más importante de todo el programa); y un round-trip completo usando un
  path armado con `Path.Combine`, confirmando que las dos piezas de Stage 53.3 (operación núcleo
  y NativeSharp puro) interoperan sin fricción.
- Suite completa final: **215/215** (`ctest --output-on-failure -j$(nproc)`), confirmado estable
  en dos corridas repetidas (214 previos + 1 nuevo).

## 8. Cierre formal

Stage 53.3b queda cerrada en este estado: tres operaciones núcleo nuevas (`File.ReadAllText`/
`WriteAllText`/`AppendAllText`), la secuencia de Win32 más larga que cualquier operación núcleo
de este proyecto haya necesitado hasta ahora (`CreateFileA`+`GetFileSizeEx`+`ReadFile`/`WriteFile`
+`CloseHandle`, 4-5 llamadas en vez de 1), reutilización completa de `buildNullTerminatedCopy`
para el argumento PATH sin necesidad de tocarlo, y el descubrimiento (no un bug, una
característica real de la firma de `WriteFile`) de que el argumento CONTENT nunca necesita esa
misma copia. Cero errores de compilador nuevos encontrados en esta ronda — el único "incidente"
fue un falso positivo de estado no limpiado entre dos corridas manuales del probe (ver sección 5
del documento paso a paso), no un bug del compilador ni del runtime. Suite completa: 215/215.

Con esto, "Filesystem/Paths" (el tercer tercio de Stage 53) queda completamente cerrado para el
alcance que este proyecto necesita hoy: manipulación de paths, existencia/borrado de archivos,
existencia/creación de directorios, y ahora lectura/escritura/append de contenido de archivo.
Queda pendiente, deliberadamente fuera (ver sección 6 de `53_3_nativesharp_filesystem_paths.md`):
enumeración de directorios, borrado de directorios, y resolución de paths relativos a absolutos —
ninguna con urgencia real para el alcance actual del proyecto. Con Stage 53 (Time, Environment,
Filesystem/Paths) funcionalmente cerrado en su forma mínima completa, queda Stage 54 (threading,
con especial cuidado — ver el precedente de RAII/Wine documentado en
`claude/13_bug_raii_trycatch_confirmado_bug_de_wine.md`). Ninguna ronda futura arranca sin señal
explícita de Alberto.
