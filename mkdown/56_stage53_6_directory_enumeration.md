# Stage 53.6 "Enumeracion de directorios" -- cierre (y cierre de Stage 53 completo)

## Que se agrego

`NativeSharp.Core.IO.Directory` gana cuatro operaciones nucleo nuevas:

```csharp
public static int GetFileCount(string path);
public static int GetFiles(string path, string[] destination);
public static int GetDirectoryCount(string path);
public static int GetDirectories(string path, string[] destination);
```

`GetFileCount`/`GetDirectoryCount` cuentan las entradas de nivel superior de `path` que son
archivos o subdirectorios respectivamente. `GetFiles`/`GetDirectories` llenan un buffer YA
ASIGNADO por quien llama con los NOMBRES (no paths completos) de esas entradas, y devuelven cuantos
elementos escribieron de verdad.

Uso tipico:

```csharp
int n = Directory.GetFileCount("C:\\datos");
string[] names = new string[n];
int written = Directory.GetFiles("C:\\datos", names);
// names[0..written-1] son los nombres de archivo, sin el directorio por delante
```

Con Stage 53.6 cerrado, **Stage 53 "Runtime esencial (I/O, tiempo, entorno)" queda completo.**

## Por que arreglo de dos pasadas y no un enumerador via `foreach`

Exactamente el mismo "hueco de owned string[]" que ya goberno cada operacion de esta familia desde
`String.Split` (Stage 51) hasta `Environment.GetCommandLineArgs` (Stage 53.5): este compilador no
tiene forma de construir un `owned string[]` cuyos elementos se llenan en varios pasos dentro del
propio metodo. La solucion es la misma de siempre: quien llama pre-asigna el buffer con
`new string[n]` (usando `GetFileCount`/`GetDirectoryCount` para saber cuanto pedir), y el metodo lo
llena devolviendo cuantos elementos escribio.

Se considero explicitamente la alternativa -- un `Directory.EnumerateFiles` respaldado por un
handle real de Win32 (`FindFirstFileA`/`FindNextFileA`), enganchado a la maquinaria de
`GetEnumerator()`/`MoveNext()`/`Current` que Stage 52.x ya construyo para las colecciones
NativeSharp puras. Se descarto para esta ronda: seria el primer enumerador de este runtime atado a
un recurso del SISTEMA OPERATIVO en vez de a memoria NativeSharp propia, y abre una pregunta nueva
que ninguna otra clase de este proyecto tuvo que responder todavia -- que pasa con el handle de
Win32 si un `foreach` corta a mitad de camino con `break`, `return`, o una excepcion (¿quien llama
a `FindClose`? ¿el destructor del enumerador, que este compilador no genera automaticamente para
tipos que no son `owned`/RAII?). Territorio nuevo y mas riesgoso que no se justifica para cerrar
Stage 53 -- el arreglo de dos pasadas resuelve el 100% de los casos de uso reales sin abrir esa
pregunta, exactamente al costo (una llamada extra) que `GetCommandLineArgCount`/
`GetEnvironmentVariableA` ya pagan en toda la familia.

## Diseno confirmado por Alberto: comparacion de las dos opciones

Antes de implementar se presentaron dos formas concretas del arreglo de dos pasadas:

- **Opcion A (la elegida)**: cuatro metodos separados -- `GetFileCount`/`GetFiles` y
  `GetDirectoryCount`/`GetDirectories` -- cada par siguiendo letra por letra el patron
  count-then-fill que `GetCommandLineArgCount`/`GetCommandLineArgs` ya establecio.
- **Opcion B**: un unico par `GetEntryCount(path, bool includeDirectories)`/
  `GetEntries(path, string[] destination, bool includeDirectories)`, con un booleano de tipo
  NativeSharp expuesto en la firma publica para elegir archivos vs directorios.

Alberto pregunto "que harias tu?" -- la recomendacion fue la Opcion A, por dos razones: (1) sigue
exactamente el precedente que esta familia de operaciones (Split/RSplit/GetCommandLineArgs) ya
establecio tres veces -- un par de metodos por "cosa distinta que se puede pedir", no un parametro
booleano que cambia el significado del resultado; y (2) evita introducir en la superficie PUBLICA
de la API un booleano de comportamiento (`includeDirectories`) cuando NativeSharp ya tiene, en
otras clases, el patron mas idiomatico de metodos con nombres distintos para cosas distintas
(`File.Exists`/`Directory.Exists`, no un `Exists(path, bool isDirectory)` unico). Alberto confirmo
("hagamozlo como comentas") antes de que empezara la implementacion.

Del lado del runtime C++, el booleano SI existe -- pero como parametro de **tiempo de generacion
de codigo en C++** (`bool wantDirectories` que la funcion constructora de LLVM recibe una vez por
cada uno de los dos codegen que produce), nunca como valor de runtime dentro del binario generado.
Ver la seccion "Runtime" mas abajo.

## Alcance de esta ronda, deliberadamente angosto

Misma disciplina que cada ronda anterior de Stage 53:

- Solo el nivel superior del directorio -- SIN recursividad (el equivalente a
  `SearchOption.AllDirectories` en .NET real queda fuera de alcance).
- Devuelve NOMBRES, no paths completos -- `Path.Combine` (NativeSharp puro, ya existente desde
  Stage 53.3) resuelve "directorio + nombre = path completo" para quien lo necesite.
- `GetFiles` excluye subdirectorios, `GetDirectories` excluye archivos -- el mismo bit
  `FILE_ATTRIBUTE_DIRECTORY` (0x10) que `File.Exists`/`Directory.Exists` ya usan desde Stage 53.3.
  `GetDirectories` ademas excluye las entradas pseudo `.` y `..` que `FindFirstFileA`/
  `FindNextFileA` siempre devuelven para cualquier directorio real.
- Directorio inexistente/inaccesible (`FindFirstFileA` devuelve `INVALID_HANDLE_VALUE`) colapsa a
  0 elementos en las cuatro operaciones -- misma simplificacion deliberada de siempre ante
  cualquier fallo de Win32.
- `destination` mas chico que la cantidad real de entradas: las que sobran se descartan en
  silencio, sin excepcion -- mismo comportamiento que `GetCommandLineArgs` (NO el
  truncado-en-el-ultimo-elemento de `String.Split`).

## Runtime: un solo par de funciones C++ compartidas, cuatro simbolos LLVM exportados

A nivel del lowerer LLVM (`ProjectOopLlvmLowerer.cpp`), las cuatro operaciones nucleo emiten
llamadas a CUATRO simbolos runtime distintos (`__nativesharp_directory_get_file_count`,
`__nativesharp_directory_get_files`, `__nativesharp_directory_get_directory_count`,
`__nativesharp_directory_get_directories`) -- misma disciplina que `File.Exists`/
`Directory.Exists` (dos simbolos distintos aunque comparten la misma llamada a
`GetFileAttributesA` internamente).

Del lado de `WindowsRuntimeBuilder.cpp`, sin embargo, esos cuatro simbolos se generan a partir de
**dos** funciones constructoras C++ compartidas -- `buildDirectoryGetEntryCount` y
`buildDirectoryGetEntries` -- cada una parametrizada por `bool wantDirectories` y llamada DOS
veces (una para el par de archivos, otra para el par de directorios) para producir los cuatro
`LLVMValueRef` de funcion exportada. El booleano decide, en tiempo de GENERACION de codigo C++, que
rama del filtro por atributo emitir (`isDirectory` vs `!isDirectory`) y si emitir ademas el bloque
de exclusion de `.`/`..` -- nunca aparece como un valor de runtime dentro del LLVM IR resultante,
asi que cada uno de los cuatro binarios finales termina con exactamente el mismo numero de
instrucciones que tendria una implementacion escrita a mano cuatro veces, sin el costo de
mantenimiento de mantenerlas sincronizadas.

Esta ronda es la PRIMERA en usar `FindFirstFileA`/`FindNextFileA`/`FindClose` en todo este
runtime. Las tres son ANSI (`*A`), misma disciplina de siempre. El WIN32_FIND_DATAA que
`FindFirstFileA`/`FindNextFileA` llenan es un struct de Win32 real (no una decision de este
compilador) que este runtime lee campo por campo via GEP con offsets constantes -- ver el
comentario de cabecera del grupo de funciones en `WindowsRuntimeBuilder.cpp` para el layout exacto
(320 bytes totales, `dwFileAttributes` en el offset 0, `cFileName` -- lo unico que este runtime
lee -- en el offset 44).

Dos helpers nuevos, compartidos por las cuatro operaciones:

- `buildDirectorySearchPattern`: construye `path + "\*" + '\0'` -- el patron con comodin que
  `FindFirstFileA` necesita para enumerar TODO el contenido de un directorio (un path a secas
  buscaria solo una entrada exacta, no una lista).
- `buildAnsiCStringToNativeSharpString`: la direccion OPUESTA de `buildNullTerminatedCopy` (que ya
  existia desde Stage 53.2b) -- convierte un buffer ANSI null-terminated crudo (`cFileName`) en un
  `NativeSharp.Core.String` recien asignado en el heap. Mide la longitud con el mismo loop de
  escaneo byte a byte que `buildPuts` ya usa, despues copia con el mismo idiom de
  `buildNullTerminatedCopy` -- dos loops separados porque el tamano del `HeapAlloc` debe conocerse
  ANTES de pedirlo.

Sin PHI nodes en ningun lado -- alloca-based slots (indice, handle, contador/writeIndex) con
branch-to-merge, exactamente el mismo estilo que `buildEnvironmentGetCommandLineArgs` (Stage 53.5)
ya establecio para el loop mas complejo de este proyecto hasta ahora.

## GetFiles/GetDirectories: cuando el buffer se llena, cortar temprano

A diferencia de `GetCommandLineArgs` (que ya sabia de antemano `writeCount = min(argc,
destination.Length)` porque `CommandLineToArgvW` entrega el conteo real en una sola llamada),
`GetFiles`/`GetDirectories` NO conocen la cantidad total de entradas hasta terminar de enumerar --
por diseno, cada llamada a este runtime hace UN SOLO recorrido `FindFirstFileA`/`FindNextFileA`
(la segunda pasada del arreglo de dos pasadas es, en si misma, un unico recorrido de Win32, no dos
recorridos internos). Por eso el loop de `buildDirectoryGetEntries` verifica, en cada entrada que
hace match, si `destination` todavia tiene espacio (`writeIndex < destination.Length`) ANTES de
convertir el nombre a `NativeSharp.Core.String` y escribirlo -- si no hay espacio, corta
directamente al bloque final (`FindClose` + `return`) en vez de seguir enumerando entradas que de
todas formas no podria escribir. Mismo resultado observable que "silenciosamente descartar los que
sobran", sin el costo de convertir y despues descartar strings que nunca se usarian.

## Verificacion

- Fixture minima: confirma que las cuatro operaciones nucleo lowerean a LLVM y cada una emite una
  llamada directa a su simbolo runtime exacto.
- Ejemplo empaquetado (`examples/project_stage53_6`) compilado a traves del Driver real, a O0 y
  O2, con 20 chequeos numerados en `Main.ns`: poblacion de un directorio de prueba con tres
  archivos y dos subdirectorios; conteos exactos; contenido exacto de `GetFiles`/`GetDirectories`
  buscado SIN asumir ningun orden particular (Win32 no lo garantiza); exclusion cruzada
  (`GetFiles` nunca devuelve un subdirectorio, `GetDirectories` nunca devuelve un archivo ni `.`/
  `..`); buffer mas chico que la cantidad real (el unico elemento escrito debe ser uno de los
  nombres reales); directorio inexistente colapsando a 0 en las cuatro operaciones sin crashear;
  limpieza completa al final.
- Ejecucion REAL bajo Wine, a O0 y O2, con un directorio de trabajo dedicado y vacio -- misma
  disciplina que cada ronda de Stage 53 desde 53.3. Confirmacion independiente desde el lado
  Linux: el directorio de trabajo queda completamente vacio despues de correr (igual que el
  chequeo de 53.4), testigo real de que la limpieza de `Main.ns` de verdad borro todo lo que creo.
- Control negativo manual: se corrio una copia del ejemplo con un chequeo de conteo deliberadamente
  roto (`fileCount != 999` en vez de `!= 3`) para confirmar que el programa realmente discrimina
  (exit code 1, no un exit 0 vacio) antes de escribir el harness de test dedicado.
- 45/45 chequeos del test dedicado, estable en tres corridas.
- Suite completa: 217/218 -- el unico fallo (`nsc.stage52-4.owned-cleanup-runtime`) es el mismo
  hallazgo preexistente ya documentado en `55_stage53_5_command_line_args.md`, no relacionado con
  esta ronda.

## Sincronizacion de simbolos Win32 en las cinco ubicaciones

`FindFirstFileA`/`FindNextFileA`/`FindClose` se agregaron a `kernel32Symbols` en las CINCO
ubicaciones que deben mantenerse sincronizadas cada vez que el modulo runtime gana nuevos imports
Win32 -- patron descubierto y documentado durante Stage 53.5 (ver ese doc para el detalle
completo): `WindowsExecutableBuilder.cpp`, `WindowsLibraryBuilder.cpp`, y las copias LOCALES de
`Stage47HostCAbiTests.cpp`, `Stage48_3CHostIntegrationTests.cpp`,
`Stage48_4CppHostIntegrationTests.cpp` (los tres tests de host-integration que linkean
directamente contra la biblioteca estatica, bypaseando el linker del driver). Esta vez se
sincronizaron las cinco ubicaciones desde el principio, no como una correccion posterior.

## Cierre de Stage 53

Con GetFileCount/GetFiles/GetDirectoryCount/GetDirectories, Stage 53 "Runtime esencial (I/O,
tiempo, entorno)" queda formalmente completo: Time (53.1), Environment (53.2), Environment
variables (53.2b), Filesystem/Paths (53.3, 53.3b, 53.4), Command-line arguments (53.5), y ahora
Directory enumeration (53.6). Stage 54 (threading) permanece bloqueado hasta que Alberto de la
senal explicita de arrancarlo.
