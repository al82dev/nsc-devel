# 101 — Bloque 4, fases B y C: E/S binaria y creación de procesos

**Fecha:** 2026-09-02
**Cierra el Bloque 4.** La fase A está en el doc 100.
**Criterio del bloque (doc 69, escrito antes de empezar):** *un programa NativeSharp compila algo y
**llama a `lld-link.exe`***. Con `Process.Run` eso ya es posible.

---

## 1. Fase B — `ByteBuffer`, `BinaryWriter`, `BinaryReader`

**No tocan el compilador.** Ni una operación núcleo, ni una línea de C++. Son NativeSharp puro
construido sobre lo que las fases anteriores dejaron: arreglos, `Span<T>`, `Optional<T>` y el
chequeo de escape del Bloque 3.

Eso no es casualidad ni suerte: es la comprobación de que los bloques 1-3 hicieron su trabajo. Si
la fase B hubiera necesitado bajar al compilador, algo de lo anterior estaría incompleto.

### 1.1 `ByteBuffer` — el único que asigna

**Por qué no es `List<byte>`.** Fue la primera idea y se descartó por dos razones concretas:

* `List<T>` guarda `owned T[]` y hace `move(items[i])` **elemento a elemento** al crecer, porque un
  `T` cualquiera puede tener destructor. Un `byte` no tiene nada que mover, así que ese bucle es una
  copia byte a byte donde debería haber un bloque. Sobre un `.obj` de cien mil bytes se nota, y la
  E/S binaria de un compilador es exactamente donde se nota.
* Y la que de verdad decide: lo que `BinaryReader` necesita es un `Span<byte>` sobre los bytes
  **vivos**, y `List<T>` no lo expone — su `ToArray()` copia. Aquí `AsSpan()` no copia nada.

**Qué no hace:** no inserta en medio, no borra en medio, no busca. No es un contenedor de propósito
general: es un buffer de salida que crece por el final, que es la única forma en que un formato
binario se escribe.

**La capacidad inicial es 16 y no 0.** Un buffer que arranca vacío hace su primer crecimiento en el
primer byte, y el caso común —una cabecera de unas decenas de bytes— se lo comería entero en
reasignaciones. 16 cubre una cabecera COFF sin crecer ni una vez.

**`Clear()` no suelta la memoria.** Es lo que permite reutilizar el buffer entre archivos: escribir
un `.obj`, volcarlo, limpiar, escribir el siguiente — y ni una asignación después del primero. Mismo
motivo por el que existe `File.ReadBytes`.

**`AsSpan()` devuelve una VISTA, y eso lo sujeta el compilador.** Sobrevive sólo mientras el buffer
no crezca: un `Add` posterior puede reasignar y dejarla apuntando a memoria liberada. El chequeo de
escape del Bloque 3 impide guardarla en un campo o devolverla desde un método que la construya sobre
un local — las dos formas en que ese error se comete de verdad. Lo que **no** puede impedir es usarla
después de un `Add` en el mismo ámbito, y por eso queda dicho en el propio archivo.

### 1.2 `BinaryWriter` — sin errores, y no es un descuido

**Little-endian sin opción.** COFF y PE lo son, y este compilador emite COFF y PE. Un parámetro de
orden de bytes sería una opción que nadie usaría nunca con el otro valor, y cada campo escrito
pagaría una rama por ella. El día que haga falta big-endian se añade un tipo aparte con ese nombre,
no un booleano aquí.

**No posee el buffer.** Escribe en uno que le prestan. Un writer dueño del buffer obligaría a sacarlo
al final —devolverlo, o exponerlo— y eso es justo la clase de API en la que alguien acaba guardando
una vista de algo que el writer va a liberar.

**Ninguna operación puede fallar.** El destino crece, así que escribir no falla salvo por falta de
memoria, que en este compilador es una trampa. **La asimetría con `BinaryReader` es deliberada y es
la correcta:** leer consume datos que otro escribió y que pueden estar mal o acabarse; escribir, no.

**Se escribe con desplazamientos, no reinterpretando memoria.** Un `*(ushort*)p = v` daría el orden
del anfitrión, que hoy coincide y mañana en otra arquitectura no — y el fallo sería un `.obj`
corrupto que nadie relaciona con esa línea.

### 1.3 `BinaryReader` — `borrowed struct` y `Optional<T>`

**Es un `borrowed struct`, y esa es la decisión central.** No posee nada: lee de bytes que son de
otro. Declararlo así hace que el chequeo de escape impida guardarlo en un campo o devolverlo desde un
método que lo construya sobre un local — exactamente las dos formas en que un lector sobrevive al
buffer del que lee, que es un uso-después-de-liberar. La alternativa —una clase que copiara los
bytes— convertiría cada `.obj` en una copia entera, justo lo que el Bloque 3 y `File.ReadBytes`
existen para evitar.

**Errores por `Optional<T>`, nunca excepciones.** Decidido y escrito **antes de la primera firma**
(doc 98), porque cambiarlo después costaría reescribirlas todas.

El motivo: parsear COFF es un bucle de campos. Si cada `ReadUInt32` pudiera lanzar, cada campo sería
un punto de salida y el código de arriba se llenaría de `try`/`catch` — o peor, no los pondría, y un
fichero truncado se llevaría por delante la compilación entera con una excepción que no dice qué
campo falló. **Un fin de buffer no es excepcional: es el caso normal de un formato que se está
validando**, y el código que lo valida quiere preguntarlo, no capturarlo.

**Una lectura que no cabe devuelve `None` y NO avanza.** El lector queda donde estaba y el llamador
decide. Avanzar en una lectura fallida dejaría la posición en un sitio que no significa nada.

**Byte a byte también por alineación.** Los bytes de un fichero no tienen por qué estar alineados. Un
`*(uint*)p` sobre un offset impar es comportamiento indefinido en C, y según la arquitectura una
lectura lenta o un fallo. Byte a byte no tiene ese problema en ningún sitio.

---

## 2. Fase C — `Process.Run`, el bloqueo duro

```csharp
public static int Run(string executable, string commandLine, string outputPath)
```

`Process.ns` existía desde Stage 55.1, pero su cabecera decía que crear procesos hijos estaba
*«deliberadamente fuera de alcance»*: sus cinco operaciones hablan sólo del proceso **actual**. Esa
frase era el motivo por el que un `nsc` escrito en NativeSharp no podía lanzar el enlazador — y por
tanto no podía producir un ejecutable.

**Qué hace, en orden:** abre `outputPath` con el handle marcado como **heredable**; lanza el proceso
con stdout **y** stderr apuntando a ese handle; espera sin límite; lee el código de salida.

**Es lo que `LldLinker.cpp` ya hace en C++**, y esa es la prueba de que el conjunto es el correcto:
no se ha inventado una API, se ha portado la que el compilador usa.

**stdout y stderr al mismo fichero** porque un enlazador escribe diagnósticos por stderr y ruido por
stdout, y quien lee el resultado los quiere **intercalados en el orden en que ocurrieron**. Dos
ficheros obligarían a reconstruir ese orden, que no se puede.

**Sin tiempo límite, a propósito.** Un enlazador que no termina es un fallo del enlazador; matarlo a
los N segundos lo convertiría en «el código de salida fue raro», que se parece a un fallo del
programa que se compilaba. `WaitForSingleObject` con `INFINITE` deja el problema donde está.

**Devuelve −1 si no se pudo lanzar**, y esa es la única ambigüedad del diseño: los códigos de salida
de Windows son `DWORD` sin signo, así que un proceso que devolviera `0xFFFFFFFF` daría −1 también. Se
acepta a sabiendas — ningún compilador ni enlazador de esta familia lo hace, y la alternativa
partiría la firma más usada del driver en dos por un caso que no ocurre.

**Una ruta de salida vacía no es un caso especial:** `CreateFileA` falla sobre `""` y se cae por el
mismo camino de error. No hace falta comprobarlo antes.

**Los dos handles del hijo se cierran.** Olvidar el del hilo es la fuga clásica de `CreateProcess`:
el proceso termina, su handle se cierra, y el del hilo primario se queda vivo para siempre.

---

## 3. Dos defectos del andamiaje, encontrados al implementar esto

### 3.1 El décimo sitio: la lista de imports del enlazador

**`Environment.CurrentDirectory` rompió los 86 trabajos de una corrida completa.**

```
lld-link: error: undefined symbol: GetCurrentDirectoryA
>>> referenced by runtime.obj:(__nativesharp_environment_current_directory)
```

Declarar el import con `LLVMAddFunction` en `WindowsRuntimeBuilder` sólo dice «existe un símbolo con
este nombre». Quien le dice al enlazador **de qué DLL sacarlo** es otra lista,
`src/Linker/WindowsRuntimeImports.cpp`, que alimenta la biblioteca de importación. No la toqué.

**Por qué cayeron los 86 y no sólo lo que usa `CurrentDirectory`:** el objeto de runtime se enlaza
dentro de **cada** ejecutable. Un import que falta no rompe la función que lo usa — rompe el enlace
de todo.

La tabla del doc 100 §3 enumeraba nueve archivos. **Son diez**, y el décimo sólo aparece cuando la
operación importa algo de Win32 que no se usaba antes: las otras cuatro de la fase A reutilizan
`CreateFileA`, `GetFileSizeEx`, `ReadFile`, `WriteFile` y `CloseHandle`, así que ninguna lo habría
destapado.

### 3.2 `functionType()` tenía un tope de 8 y devolvía `nullptr` en silencio

`CreateProcessA` tiene **diez** parámetros. El helper del runtime construía el tipo así:

```cpp
std::array<LLVMTypeRef, 8> storage{};
if (parameters.size() > storage.size()) { return nullptr; }
```

Resultado: el tipo salía nulo, `LLVMAddFunction` lo recibía, y el compilador se caía con un SIGSEGV
**dentro de `llvm::PointerType::get`** — una pila que no menciona ni el helper, ni el tope, ni el
nombre del import:

```
#0  llvm::PointerType::get(llvm::Type*, unsigned int)
#1  llvm::Function::Function(...)
#2  LLVMAddFunction
#3  WindowsRuntimeBuilder::buildInternal(...)
```

**Arreglado subiendo el tope a 16 y dejando de devolver `nullptr`:** ahora un tope superado **aborta
con un mensaje que dice cuántos parámetros llegaron y cuál es el límite**. Devolver nulo ahí no era
un error que el llamador pudiera manejar — nadie comprueba el resultado, son cincuenta declaraciones
de imports seguidas, y el síntoma aparece a treinta marcos de distancia. **Entre un fallo ruidoso en
el sitio y un puntero nulo que viaja, el ruidoso.**

El límite estaba puesto porque hasta ahora ninguna firma de Win32 usada aquí pasaba de ocho. Es un
límite razonable que envejeció mal, y lo que estaba mal no era el número: era devolver nulo.

---

## 4. Las fixtures

### `tests/cc1/c25_binary_roundtrip` — salida 42

**Ida y vuelta, no comparación contra literales.** Se construye una cabecera con la forma de la de
COFF —máquina, secciones, sello de tiempo, punteros, un nombre de sección de ancho fijo—, se vuelca
a disco con las operaciones de la **fase A**, se relee y se exige que **todos** los campos coincidan.

**La propiedad central es que escribir y leer son inversas.** Una prueba que comprobara el lector
contra bytes literales pasaría igual si los **dos** estuvieran mal del mismo modo.

Cubre además, con un código de salida por caso:

* el **máximo de 64 bits** (`18446744073709551615`) y un **−1** — donde el signo se rompe si algún
  paso trata el valor como con signo;
* que leer de más devuelve `None` **y la posición no se mueve**;
* que `Seek` fuera de rango devuelve `false` **sin mover nada**;
* que `ByteBuffer` no pierde un byte al crecer de 2 a 300 de capacidad;
* que `Clear()` conserva la capacidad;
* que `AddRange` recorta cuando se le pide más de lo que hay.

### El nombre del fichero temporal sale del propio ejecutable

`run_cases.py` lanza los 88 trabajos con `cwd` en la **raíz del repo** y 16 en paralelo, así que
`-O0` y `-O2` de la misma fixture corren **a la vez en el mismo directorio**. Con un nombre fijo los
dos escribirían, leerían y borrarían el **mismo** fichero.

El fallo sería **intermitente**: uno borra mientras el otro lee, y el caso rojo cambia de corrida en
corrida. Es la peor clase de prueba que hay — una que falla a veces y por un motivo que no está en el
código que se estaba tocando. Exactamente lo mismo que le pasó a `stage48_5` con `OutDir` (doc 97
§2.3), y por eso se buscó antes de correr nada.

`args[0]` es la ruta del ejecutable, que `run_cases.py` nombra `<etiqueta>_<opt>.exe`. Derivar de ahí
da un nombre distinto por trabajo sin que el programa tenga que saber con qué optimización se
compiló.

### Por qué `Process.Run` no tiene fixture ejecutable

Porque una fixture que lance un proceso tiene que lanzar **algo**, y todo lo que hay a mano —`cmd.exe`
y su ruta, un `.exe` del propio árbol— ata la prueba a una máquina concreta o al orden en que se
construyen las cosas. Una prueba que falla porque `cmd.exe` no está donde alguien supuso no mide el
compilador.

**Lo que sí está medido** es que la operación núcleo llega entera hasta el IR: la llamada al hook en
el módulo del programa, y la definición del hook en el runtime con `CreateProcessA`,
`WaitForSingleObject`, `GetExitCodeProcess` y los tres `CloseHandle`. Es lo mismo que se puede medir
de las otras cinco sin ejecutar en Windows.

**Su prueba de verdad es la fase C del criterio del bloque** — un programa NativeSharp que invoque al
enlazador de verdad — y ésa es una fixture del driver, no de `Process.Run` suelto. Queda anotada, no
hecha, y dicho aquí para que nadie la dé por cubierta.

---

## 5. Qué se tocó

| Archivo | Qué |
|---|---|
| `lib/NativeSharp.Core/ByteBuffer.ns` | **nuevo** |
| `lib/NativeSharp.Core/BinaryWriter.ns` | **nuevo** |
| `lib/NativeSharp.Core/BinaryReader.ns` | **nuevo** — `borrowed struct` |
| `lib/NativeSharp.Core.Diagnostics/Process.ns` | `Run` |
| `include/…/ProjectMethodBodyModel.h` · `…HirModel.h` · `…MirModel.h` | `ProcessRun` / `CoreProcessRun` |
| `src/Frontend/ProjectMethodBodyModel.cpp` | reconocimiento de `Run` |
| `src/Frontend/ProjectOopHirModel.cpp` · `…MirModel.cpp` | nombres, las dos listas, mapeos |
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | el hook `__nativesharp_process_run` |
| `src/Runtime/WindowsRuntimeBuilder.cpp` | `buildProcessRun`, `buildZeroFillStack`, los imports `CreateProcessA`/`GetExitCodeProcess`, y **el tope de `functionType`** |
| `src/Linker/WindowsRuntimeImports.cpp` | `GetCurrentDirectoryA`, `CreateProcessA`, `GetExitCodeProcess` |
| `tests/cc1/c25_binary_roundtrip/` | **nueva** fixture |
| `tests/unit/Cc1CorrectnessTests.cpp` · `run_cases.py` · `nslink_pipeline.py` | registrada en los tres sitios |

---

## 6. Cómo se comprobó

* `c25` por el frontend: `--stage=llvm` **OK**.
* `Process.Run`: la llamada al hook en el IR del programa, y la definición en el runtime con
  `CreateProcessA(…10 argumentos…)`, `WaitForSingleObject(…, i32 -1)`, `GetExitCodeProcess` y los
  tres `CloseHandle`.
* Barrido del árbol en `bodies` y en `llvm`: **169 OK / 5 FAIL** — dos más que la línea base de la
  mañana (c24 y c25), y los mismos cinco negativos intencionados.

**Lo que esto NO demuestra:** que funcione. El contenedor no enlaza ni ejecuta en Windows. Ninguna
de estas seis operaciones ha llamado nunca a su función de Win32 de verdad. Eso lo mide `all.bat`, y
hasta entonces el Bloque 4 está **escrito**, no verificado.

---

## 7. Estado del Bloque 4

| Fase | Qué | Estado |
|---|---|---|
| A | `File.GetSize` · `ReadAllBytes` · `ReadBytes` · `WriteAllBytes` · `CurrentDirectory` | escrita |
| B | `ByteBuffer` · `BinaryWriter` · `BinaryReader` | escrita |
| C | `Process.Run` | escrita |
| D | `NativeLibrary` | **fuera** — doc 99 §1 |

**Lo que falta para cerrarlo de verdad:** una corrida de `all.bat` en verde, y la fixture del criterio
—un programa NativeSharp que encuentre fuentes, las lea, escriba un binario, lo relea y **llame al
enlazador**—. Las piezas están; falta el programa que las use todas a la vez.
