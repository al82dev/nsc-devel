# 100 — Bloque 4, fase A: E/S binaria de archivos

**Fecha:** 2026-09-02
**Alcance de la fase:** `File.GetSize` · `File.ReadAllBytes` · `File.ReadBytes` (buffer del
llamador) · `File.WriteAllBytes` · `Environment.CurrentDirectory`.
**Estado:** **fase cerrada.** Las cinco operaciones implementadas y verificadas en el contenedor;
la ejecución real la mide `all.bat` en Windows.

---

## 1. Por qué esta fase va primera aunque sea la más pequeña

El doc 98 lo dice y conviene repetirlo aquí: es la primera vez que una operación núcleo pasa algo
que no es `string`, `int` o un puntero de cadena a una API de Win32. Si la forma de esa frontera
tiene un problema, aparece en cuatro funciones y no dentro de `BinaryReader`.

Y hay una segunda razón, que resultó ser la de verdad: **la cadena de seis capas tiene un tramo que
falla en silencio**, y descubrirlo aquí cuesta una función. Ver §4.

---

## 2. `File.GetSize` — qué es y qué devuelve

```csharp
public static long GetSize(string path)
```

Operación núcleo: `CreateFileA` (OPEN_EXISTING, sólo lectura) + `GetFileSizeEx` + `CloseHandle`.
Devuelve cuánto ocupa el archivo **sin leerlo**.

**Por qué el driver la necesita.** Decide con este número: cuánto buffer reservar para un `.obj`,
si saltarse un archivo vacío, si algo no cabe en memoria. Deducirlo leyendo el archivo entero es
exactamente lo contrario de lo que se busca.

### 2.1 Devuelve −1, y ahí me separo del resto de la familia

`File.Exists` colapsa cualquier fallo en `false`; `File.ReadAllText`, en la cadena vacía. Los dos
tienen su motivo escrito: allí el valor de fallo no estorba porque no puede confundirse con un
resultado legítimo.

Aquí sí puede. **`0` es el tamaño de un archivo vacío**, que existe y es un caso normal. Devolver
`0` para «no se pudo abrir» haría indistinguible un archivo vacío de uno ausente — y esa distinción
es justo la que el driver necesita. `-1` no puede ser el tamaño de nada.

### 2.2 Por eso sí se mira el BOOL de `GetFileSizeEx`

`buildFileReadAllText` lo descarta, con su motivo escrito: allí el peor caso al ignorarlo es leer
cero bytes. Aquí **el resultado ES el tamaño**, y un cero silencioso sería devolver un dato falso.
Es la misma función de Win32 tratada distinto porque el consumidor es distinto, y eso se documenta
en el sitio en vez de dejar dos criterios que parezcan una incoherencia.

### 2.3 `long` y no `int`

Un `.obj` de LLVM pasa de los 2 GiB sin esfuerzo en un proyecto grande, y `GetFileSizeEx` devuelve
64 bits de verdad. Truncar sería inventar un límite que ni Windows ni el formato tienen.
`FileReadAllText` **sí** trunca y lo dice: allí es aceptable porque el resultado es una cadena que
se va a manipular en memoria; aquí no hay arreglo aguas arriba.

### 2.4 El handle se cierra antes de la bifurcación

```llvm
have_handle:
  %size_ok_flag = call i32 @GetFileSizeEx(ptr %handle, ptr %size_slot)
  %1 = call i32 @CloseHandle(ptr %handle)
  br i1 %size_failed_flag, label %size_failed, label %size_ok
```

En los dos caminos. Un `return` temprano que se deja el handle abierto es una fuga que no da la
cara hasta que el proceso lleva horas corriendo.

---

## 3. Los sitios que hay que tocar, enumerados

Para la siguiente operación núcleo, y para no volver a buscarlos:

| # | Archivo | Qué |
|---:|---|---|
| 1 | `include/…/ProjectMethodBodyModel.h` | `MethodCoreOperationKind::FileGetSize` |
| 2 | `src/Frontend/ProjectMethodBodyModel.cpp` | reconocimiento por clase + nombre + firma; y el `case` del nombre |
| 3 | `include/…/ProjectOopHirModel.h` | `ProjectOopHirNodeKind::CoreFileGetSize` |
| 4 | `src/Frontend/ProjectOopHirModel.cpp` | nombre · **lista de «es operación núcleo»** · **lista de aridad** · mapeo |
| 5 | `include/…/ProjectOopMirModel.h` | `ProjectOopMirNodeKind::CoreFileGetSize` |
| 6 | `src/Frontend/ProjectOopMirModel.cpp` | nombre + mapeo HIR→MIR |
| 7 | `src/LLVM/ProjectOopLlvmLowerer.cpp` | nombre del hook · tipo · `LLVMAddFunction` · la guarda de «todos los hooks existen» · el `case` de bajada · los dos miembros |
| 8 | `src/Runtime/WindowsRuntimeBuilder.cpp` | `buildFileGetSize` + el cableado en `buildInternal` |
| 9 | `lib/NativeSharp.Core.IO/File.ns` | el método, con cuerpo trivial |
| 10 | `src/Linker/WindowsRuntimeImports.cpp` | **sólo si la operación importa algo de Win32 que no se usaba antes** — ver doc 101 §3.1 |

Son **diez archivos y quince sitios** para una función. El décimo se descubrió al implementar
`Environment.CurrentDirectory`, y su omisión rompió los 86 trabajos de una corrida completa con
`undefined symbol: GetCurrentDirectoryA`: declarar el import en `WindowsRuntimeBuilder` sólo dice
que el símbolo existe; quién le dice al enlazador de qué DLL sacarlo es esa lista. El `/WX` y el `switch` sin `default`
atrapan la mayoría si se olvida uno — pero no todos, y ese es el punto siguiente.

---

## 4. EL HALLAZGO: hay dos listas, y una falla en silencio

`ProjectOopHirModel.cpp` tiene **dos** listas de `MethodCoreOperationKind`, separadas por unas cien
líneas:

* La de **~2596** decide que esto ES una operación núcleo cuyos operandos salen del AST.
* La de **~2746** decide **cuántos operandos** se esperan, y su valor por omisión es **2**.

**Estar sólo en la primera no produce ningún error.** Ni de compilación, ni de enlace, ni un
diagnóstico. El hook aparece **declarado en el IR sin una sola llamada**, y la llamada se queda
siendo el método NativeSharp normal — que en este caso devuelve `-1`, o sea que el programa
compila, corre, y da un resultado plausible y falso.

Así se vio, y es lo que hay que buscar si vuelve a pasar:

```
declare i64 @__nativesharp_file_get_size(ptr)          <- el hook existe
%static.call = call i64 @__nsc_method_…File__GetSize…  <- pero nadie lo llama
```

**Estar sólo en la segunda** da un error, pero no ayuda:

```
loader-error: Stage 51 string intrinsic received an invalid operand count
```

No menciona la operación, ni el archivo, ni cuál de las dos listas falta. Y dice «string
intrinsic» sobre algo que no tiene nada que ver con cadenas.

Queda escrito en las dos listas, en el código, con este caso como ejemplo. **La comprobación que
de verdad cierra esto** —una prueba que recorra el enum y exija que cada valor esté en ambas
listas— no se escribió: es una tarea aparte y se anota, no se cuela aquí.

---

## 5. Cómo se comprobó `File.GetSize`

Al implementarla sola, antes de las otras cuatro: `--stage=llvm` OK, dos `call i64
@__nativesharp_file_get_size` en el IR y **cero** llamadas al método normal, la definición del hook
en el runtime con el `CloseHandle` en el camino común, y el barrido en **167 OK / 5 FAIL** — la
línea base de entonces. La comprobación de la fase entera está en §8.

---

## 6. Las otras cuatro operaciones

### 6.1 `File.ReadAllBytes(string) -> byte[]`

```
CreateFileA (OPEN_EXISTING) + GetFileSizeEx + ReadFile + CloseHandle
```

**Es el mismo IR que `__nativesharp_file_read_all_text`, y no es pereza.** Un `byte[]` y un
`string` de NativeSharp tienen el **mismo layout**: `{ i64 longitud; los datos justo detrás }`. Se
comprobó mirando el IR de `new byte[8]`, que pide **16** bytes — 8 de cabecera y 8 de datos. Leer
un archivo entero a memoria produce esa forma, se llame como se llame.

**Por qué entonces dos hooks y no uno.** Porque quien los distingue es el **frontend**, por la clave
de tipo del valor devuelto (`builtin:byte[]` contra `builtin:string`), y esa distinción tiene que
sobrevivir hasta el IR para que el código de arriba sepa qué tiene. Compartir el símbolo obligaría
a que la bajada eligiera el tipo de salida por su cuenta, que es justo la clase de decisión que no
debe estar ahí. El coste son sesenta líneas de IR duplicadas en el runtime; el beneficio es que el
tipo lo decide quien lo sabe.

**Devuelve un arreglo VACÍO si no se pudo abrir, no nulo.** Devolver nulo obligaría a cada llamador
a comprobarlo antes de mirar `.Length`. Quien necesite distinguir «vacío» de «no existe» tiene
`Exists` y `GetSize`, que para eso devuelve −1.

### 6.2 `File.ReadBytes(string, byte[]) -> int` — la que no asigna

**Es la que usará el driver.** Leer cien `.obj` con `ReadAllBytes` son cien asignaciones; con un
buffer reutilizado, ninguna. Es el patrón «buffer provisto por el llamador» que
`Directory.GetFiles(string, string[])` ya estableció.

Lee **el menor** entre el tamaño del archivo y la capacidad del destino, y devuelve **lo que
`ReadFile` dice que leyó** — no lo que se le pidió: son distintos si el archivo se acorta entre
`GetFileSizeEx` y `ReadFile`, y el dato honesto es el segundo.

**Un buffer corto NO es un error.** Se lee el principio y se dice cuánto; comparar lo devuelto con
`GetSize` dice si se quedó corto. Fallar obligaría a dos llamadas siempre —tamaño y lectura— para
el caso normal en que el buffer sobra.

### 6.3 `File.WriteAllBytes(string, byte[], int) -> bool`

`CREATE_ALWAYS`, igual que `WriteAllText`: el archivo termina conteniendo **sólo** lo que se
escribe ahora.

**El contador es explícito y no la longitud del arreglo.** Quien construye un buffer casi siempre
lo tiene más grande que lo que ha llenado, y obligarle a recortarlo sería una copia entera por
escritura. Se recorta a la longitud real del arreglo: pedir más es pedir que se lea memoria ajena y
se meta en un archivo.

**Devuelve `bool`, y aquí sí se mira el BOOL de `WriteFile`** — a diferencia de `WriteAllText`, que
lo descarta. Mismo criterio que en `GetSize`: escribir un `.obj` a medias y seguir produce un enlace
que falla mucho después, con un error que no menciona la escritura. Y es verdadero sólo si la API
dijo que sí **y** se escribió todo lo pedido: **un disco que se llena a mitad devuelve TRUE con
menos bytes escritos**.

### 6.4 `Environment.CurrentDirectory() -> string`

Cierra `current_path`, la última de las siete operaciones de filesystem que el doc 81 midió.

`GetCurrentDirectoryA` es **el único import de Win32 que esta fase añade**: las otras tres
reutilizan `CreateFileA`, `GetFileSizeEx`, `ReadFile`, `WriteFile` y `CloseHandle`, que ya estaban
para las variantes de texto.

Mismo idioma de dos llamadas que `Path.GetFullPath`. Se guarda la longitud que devuelve la
**segunda** llamada, no `requiredSize - 1`: si el directorio cambiara entre las dos, el dato honesto
es el de la segunda.

---

## 7. La fixture, que es además el guardián

`tests/cc1/c24_binary_file_io`, salida esperada **42**, registrada en los tres sitios que hay que
tocar: `run_cases.py`, `nslink_pipeline.py` (que lleva su propia copia de la tabla, doc 95 §6) y
`Cc1CorrectnessTests.cpp`.

**Las cinco operaciones se enfrentan ENTRE SÍ** en vez de compararse contra números escritos a
mano: se escribe un archivo, se pregunta su tamaño, se lee de las dos formas, y las tres respuestas
tienen que coincidir. Si algún día una se desalinea de las otras, se desalinea ahí.

Los once pasos, cada uno con su propio código de salida para que el reporte diga **cuál** se rompió
sin abrir un depurador:

| # | Qué mide |
|---|---|
| 1-2 | escribir y que `GetSize` coincida con lo escrito |
| 3-7 | `ReadAllBytes` devuelve la misma longitud y los cuatro bytes exactos |
| 8-11 | `ReadBytes` en un buffer **más grande**: devuelve lo que hay, no la capacidad, y no toca lo de más allá |
| 12-14 | `ReadBytes` en un buffer **más pequeño**: lee el principio y lo dice |
| 15-16 | escribir **menos** de lo que el arreglo tiene: manda el contador |
| 17-18 | pedir escribir **más**: se recorta a la longitud real |
| 19 | archivo ausente → **−1** |
| 20 | y `ReadAllBytes` de un ausente → arreglo vacío, no nulo |
| 21-22 | archivo **vacío** → **0**, que es lo que el −1 permite distinguir |
| 23 | `CurrentDirectory` no vacío |

**El patrón de bytes es `0, 127, 128, 255` a propósito:** el cero, el último positivo con signo, el
primero que un `sbyte` vería negativo, y el máximo. Si en algún punto de la cadena un byte se trata
como con signo, `128` y `255` lo delatan.

**Y por eso es el guardián del hueco de §4.** Una operación núcleo que se quede fuera de las dos
listas cae al método de la biblioteca, cuyo cuerpo devuelve el valor trivial —`-1`, `0`, `false`,
arreglo vacío—. La fixture compara justo esos valores contra el comportamiento real, así que un
hook que no se emite sale como un código de salida concreto en vez de pasar desapercibido. **No
hace falta la comprobación mecánica que se anotó en §4: la fixture la cubre para estas cinco.** Para
las futuras, sigue anotada.

---

## 8. Cómo se comprobó

En el contenedor, sobre el frontend real:

* `--stage=llvm` de la fixture c24: **OK**.
* `--stage=ir`: **los cinco hooks llamados** — `file_get_size` ×6, `file_read_all_bytes` ×3,
  `file_read_bytes` ×3, `file_write_all_bytes` ×5, `environment_current_directory` ×2 — y **cero**
  llamadas a los métodos de la biblioteca.
* `--stage=runtime`: **las cinco definiciones** presentes.
* Barrido de los `.nsproj` del árbol en `bodies` y en `llvm`: **168 OK / 5 FAIL** — uno más que
  antes, que es c24, y los mismos cinco negativos intencionados.

**Lo que esto NO demuestra:** que funcione. El contenedor no enlaza ni ejecuta en Windows, así que
ni `GetFileSizeEx` ni `ReadFile` ni `WriteFile` ni `GetCurrentDirectoryA` se han llamado nunca. Lo
medido es que el compilador **emite lo que debe emitir**. La ejecución la mide `all.bat`, y hasta
entonces la fase está escrita, no verificada.

---

## 9. Qué se tocó

| Archivo | Qué |
|---|---|
| `include/…/ProjectMethodBodyModel.h` | 5 valores nuevos de `MethodCoreOperationKind` |
| `src/Frontend/ProjectMethodBodyModel.cpp` | reconocimiento de las 5 + sus nombres |
| `include/…/ProjectOopHirModel.h` · `…MirModel.h` | 5 nodos en cada uno |
| `src/Frontend/ProjectOopHirModel.cpp` | nombres, **las dos listas**, el bucket de cero argumentos y los mapeos |
| `src/Frontend/ProjectOopMirModel.cpp` | nombres y mapeos HIR→MIR |
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | 5 hooks: nombre, tipo, declaración, guarda, `case` de bajada y miembros |
| `src/Runtime/WindowsRuntimeBuilder.cpp` | `buildFileGetSize`, `buildFileReadAllBytes`, `buildFileReadBytes`, `buildFileWriteAllBytes`, `buildEnvironmentCurrentDirectory`, el import `GetCurrentDirectoryA` y el cableado |
| `lib/NativeSharp.Core.IO/File.ns` | `GetSize`, `ReadAllBytes`, `ReadBytes`, `WriteAllBytes` |
| `lib/NativeSharp.Core/Environment.ns` | `CurrentDirectory` |
| `tests/cc1/c24_binary_file_io/` | **nueva** fixture |
| `tests/unit/Cc1CorrectnessTests.cpp` · `run_cases.py` · `nslink_pipeline.py` | la fixture registrada en los tres sitios |

---

## 10. Lo que sigue

**Fase B** — `ByteBuffer`, `BinaryReader`, `BinaryWriter` sobre `Span<byte>`. La decisión que hay
que tomar **antes** de la primera firma sigue siendo la del doc 98: leer más allá del final devuelve
`Optional<T>`, no lanza. Un `BinaryReader` que lanza en un bucle de parsing de COFF convierte cada
campo en un punto de salida.

**El buffer del llamador es `byte[]` y no `Span<byte>`**, siguiendo el precedente de
`Directory.GetFiles`. El `Span<byte>` vive una capa más arriba, en NativeSharp, envolviendo el
arreglo: la frontera de operaciones núcleo no tiene hoy ningún precedente de struct genérico
monomorfizado, y reconocerlo por clave de tipo sería un mecanismo nuevo. `BinaryReader` trabaja
sobre `Span<byte>` igual, construido sobre ese arreglo.

**Anotado, no hecho:** la comprobación mecánica de §4 para las operaciones núcleo futuras.
