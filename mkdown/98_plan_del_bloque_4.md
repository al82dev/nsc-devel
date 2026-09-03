# 98 — Plan del Bloque 4: sistema operativo y binarios

**Fecha:** 2026-09-02
**Estado de partida:** Bloques 1, 2 y 3 cerrados. `lld-link` 244/244, NSLink 244/244, 84 trabajos OK
con los dos. ID 131 cerrado, que era el tapón.
**Criterio de cierre del bloque (doc 69, escrito antes de empezar):** *un programa NativeSharp
compila algo y **llama a `lld-link.exe`***.

---

## 1. Lo primero: SHR-031 está casi hecho, y eso cambia el tamaño del bloque

El plan original listaba «filesystem compiler-grade» como una de las cinco piezas. Contando lo que
hay hoy en `lib\` contra las **siete operaciones medidas** en el doc 81 —las que el árbol de `nsc`
usa de verdad de `std::filesystem`— queda esto:

| Operación de `std::filesystem` | Usos | Qué hay hoy | Falta |
|---|---:|---|---|
| `path` (manipulación de rutas) | 108 | `Path.Combine`, `GetFileName`, `GetDirectoryName`, `GetExtension`, `GetFileNameWithoutExtension` | — |
| `remove` | 31 | `File.Delete`, `Directory.Delete` | — |
| `is_regular_file` | 9 | `File.Exists` | distinguir archivo de directorio |
| `weakly_canonical` | 2 | `Path.GetFullPath` | — |
| `absolute` | 2 | `Path.GetFullPath` | — |
| `file_size` | 1 | — | **SHR-030** |
| `current_path` | 1 | — | **falta**: `Environment` no lo tiene |

**El 80 % era manipulación de texto y ya estaba.** De SHR-031 quedan **dos funciones y media**:
`File.GetSize`, el directorio de trabajo actual, y afinar `Exists` para no confundir un directorio
con un archivo. Eso no es una pieza del bloque: es un remate.

`Directory` ya tiene `Exists`, `CreateDirectory`, `Delete`, `GetFiles`, `GetDirectories` y sus
conteos. La enumeración de fuentes que el driver necesita está entera.

---

## 2. Qué queda de verdad, en el orden en que hay que hacerlo

### Fase A — La mitad binaria de `File` (SHR-028/029/030)

`lib\NativeSharp.Core.IO\File.ns` tiene hoy `Exists`, `Delete`, `ReadAllText`, `WriteAllText`,
`AppendAllText`, `Copy`, `Move`. **Falta la mitad binaria, no el archivo entero.**

```
File.ReadAllBytes(string path)                 -> owned byte[]
File.ReadBytes(string path, Span<byte> dest)   -> int      (sin asignar)
File.WriteAllBytes(string path, ReadOnlySpan<byte> data)
File.GetSize(string path)                      -> long
Environment.CurrentDirectory()                 -> string
```

**Por qué primero:** es lo más pequeño, lo que menos incógnitas tiene, y todo lo demás lo usa. Y es
la primera vez que una operación núcleo del compilador toma un `Span<T>` en la frontera con Win32 —
si esa forma tiene un problema, se descubre aquí y no dentro de `BinaryReader`.

**Por qué `ReadBytes` sobre `Span` además de `ReadAllBytes`:** leer un `.obj` de 100 KB para
después copiarlo a un buffer es exactamente lo que el Bloque 3 existe para evitar. La variante que
asigna se queda porque es la cómoda; la que no asigna es la que usará el driver.

**Terminado cuando:** una fixture escribe un archivo binario, lo lee de las dos formas, y los bytes
coinciden byte a byte con lo escrito. Y `GetSize` coincide con la longitud leída.

---

### Fase B — `ByteBuffer`, `BinaryReader`, `BinaryWriter` (SHR-025/026/027)

Es el corazón del bloque, y donde está el trabajo real: **~15 funciones** sobre `Span<byte>`.

```
ByteBuffer      buffer que crece, dueño de su memoria    -> Span<byte>
BinaryReader    lee de un ReadOnlySpan<byte>, con posicion
BinaryWriter    escribe en un ByteBuffer
```

Lo que tienen que saber leer y escribir, y no es una lista abierta: **es exactamente lo que COFF y
PE necesitan** — `byte`, `ushort`, `uint`, `ulong` en little-endian, un bloque de bytes, y una
cadena de longitud fija. Nada más, hasta que algo lo pida.

**Tres decisiones que hay que tomar aquí, y ninguna es de estilo:**

1. **`BinaryReader` no posee nada.** Toma un `ReadOnlySpan<byte>` y lleva una posición. Es un
   `borrowed struct`, así que el escape checking del Bloque 3 impide que sobreviva al buffer del que
   lee — que es precisamente el uso-después-de-liberar que se quiere imposible.
2. **`ByteBuffer` sí posee**, y por tanto es `owned` con su destructor. Es la única de las tres que
   asigna.
3. **Los errores no son excepciones.** Leer más allá del final devuelve `Optional<T>` o falla con un
   código; un `BinaryReader` que lanza en un bucle de parsing de COFF convierte cada campo en un
   punto de salida. Esto hay que decidirlo **antes** de escribir la primera función, porque cambia
   la firma de las quince.

**Terminado cuando:** una fixture construye un objeto COFF mínimo con `BinaryWriter`, lo vuelve a
leer con `BinaryReader`, y todos los campos coinciden. Ida y vuelta, no una comprobación contra
números escritos a mano.

---

### Fase C — `Process.Run` (SHR-032). **El bloqueo duro.**

`lib\NativeSharp.Core.Diagnostics\Process.ns` existe pero su cabecera dice, literalmente, que
*«crear procesos hijos (CreateProcess) está deliberadamente fuera de alcance»*. Tiene
`CurrentProcessId`, `WorkingSetSize`, `KernelTime`, `UserTime`, `PriorityClass`. **Ninguna crea un
proceso.**

Lo que hace falta:

```
Process.Run(string executable, string commandLine, string stdoutPath) -> int
```

Y no es «una función más». Es **seis operaciones núcleo nuevas** en la cadena de seis capas del
compilador —`MethodCoreOperationKind` → HIR → MIR → bajada a LLVM → el `case` obligatorio en
`methodCoreOperationKindName`— más el `CreateProcessW` real en `WindowsRuntimeBuilder`, con
redirección de stdout/stderr a un fichero y espera del código de salida. Es exactamente lo que
`LldLinker.cpp` hace hoy en C++, y hay que reproducirlo.

**Aquí es donde entra la pregunta que dejaste abierta**, y por eso esta fase va la última:

> Si `nsc` deja de *lanzar* un enlazador y enlaza **en proceso** contra `NativeLinkCore`,
> `Process.Run` se cae del camino crítico del auto-hospedado.

Medido: `CompatibleDriver::parse` y `::run` ya existen en `NativeLinkCore.lib`, y `nsc` es C++, así
que la mitad de hoy no depende de nadie. Para el `nsc` escrito en NativeSharp haría falta una
entrada con ABI de C — una petición pequeña por `nsc_actions\`.

**Las dos ramas no se excluyen, y por eso el plan no las enfrenta:** `Process.Run` hace falta de
todos modos para cualquier otra herramienta que el driver quiera invocar. Lo que la rama en-proceso
cambia es si es **bloqueante** para el auto-hospedado o simplemente **útil**. Esa decisión es tuya y
sólo afecta a la prioridad de esta fase, no a su contenido.

**Terminado cuando:** un programa NativeSharp lanza `lld-link.exe` con un fichero de respuesta,
espera, y lee su código de salida y su salida de texto.

---

### Fase D — `NativeLibrary` (SHR-035) — **NO SE HACE**

**Se cayó del bloque el 2026-09-02, el mismo día que se escribió este plan.** Aquí decía que iba la
última y que podía caerse «si se decide enlazar contra `NativeLinkCore` directamente». Eso se
decidió: el flag de enlazador tiene tres valores —`lld`, `nslink-exe`, `nslink-inproc`— y el tercero
enlaza contra `NativeLinkCore.lib` **dentro** de `nsc`, así que no hay ninguna DLL que cargar.

**El motivo completo, los cuatro casos que sí justificarían la carga dinámica, y las condiciones
concretas que la reabren —recarga en caliente de lógica de juego entre ellas— están en el doc 99 §1.**

No se vuelve a listar como pendiente sin haber mirado esa sección. La forma de reabrirla no es que
alguien la encuentre en una tabla vieja: es que se cumpla una de sus cuatro condiciones.

**El Bloque 4 se cierra sin ella.** Lo único que le falta es el programa del criterio de cierre.

---

## 3. Cómo se prueba el bloque entero

El criterio del doc 69 es un **programa**, no una lista de comprobaciones:

```
tests/cc1/c24_driver_minimo/
    encuentra las fuentes de un directorio        Directory.GetFiles
    lee cada una                                  File.ReadAllText
    escribe un objeto binario                     ByteBuffer + BinaryWriter + File.WriteAllBytes
    lo vuelve a leer y comprueba los campos       File.ReadBytes + BinaryReader
    invoca al enlazador                           Process.Run
    lee su codigo de salida                       -> exit 42
```

Un solo `.nsproj` que recorre las cuatro fases. Si pasa a `-O0` y a `-O2`, el bloque está cerrado.

**Y las fases A y B se prueban además por separado**, porque un fallo en el programa completo no
dice cuál de las seis cosas falló.

---

## 4. Riesgos, dichos antes de empezar

**`Span<byte>` cruzando a Win32 no se ha hecho nunca.** Todas las operaciones núcleo actuales pasan
`string` o enteros. La Fase A es la primera que pasa un puntero y una longitud a una API del
sistema. Si la forma de la frontera tiene un problema, aparece ahí — y por eso la Fase A va primera
aunque sea la más pequeña.

**ID 132 sigue abierto y puede molestar.** Los métodos genéricos no se sobrecargan por lista de
parámetros. Si `BinaryReader` quiere `Read<T>()` y `Read<T>(int count)`, chocan. Se rodea
renombrando —como se hizo en el Bloque 3— y se anota; no se arregla dentro de este bloque.

**SHR-063 (condicionales de compilación) sigue sin decidirse.** Bloquea a este bloque **sólo si** el
driver necesita distinguir configuraciones. Mientras el programa de prueba sea uno solo, no bloquea.

**El manejo de errores de la Fase B es una decisión, no un detalle.** Tomarla tarde significa
reescribir quince firmas.

---

## 5. Orden, y por qué

```
A  File binario + CurrentDirectory     pequeño, sin incognitas, lo usa todo lo demas
B  ByteBuffer / BinaryReader / Writer  el grueso; decidir el manejo de errores ANTES
C  Process.Run                         seis operaciones nucleo + CreateProcessW
D  NativeLibrary                       CAYO -- se decidio enlazar en proceso, ver doc 99 §1
```

A y B son secuenciales de verdad: B se construye sobre lo que A demuestre de la frontera. C es
independiente de las dos y **podría ir en paralelo** si hubiera otro frente. D depende de tu
decisión sobre el enlazado en proceso.

---

## 6. Lo que este plan NO promete

No hay estimaciones en días. El doc 67 §2 explica por qué: la primera versión de estos planes decía
«1-2 días» para cosas cuyo tamaño nadie había mirado.

Lo que sí está medido es **cuánto queda**, y el hallazgo de la sección 1 lo reduce: SHR-031 estaba
casi hecho, así que el bloque son tres piezas y un remate, no cinco piezas.
