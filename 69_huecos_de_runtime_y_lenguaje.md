# 69 — Huecos de runtime y de lenguaje, medidos contra el árbol actual

**Fecha:** 2026-08-30, después de cerrar los bugs reportados (ver
`68_triage_y_cierre_de_bugs_reportados.md`).
**Método:** medido contra el árbol, no de memoria. Los huecos de lenguaje salen de
`include/nativesharp/Lexer/TokenKind.h` (la lista completa de keywords y de puntuación); los de
runtime, de leer las APIs públicas de `lib/NativeSharp.Core*`.
**Alcance:** el compilador y su biblioteca. Bindings y ports son de otras instancias.

---

## 1. Lo que ya hay, para que el resto se lea en contexto

No es poco, y conviene decirlo antes de la lista de faltantes.

**Lenguaje:** clases, structs, interfaces, enums, delegates, `event`, propiedades, indexers,
sobrecarga de operadores, conversiones `implicit`/`explicit`, `virtual`/`override`/`abstract`/
`sealed`, genéricos monomorfizados, lambdas con captura, excepciones, `foreach`, `var` (existe y
sí infiere), `ref`/`out`/`in`, `raii`/`owned`/`borrowed`/`move`, `is`/`as`, `extern C`/`export`,
`stackalloc`, `sizeof`, inicializadores agregados (`V v = {1,2}`), raíz de tipos con
`ToString`/`Equals`/`GetHashCode` y sin boxing.

**Biblioteca:** `String` muy rica (mezcla de C# y Python), colecciones genéricas completas
—`List`, `Dictionary`, `HashSet`, `SortedSet`, `SortedList`, `SortedDictionary`, `LinkedList`,
`Queue`, `Stack`, `PriorityQueue`— con `Sort`, `BinarySearch`, `Find*` incluidos, `Random`,
`DateTime`/`TimeSpan`, `NumericLimits`, threading completo (threads, atomics, mutex, monitor,
semáforo, evento, scoped lock), `Path`/`File`/`Directory`, `Process`/`Stopwatch`/`SystemInfo`.

---

## 2. Huecos de lenguaje

Verificado contra `TokenKind.h`. **No existe el token, no sólo la característica.**

| Falta | Consecuencia concreta |
|---|---|
| **`switch` / `case`** | Un `WndProc` es una cadena `if / else if` de cuarenta ramas, sin jump table. Es *el* patrón de Win32. `default` sí existe como keyword y se puede reusar. |
| **`?:`** | No existe el token `?` en absoluto. Tampoco `??` ni `?.`. |
| **`do…while`**, `goto` | No existen. |
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

**`switch` y `?:` son los dos que están deformando código real ahora mismo**, y son los más
baratos de la lista.

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

## 7. Preguntas de alcance que se contestaron midiendo

### 7.1 ¿Se puede hacer NativeForms, un clon de WinForms sobre Win32?

**Sí, y las piezas difíciles ya están.** `extern C from "user32.dll"` con identidad de import por
triple; `extern C delegate` como puntero a función crudo **con thunk cacheado por objetivo**
—crítico, porque `SetWindowLongPtr` y `UnhookWindowsHookEx` comparan punteros—; el **borde de
excepciones** en callbacks, sin el cual un `throw` dentro de un `WndProc` desenrolla el message
pump de Windows; structs con layout exacto, arrays embebidos y unions para `MSG`, `WNDCLASSEXW`,
`PAINTSTRUCT`, `INPUT`; clases con `virtual`/`override`/`abstract`, interfaces, propiedades,
`event`.

**Lo que falta antes de escribir la primera línea:** `switch` (§2), `Encoding` UTF-16 (§4), y sobre
todo **la decisión de ownership del árbol de controles**. En WinForms `Controls` es dueño de los
hijos, `Parent` es un back-pointer, y cada handler captura referencias cruzadas. Con single-owner
y sin `shared` el diseño tiene que quedar fijado así: **el `Form` posee todo el árbol; `Parent` y
todo lo demás es `borrowed`; los handlers son métodos del propio `Form`.** Es una restricción real,
no un bloqueo, pero decidirla después de tener treinta controles escritos sería carísimo.

**La decisión de arquitectura:** HWND real por control más comctl32 (teclado, foco, IME,
accesibilidad, DPI y scrollbars gratis, look nativo por definición) contra el modelo FLTK de un
solo HWND y todo dibujado a mano (control total, cero comctl32, y reimplementar todo eso). Para
algo que se llame clon de WinForms, la respuesta es la primera con owner-draw sólo donde haga
falta: WinForms mismo es exactamente eso, y FLTK eligió la segunda y por eso se ve como FLTK.

**Nota de alcance:** los docs `01`/`06` fijan Win32/OpenGL/Vulkan/DirectX y "nunca aplicaciones de
soporte comercial". Un toolkit GUI no es engine, y para herramientas de gamedev —editor de niveles,
empaquetador de assets— se justifica solo. Conviene declararlo explícito para que no se cuele scope
de app de negocio por la puerta de atrás.

### 7.2 Audio e imagen internos

**Vale la pena nativo**, barato y sin dependencias: parser/writer de **WAV/RIFF**; **BMP, TGA, PPM,
QOI**; **PNG** en cuanto haya inflate y CRC32 (§5); y un **mezclador propio** (float32, N voces,
resample lineal) — el mixer es tuyo por definición, ninguna librería lo va a dar como se quiere.

**No vale la pena nativo:** JPEG (un decoder decente son 2–3k líneas), MP3, OGG. `stb_image`,
`dr_mp3`/`minimp3` y `stb_vorbis` son single-header C, que es justo lo que nsbind consume.

**Backend de audio:** `waveOut` es C plano y funciona hoy sin COM, pero con 30–60 ms de latencia,
inaceptable para un juego. WASAPI y XAudio2 son COM. OpenAL es la salida C plana buena.

### 7.3 Skia

Skia es C++ y su API C oficial (`sk_*.h`) está incompleta. SkiaSharp funciona porque existe
`libSkiaSharp`, que expone una API C plana grande — **eso sí es bindeable**, sin COM ni shims.

El problema no es técnico sino de costo: compilar Skia pide depot_tools/GN/Ninja, el binario ronda
8–12 MB, la superficie de API es enorme, y ata al versionado de un tercero gigante. Para un
lenguaje ligero de juegos es desproporcionado.

Alternativas con ~90% del valor y una fracción del costo: **NanoVG** (dos archivos C, dibuja
vectores sobre el contexto GL que de todos modos va a existir) si lo que se quiere es un canvas 2D
para UI y juegos 2D; **Blend2D** (API C, MIT, CPU) si se quiere calidad de impresión;
**stb_truetype** para texto. Skia sólo si hace falta su ecosistema completo.

### 7.4 GDI+ / System.Drawing — el dato que cambia el cálculo

**GDI+ no es COM.** `gdiplus.dll` exporta una API C plana (`gdiplusflat.h`: `GdiplusStartup`,
`GdipCreateBitmapFromFile`, `GdipDrawImageRectI`, `GdipDrawString`…). El `System.Drawing` de .NET es
literalmente un wrapper sobre esa API.

Es decir: **consumible hoy, con `extern C from "gdiplus.dll"`, sin una sola primitiva nueva del
compilador.** Da de un jalón cargar y guardar BMP/JPEG/PNG/GIF/TIFF, líneas, curvas, paths y texto
con antialiasing, y un `NativeSharp.Drawing` con `Bitmap`/`Graphics`/`Pen`/`Brush`/`Font`/`Color`/
`Rectangle`. Sin aceleración por hardware y más lento que Direct2D, y Microsoft lo trata como
legacy, pero está en todo Windows desde XP. Necesita `GdiplusStartup`/`Shutdown` envueltos en RAII,
y encaja perfecto con el `OnPaint` de NativeForms.

**Es la ruta más corta a imagen y dibujo 2D, y ya está disponible.**

### 7.5 Imagen nativa de Windows = WIC

`IWICImagingFactory` **sí es COM**. Decodifica y codifica PNG/JPEG/GIF/TIFF/BMP/DDS/HEIF (WebP en
Win11), convierte formatos de pixel, escala y lee metadatos, sin un solo tercero.

Y aquí está la conexión que importa: **la fundación COM que ya hace falta para DXGI/D3D11
desbloquea de golpe WIC + WASAPI + XAudio2 + Media Foundation + DirectWrite + Direct2D.** Es
probablemente la inversión de compilador con mayor retorno de toda esta lista, y el único punto
donde el compilador tiene que crecer para nada de esto.

---

## 8. Orden sugerido

1. `switch` + `?:` + `do…while`, y re-medir `x += y`.
2. Texto y números: `ToString(float/double)`, `Parse`/`TryParse`, `StringBuilder`, `Console`.
3. `Encoding` + `Convert`/Base64 + `BitConverter` + `Span<T>` + `Guid`.
4. `Stream` / `BinaryReader` / `ReadAllBytes`.
5. **Inflate/Deflate + CRC32** → PNG, zip, gzip de un golpe.
6. GDI+ flat → `NativeSharp.Drawing` (ya posible, retorno inmediato).
7. Winsock2 + WinHTTP.
8. **Fundación COM** → D3D11, WIC, WASAPI, DirectWrite.
9. NativeForms sobre 1 + 3 + 6.
10. NanoVG o Blend2D antes de considerar Skia.
