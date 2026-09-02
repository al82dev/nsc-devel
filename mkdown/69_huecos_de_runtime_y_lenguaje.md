# 69 — Huecos de runtime y de lenguaje, medidos contra el árbol actual

**Fecha:** 2026-08-30, después de cerrar los bugs reportados (ver
`68_triage_y_cierre_de_bugs_reportados.md`).
**Método:** medido contra el árbol, no de memoria. Los huecos de lenguaje salen de
`include/nativesharp/Lexer/TokenKind.h` (la lista completa de keywords y de puntuación); los de
runtime, de leer las APIs públicas de `lib/NativeSharp.Core*`.
**Alcance:** el compilador y su biblioteca — `src/`, `include/`, `lib/`, `tests/`. **Nada más.**
Qué hacer con estos huecos desde fuera —qué bindear, qué envolver, qué construir encima— no se
decide aquí y no está en este documento.

---

## 1. Lo que ya hay, para que el resto se lea en contexto

No es poco, y conviene decirlo antes de la lista de faltantes.

**Lenguaje:** clases, structs, interfaces, enums, delegates, `event`, propiedades, indexers,
sobrecarga de operadores, conversiones `implicit`/`explicit`, `virtual`/`override`/`abstract`/
`sealed`, genéricos monomorfizados, lambdas con captura, excepciones, `foreach`, `var` (existe y
sí infiere), `ref`/`out`/`in`, `raii`/`owned`/`borrowed`/`move`, `is`/`as`, `extern C`/`export`,
inicializadores agregados (`V v = {1,2}`), raíz de tipos con
`ToString`/`Equals`/`GetHashCode` y sin boxing, y —**desde el milestone CC1**— `sizeof`,
`stackalloc`, `new T` escalar y aritmética de punteros.

> **Corrección (tarea 69).** Esta lista incluía `stackalloc` y `sizeof` **antes de que existieran
> de verdad**. Las dos parseaban y tipaban, y morían al bajar a código: el HIR no tenía caso para
> ninguna. Yo las di por presentes tras mirar sólo la mitad delantera del pipeline — el mismo
> error de inferencia que luego cometió un consumidor externo con otra cosa, y que este documento
> tiene que dejar de fomentar. **Que una forma parsee y tipe no significa que compile.**
>
> Las fases 4A–4D del milestone CC1 las implementaron de verdad, atravesando las cuatro capas, y
> hay fixtures que las EJECUTAN (`c9_sizeof`, `c10_stackalloc`, `c11_raw_new`,
> `c12_pointer_arith`, todas en 42 a `-O0` y `-O2`). Sólo por eso están ahora en esta lista.
>
> El documento de auditoría externa cometió el error simétrico: afirmaba que las dos existían «en
> el pipeline clásico». Barrido completo del árbol: **no había implementación clásica de ninguna
> de las dos.** Ver `75_cc1_fase4a_sizeof.md` §2.1 y `76_cc1_fases_4b_4c_4d_4e.md` §2.2.

**Biblioteca:** `String` muy rica (mezcla de C# y Python), colecciones genéricas completas
—`List`, `Dictionary`, `HashSet`, `SortedSet`, `SortedList`, `SortedDictionary`, `LinkedList`,
`Queue`, `Stack`, `PriorityQueue`— con `Sort`, `BinarySearch`, `Find*` incluidos, `Random`,
`DateTime`/`TimeSpan`, `NumericLimits`, threading completo (threads, atomics, mutex, monitor,
semáforo, evento, scoped lock), `Path`/`File`/`Directory`, `Process`/`Stopwatch`/`SystemInfo`.

---

## 2. Huecos de lenguaje

Verificado contra `TokenKind.h`. **No existe el token, no sólo la característica.**

**Actualización 2026-08-31.** Los tres primeros de esta tabla **ya se cerraron**: `?:` y `do…while`
en la ronda del doc 71 (verde en Windows real), `switch`/`case` en la del doc 72. Se dejan tachados
en vez de borrados porque el resto del documento razona sobre ellos.

| Falta | Consecuencia concreta |
|---|---|
| ~~**`switch` / `case`**~~ **HECHO — doc 72** | Era *el* patrón de Win32: un `WndProc` era una cadena `if / else if` de cuarenta ramas, sin jump table. Ahora baja a una `LLVMBuildSwitch`. `default` se reusó como se preveía. |
| ~~**`?:`**~~ **HECHO — doc 71** | El token `?` ya existe. `??` y `?.` siguen sin existir: ahora el obstáculo es de diseño, no de lexer. |
| ~~**`do…while`**~~ **HECHO — doc 71** · `goto` | `goto` no existe y no está planeado. |
| `params` | Sin argumentos variádicos idiomáticos. |
| Interpolación de strings | Cada línea de texto se arma a mano. Ver §3. |
| `sbyte` | No está en la lista de builtins. |
| `Nullable<T>` | No existe, y sin `?` tampoco tendría sintaxis. |
| Structs genéricos (`Vector2<T>`) | No se tocó en C6. |
| `readonly struct` / `in` en el receptor | El compilador no puede evitar copias del receptor. |
| Union anónima dentro de struct | La nombrada ya expresa `INPUT` de Win32; la anónima es azúcar. |
| Alias genéricos (`using Pair<T> = …`) | Marcado como "después" en el roadmap del compilador. |
| Inferencia de argumentos de tipo en genéricos | Contrato negativo explícito de Stage 44. |
| Propiedades sólo-`set` | Hueco conocido; el motivo real está en §6.3 del doc 68. |
| `x += y` en lowering OOP | Reportado roto contra C1–C5 por el port de saru. **Hay que re-medirlo contra C8** antes de darlo por vivo. |

~~**`switch` y `?:` son los dos que están deformando código real ahora mismo**, y son los más
baratos de la lista.~~ — cerrados. Lo más caro que queda de esta tabla es la interpolación de
strings, y depende de §3 (texto y números), no del lenguaje.

---

## 3. Huecos de runtime — texto y números

Éste es el bloque que más cuesta en el día a día, y el que menos se nota en una lista de features.

- **`String.ToString(float)` y `(double)` no existen.** Hay `ToString` para `bool`, `char`, `int`,
  `long`, `ulong` y `string`. **No se puede imprimir un flotante.** Para un lenguaje cuyo objetivo
  declarado son los videojuegos, eso significa que no se puede imprimir un `Vector3`.
- **No hay parsing numérico.** Ni `Parse` ni `TryParse` para ningún tipo. No se puede leer un
  número de un archivo de texto ni de un archivo de configuración.
- **No hay `StringBuilder`,** y `String.Format` sólo acepta `string[]`, así que todo argumento hay
  que preconvertirlo — y un `double` no se puede convertir. La evidencia está en el propio
  `book_examples/01`: imprimir **una** línea cuesta doce `String.Concat` encadenados con doce
  `raii`.
- **`Console` tiene exactamente un método:** `WriteLine(string)`. No hay `Write`, ni sobrecargas
  numéricas, ni entrada. Ahora que C8 dio `ToString()` a la raíz, un `WriteLine(Object)` es casi
  gratis.

## 4. Huecos de runtime — datos e IO

- **`Encoding`** (UTF-8 ↔ UTF-16 ↔ ANSI). Sin esto, cada binding de Win32 `-W` reimplementa
  `MultiByteToWideChar` a mano. Es prerrequisito de cualquier GUI.
- **`Convert` / Base64 / Hex**, **`BitConverter` con endianness explícito**, **`Guid`** (necesario
  para COM: IID y CLSID), **`Span<T>` / `ReadOnlySpan<T>`** — con `borrowed` la semántica de
  ownership ya está, falta el tipo; es lo que evita copias en todo parsing binario.
- **Algoritmos sobre `T[]` crudo**: `Sort`, `BinarySearch`, `LowerBound`, `Reverse`, `Fill`,
  `Copy`, `Resize`. Existen como métodos de `List<T>`, no como algoritmos libres.
- **IO real.** Hoy `File` sólo tiene `ReadAllText`/`WriteAllText`/`AppendAllText`/`Copy`/`Move`/
  `Delete`/`Exists`: **no hay `ReadAllBytes` y no hay streams.** Sin `Stream`, `BinaryReader` y
  `BinaryWriter` no se puede leer un PNG, un WAV ni un OBJ grandes, ni streamear audio. Después,
  memory-mapped files (`CreateFileMapping`) para assets, que es API C plana.

## 5. Compresión — la pieza de mayor apalancamiento

No existe nada. Un **inflate/deflate nativo (~600 líneas)** más **CRC32** desbloquea de un solo
golpe: decodificar **PNG** sin terceros, leer **.zip** para paquetes de assets, y **gzip** con el
`Content-Encoding` de HTTP.

Windows trae `compressapi.h` en Cabinet.dll (XPRESS/MSZIP/LZMS, API C plana), pero **no lee
zip/gzip estándar**, así que no sustituye a inflate.

## 6. Red

Cero hoy. Son dos piezas separadas y conviene no confundirlas:

1. **Winsock2** — C plano, consumible ya con `extern C from "ws2_32.dll"`: `Socket`,
   `TcpListener`/`TcpClient`, `UdpClient`, `IPEndPoint`, `getaddrinfo`. Para juegos lo que importa
   es UDP más un canal confiable propio encima.
2. **Descargas: no meter curl.** `WinHTTP` (`winhttp.dll`) es API C plana, viene en todo Windows,
   hace HTTPS con el certificate store del sistema y no arrastra OpenSSL.
   `HttpClient.GetBytes(url)` son ~300 líneas de binding. curl obligaría a distribuir libcurl más
   su cadena de TLS.

Encaja con el posicionamiento —multiplayer, descarga de assets y parches—; no es "web".

Falta también **serialización**: JSON pequeño y/o un formato binario propio, para save-games y
payload de red.

---

## 7. La única pieza de esto que es del compilador: COM

Todo lo demás de esta lista se resuelve dentro de `lib/NativeSharp.Core*` con lo que el lenguaje ya
sabe hacer. **COM no.** Consumir una interfaz COM son vtables, `IUnknown`, `HRESULT` y
`QueryInterface`, y `extern C` no alcanza: hoy se resuelve generando thunks fuera del compilador,
y el lenguaje no lo expresa.

Es el único punto de esta lista donde el compilador tiene que crecer, y es la inversión de mayor
retorno que le queda: una fundación COM desbloquea de una sola vez todo lo que en Windows vive
detrás de esa ABI.

Qué haría falta, a grandes rasgos: representar una vtable de interfaz COM como tipo, la convención
de llamada por slot con `this` primero, `HRESULT` como tipo de retorno con su regla de error, y
`GUID` como literal y como valor — que es también por lo que `Guid` aparece en §4.

## 8. Orden sugerido

1. ~~`switch` + `?:` + `do…while`~~ **hecho** (docs 71 y 72); falta re-medir `x += y`.
2. Texto y números: `ToString(float/double)`, `Parse`/`TryParse`, `StringBuilder`, `Console`.
3. `Encoding` + `Convert`/Base64 + `BitConverter` + `Span<T>` + `Guid`.
4. `Stream` / `BinaryReader` / `ReadAllBytes`.
5. **Inflate/Deflate + CRC32.**
6. Winsock2 + WinHTTP.
7. Serialización.
8. **Fundación COM** (§7).
