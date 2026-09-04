# 118 -- El tipo subyacente de un enum, y el contrato de layout de una struct

Fecha: 2026-09-03
Alcance: SOLO el compilador. Nada del enlazador, nada de los bindings.

Esta entrega cierra las dos primeras peticiones de la lista de cuatro que salio de la revision
de lo que el compilador le falta a los bindings: **aserciones de layout** (`size(N)`) y **enums
con tipo subyacente** (`enum E : byte`). Las dos son de la misma clase, y por eso van juntas.

---

## 1. Por que estas dos y no otras

La lista que el trabajador de bindings mando tenia cuatro peticiones. Las ordene por una sola
pregunta: **¿esto es una carencia de expresividad, o es una respuesta falsa?**

Una carencia de expresividad se nota: no compila, alguien lo escribe de otra forma y sigue. Una
respuesta falsa compila, no dice nada, y el programa lee el campo equivocado desde la primera
llamada. Por el criterio de este proyecto, la segunda es P1 y la primera no.

| Peticion | Que es | Veredicto |
|---|---|---|
| Aserciones de layout | Sin ellas, un binding con un tamano mal calculado compila y lee basura | **Respuesta falsa. P1.** |
| Enum con tipo subyacente | `enum : uint8_t` en un header de C producia un campo de 4 bytes donde C pone 1 | **Respuesta falsa. P1.** |
| Tipos opacos nominales | Hoy se escribe `void*` y funciona; se pierde la comprobacion de tipos | Carencia. Siguiente. |
| Importar variables C | No hay consumidor medido, y necesita que NSLink marque el import como dato | Otra via. Aparcado. |
| Variadicas `extern C` | El envoltorio tipado es mejor respuesta | **No.** Ver §6. |

---

## 2. `size(N)` -- el contrato de layout

### El defecto que cierra

Un binding se escribe a partir de un header de C. El header dice:

```c
typedef struct { uint8_t flag; int32_t value; } R;   // sizeof == 8
```

y quien traduce escribe cinco, porque conto los campos y no el relleno. Eso **compila**. El
programa llama a la API, la API lee ocho bytes, y los tres ultimos son lo que hubiera en la pila.
No hay error, no hay aviso, y el sintoma aparece a tres capas de distancia.

### Que se anadio

Un modificador de declaracion, `size(N)`, en la misma familia que `pack(N)` y `align(N)`:

```csharp
public size(8) struct R
{
    public byte Bandera;
    public int Valor;
}
```

**`size(N)` NO RELLENA. VERIFICA.** Es la mitad que importa y esta escrita en el propio
diagnostico:

```
struct 'R' declares size(5) but its fields lay out to 8 bytes with alignment 4;
size() verifies the layout, it does not pad it
```

Un modificador que rellenara seria una forma de que el binding y el header discrepen **en
silencio**: el compilador taparia la diferencia en vez de senalarla. Lo que hace falta aqui es
justo lo contrario -- que el numero que el generador leyo del header y el numero que el modelo de
layout calcula tengan que coincidir, y que no coincidir sea un error de compilacion con los dos
numeros escritos al lado.

### Los tres rechazos

* `size(5)` sobre una struct que mide 8 -- el caso que justifica la funcion entera.
* `size(N)` sobre una `class` -- una clase no tiene layout de valor, es un objeto de monticulo con
  su cabecera. Aceptarlo lo convertiria en una decoracion que nadie honra, que es lo mismo que
  `pack`/`align` ya rechazan por el mismo motivo.
* `size(0)` -- casi siempre es un generador que no supo leer el tamano y escribio su valor por
  omision. Una struct de cero bytes no existe en el modelo de objetos de Windows x64.

### La fixture: `tests/cc1/c37_contrato_de_layout`

Once structs. **Los tamanos no los escribi yo: salieron de compilar las equivalentes en C con
clang y preguntarle `sizeof` y `_Alignof`.** Ninguna verificacion cuenta si el modelo contra el
que verifico lo construi yo, y aqui el modelo es un compilador de C -- que es exactamente contra
quien estas structs tienen que coincidir cuando el binding llame a la API de verdad.

La fixture mide las **dos** mitades:

* que el modelo de layout coincida con C -- eso lo comprueba el propio `size(N)` al compilar;
* que lo que el backend materializa sea ese mismo modelo -- eso lo comprueban `sizeof` y la
  lectura por punteros en ejecucion.

Si solo se comprobara la primera, un backend que emitiera otra cosa pasaria.

---

## 3. `enum E : byte` -- el tipo subyacente

### El defecto que cierra

Hasta hoy **todo enum de NativeSharp media cuatro bytes y se trataba como con signo**. Eso no
estaba escrito en un sitio: estaba escrito en **cuatro**, por separado, cada uno con su comentario
diciendo "los enums son int con signo":

1. `TargetDataLayout` devolvia `{4,4}` para `BuiltinLayoutKind::Enum`;
2. `ProjectOopLlvmLowerer::llvmType` devolvia `i32`;
3. `convert()` daba ancho 32 y signo a toda clave de enum;
4. la bajada de `switch` y la de campos `const` emitian las constantes **con signo**.

Un header de C con `enum Estado : uint8_t` dentro de una struct producia entonces un layout
silenciosamente equivocado: el campo ocupaba cuatro bytes donde C pone uno, **y todo lo que iba
detras se desplazaba**. Otra vez la misma forma: compila, no dice nada, y el error aparece en la
llamada.

### Que se anadio

```csharp
public enum SinSigno : byte  { Bajo = 0, Medio = 128, Alto = 200 }
public enum ConSigno : sbyte { MenosTres = -3, Cero = 0, Tope = 127 }
public enum Ocho     : long  { Cero = 0, Enorme = 5000000000 }
```

Sin `: tipo` es `int`, que es lo que ya hacia y lo que hace C por omision. **Nada de lo escrito
hasta hoy cambia de tamano** -- ningun `.ns` del arbol declaraba un tipo subyacente, asi que el IR
de todo el corpus es identico por construccion, no por parecido.

Solo enteros incorporados: ni `bool`, ni coma flotante, ni otro enum, ni un tipo de usuario. El
ancho y el signo de un enum son parte del ABI, y para esos no hay respuesta.

### Una sola regla -- y eran SEIS copias, no cuatro

**Correccion, escrita despues de la revision del delta (ver §7).** Este documento decia cuatro
sitios porque arregle cuatro. Contar los sitios de una regla duplicada requiere BUSCARLOS, no
recordarlos: habia dos mas, en `ProjectIntegerEvaluator` y en `ProjectOopHirModel`, y las dos
truncaban a 32 bits despues de que el modelo de miembros ya hubiera aceptado el valor. La lista
completa esta en §7.

### Una sola regla, seis consumidores

La regla vive en `include/nativesharp/Frontend/EnumUnderlyingType.h` y **es la unica**. En el
backend hay una sola funcion --`enumUnderlyingInfo(typeKey)`-- que la consulta, y de ella salen el
tipo LLVM, el ancho de las conversiones, el signo de las extensiones y el signo de las constantes
de `case`. Fuera del backend la consultan el modelo de layout, el modelo de miembros, el evaluador
de constantes, el modelo HIR y el generador de cabeceras C. Antes cada uno tenia su copia, y que
fueran varias es la razon por la que un `enum : byte` las contradecia a todas a la vez.

Es la misma forma de defecto que los docs 102, 103, 106, 112 y 115 documentan una y otra vez en
este arbol: **una regla escrita varias veces con una nota pidiendo que alguien se acuerde.**

### La validacion mira la FORMA, no el nombre

`enum E : int*` tiene `"int"` de nombre base. Una comprobacion escrita sobre el nombre lo
aceptaria, y el enum acabaria midiendo cuatro bytes cuando el autor escribio un puntero. Se
rechazan los modificadores (puntero, arreglo), los argumentos genericos y el cualificador de
propiedad, mirando la estructura del `TypeSyntax`.

### El auto-incremento tambien se comprueba

```csharp
public enum E : byte { A = 255, B }   // B seria 256
```

`B` no lleva **ningun numero escrito que mirar**. Comprobar solo los valores explicitos dejaria
pasar justo el caso que nadie ve. El rango se comprueba despues de calcular el valor, no dentro de
la rama del inicializador.

### El fallo que este trabajo produjo, y por que fue el afortunado

La primera version cambio el tipo LLVM del enum pero no el de las conversiones. El resultado:

```
LLVM verifier rejected OOP module: Both operands to ICmp instruction are not of the same type!
  %icmp = icmp ne i8 %local.load, i32 200
```

Es un fallo **ruidoso**: el verificador de LLVM lo paro. Con `enum : sbyte` habria sido peor --
extender sin signo un `-3` da `253`, y eso compila, verifica y responde mal. Por eso la fixture
mide el signo en las dos direcciones y con valores elegidos para que discrepar se note.

### La fixture: `tests/cc1/c38_enum_ancho`

Los valores estan escogidos para delatar cada error posible:

| Valor | Si el signo estuviera al reves |
|---|---|
| `SinSigno.Alto = 200` (`byte`) | extendido con signo daria **-56** |
| `SinSigno.Medio = 128` (`byte`) | extendido con signo daria **-128** |
| `ConSigno.MenosTres = -3` (`sbyte`) | extendido sin signo daria **253** |

Y se mide en los cuatro sitios donde la respuesta se da:

* **el tamano** -- `sizeof` de cada enum y de tres structs que los contienen;
* **la conversion** -- `(int)`, `(long)` y la direccion contraria `(SinSigno)entero`;
* **el `switch`** -- si la constante de `case` 200 se emitiera con signo (`-56`) y el valor sin
  signo (`200`), ningun `case` acertaria;
* **el layout** -- campos de struct, verificados por `size(N)` contra clang, y un arreglo, donde un
  elemento de cuatro bytes en vez de uno mandaria los indices a otro sitio.

Mas `ToString()`, que compara el valor contra cada constante -- comparaciones que el signo tambien
puede romper.

Los cuatro rechazos correspondientes estan en `Cc1CorrectnessTests.cpp`: valor que no cabe,
auto-incremento que no cabe, tipo subyacente que no es entero, y tipo subyacente con puntero.

---

## 4. Lo medido

* Barrido del corpus completo en el arnes de contenedor: **OK=183 FAIL=5**, y los cinco FAIL son
  los negativos intencionados de siempre (`ForeachNegativeTest`, `BorrowEscape`,
  `BorrowedToOwned`, `ImplicitOwnedCopy`, `UseAfterMove`).
* **El IR del resto del corpus no cambia, y no por parecido: por construccion.** Ningun `.ns` del
  arbol declara un tipo subyacente de enum, asi que `enumUnderlyingTypeKey` devuelve `builtin:int`
  para todos, los cuatro consumidores dan exactamente la respuesta que daban antes (32 bits, con
  signo), y `size(N)` no aparece en ninguna struct ya escrita.
* Las extensiones emitidas, leidas del IR: `zext i8 -> i32` y `zext i8 -> i64` para `byte`,
  `sext i8 -> i32/i64` para `sbyte`, `zext i16 -> i32` para `ushort`, `trunc i32 -> i8` en la
  direccion contraria.
* Las constantes de `switch`: `i8 0`, `i8 -128`, `i8 -56` para `SinSigno` -- los patrones de bits
  de 0, 128 y 200 sin signo, que es lo correcto.
* El arreglo de cuatro enums de un byte reserva `__nativesharp_alloc(i64 12)`: ocho de cabecera y
  cuatro de elementos.

Lo que **no** se ha medido todavia: la ejecucion en Windows. Las dos fixtures compilan y su IR es
el correcto, pero `c37` y `c38` no han corrido nunca en una pasada de `all.bat` -- ni siquiera
`c37`, que se escribio antes y cuya inscripcion en `cases_table.py` y en `Cc1CorrectnessTests.cpp`
se quedo sin entregar. Esta entrega las inscribe las dos.

---

## 5. Lo que queda de la lista de cuatro

### Tipos opacos nominales -- pendiente, y es el siguiente

Un header de C declara `typedef struct HWND__* HWND;` sin decir nunca que hay dentro. Hoy el
binding escribe `void*` y funciona, pero entonces `HWND`, `HDC` y `HBRUSH` son **el mismo tipo**:
pasarlos cambiados compila.

La forma que encaja con el diseno de NativeSharp es una declaracion que diga "esto tiene nombre y
no tiene contenido": usable solo detras de un puntero, sin `sizeof`, sin locales, sin `new`. No es
una respuesta falsa --lo que hay hoy funciona-- pero es la unica de las cuatro que anade
comprobacion en vez de quitarla.

### Importar variables C -- aparcado, y por que

`extern C` importa funciones. Un simbolo de **datos** (`extern int errno;`) necesita que el
enlazador marque el import como dato y no como funcion, y **el enlazador lo hace otro**. Sin un
consumidor medido que lo pida, esto es coordinacion entre carriles por adelantado.

### Variadicas `extern C` -- no

`printf(const char*, ...)` es la peticion. La respuesta es **no**, y no por dificultad:

* Una variadica de C **no comprueba nada**. Es exactamente el agujero que el resto del lenguaje
  existe para no tener.
* La ABI de Windows x64 tiene reglas propias para el argumento variadico (los `float` viajan
  duplicados en el registro entero y en el XMM) que habria que implementar y mantener.
* Un envoltorio tipado --`printf_s(const char*, int)`, `printf_d(const char*, double)`-- es una
  linea por forma usada, se comprueba, y es lo que un binding serio escribe de todas formas.

Aceptarla seria "poner cosas de C# --o de C-- a lo idiota": copiar una forma porque existe alla, no
porque resuelva algo aqui.

---

## 6. Ficheros

Ver `mkdown/STAGE_ENUM_CHANGED_FILES.txt`.

---

## 7. Revision del delta: seis defectos, y lo que enseñan

Una revision estatica sobre el delta encontro seis problemas. Los seis eran reales. Van aqui con
lo que cada uno midio de mas que su propio arreglo.

### P0 -- El reparto de pruebas estaba anulado

`CMakeLists.txt` registra una prueba de ctest por fixture y le pasa la etiqueta como argumento,
pero el ejecutable habia vuelto a `int main()` sin argumentos. Tres consecuencias, y las tres
peores que la lentitud que el reparto venia a arreglar:

1. cada una de las ~40 entradas corria la suite ENTERA -- el reparto multiplicaba el trabajo por
   cuarenta en vez de repartirlo;
2. las ~40 compartian `%TEMP%/nsc-cc1` y cada una lo borraba al arrancar: una instancia borraba
   los `.exe` que otra estaba enlazando. **Rojos que no corresponden a ningun defecto del
   compilador y que no se reproducen** -- lo peor que le puede pasar a una suite;
3. un rojo seguia sin decir cual, que era el segundo motivo del reparto.

Restaurado: `main(int argc, char** argv)`, `fixtureSelected()`/`restSelected()`, y **el directorio
temporal lleva el selector en el nombre**, para que dos entradas concurrentes no puedan pisarse
aunque el argumento se pierda por otro camino.

Como llego a pasar: el fichero se entrego desde una copia del contenedor que era anterior al
trabajo del selector. **La leccion no es "tener cuidado": es que el tamano en bytes no es un
diff.** La entrega anterior tambien perdio la inscripcion de `c37` por lo mismo.

### P1 -- Los headers C conservaban enums de 32 bits

`CHeaderGenerator` emitia `typedef int32_t NS_Foo;` para todo enum. Es el mismo defecto que el
tipo subyacente vino a cerrar, **en la direccion que el propio compilador genera**: un
`enum : byte` exportado le decia a C que leyera cuatro bytes de un campo de uno.

El typedef sale ahora de `hostType()`, que ya sabe traducir cada clave de entero a `<stdint.h>`;
no se escribio una segunda tabla. Medido en `Stage48_1CHeaderGeneratorTests.cpp` con un
`enum : byte` (`uint8_t`) y un `enum : long` (`int64_t`), y el enum sin tipo declarado sigue dando
`int32_t`.

### P1 -- Los alias de enum elegian el ULTIMO nombre

`lowerEnumName` construia la cadena de `select` en orden de declaracion. Como cada vuelta envuelve
al anterior, **el ultimo par procesado queda mas afuera y gana** -- lo contrario de lo que el
frontend documenta y de lo que la fixture c34 fija (`Solo = 1` declarado tras `Lectura = 1` tiene
que dar `"Lectura"`).

Confirmado en el IR antes de tocar nada: el `select` de mas afuera era el de `Solo`. La cadena se
construye ahora del ultimo par al primero.

**Esta es la que mas enseña.** El defecto lo introdujo la reescritura del ID 129 --pasar de una
cadena de condicionales al nodo `EnumName`-- y c34 lo habria cazado en la primera pasada de
Windows. No lo cazo porque **c34 no ha vuelto a correr desde esa reescritura**: el ultimo
`all.bat` verde es anterior. Un corpus que no corre no mide nada.

### P1 -- El modelo no representaba todo el rango de `ulong`

El valor de un enumerador se llevaba en un `int64_t`. Los dos extremos que un enum puede declarar
--el minimo de `long`, -2^63, y el maximo de `ulong`, 2^64-1-- **no caben en el mismo tipo de
C++**: `ulong.MaxValue` se veia como -1 y se rechazaba una constante legal.

Se lleva ahora como par `(magnitud sin signo, signo)` y el `int64_t` de `ClassMemberSymbol` guarda
el **patron de bits**, que es lo que todos sus consumidores ya hacian con el.

Y tirando de ese hilo aparecieron **dos copias mas** de "un enum es un `int` con signo", ninguna de
las dos en la lista de cuatro que el doc decia haber cerrado:

* `ProjectIntegerEvaluator` fijaba `value.typeKey = "builtin:int"` para todo miembro de enum;
* `ProjectOopHirModel` sintetizaba el literal del campo estatico con `"builtin:int"`.

Las dos truncaban a 32 bits **despues** de que el modelo de miembros hubiera aceptado el valor:
`4294967295` en un `enum : ulong` salia valiendo -1 en el IR, y nada en el camino lo decia. Eran
la quinta y la sexta. Medido ahora en los diez tipos, leyendo el IR: `i64 -1` para
`ulong.MaxValue`, `i64 -9223372036854775808` para el minimo de `long`, `i32 -1` para
`uint.MaxValue`, `i16 -5536` para 60000, `i8 -56` para 200.

**Lo que esto enseña sobre el doc que esta encima:** decir "cuatro sitios" y arreglar cuatro no
demuestra que fueran cuatro. Contar los sitios de una regla duplicada requiere buscarlos, no
recordarlos.

### P1 -- La ruta de fichero unico seguia forzando i32

`LlvmLowerer` y `SemanticAnalyzer` tienen su propio modelo de tipos, su propio layout y su propio
bajador, y en los tres un enum media 32 bits con signo. Aceptar `enum E : byte` y seguir dando 32
bits es la unica salida que este proyecto no admite: el autor escribio el ancho que queria y el
compilador dijo que si.

**Decision: rechazar, no implementar.** Implementarlo alli seria una SEGUNDA COPIA de la regla
--el defecto que este trabajo vino a cerrar-- en un camino que ninguna fixture del corpus
ejercita, o sea codigo sin medir. El rechazo es corto, cierto, y dice adonde ir:

```
a declared enum underlying type is only supported by the project pipeline;
in the single-file pipeline an enum is always a signed 32-bit integer
```

### P2 -- Un fallo de `size` dejaba el layout en `Visiting`

Los otros tres puntos de fallo de `buildStructLayout` marcan `Failed` antes de salir; el de
`size(N)` no. La siguiente consulta del mismo tipo --cualquier struct que lo tenga como campo-- lo
tomaria por un ciclo, y el segundo diagnostico diria "referencia circular" sobre una struct que no
tiene ninguna. **Un error que inventa un segundo error distinto es peor que el primero.**

---

## 8. Los dos bordes, definidos

### `enum E : char` -- rechazado

`char` esta en la tabla general de enteros porque aritmeticamente se comporta como uno (ocho bits,
sin signo, CC1-100), asi que se colaba. Pero el tipo subyacente de un enum es una decision de ABI y
"el ancho de un caracter" no responde a esa pregunta: `byte` dice exactamente lo mismo sin
ambiguedad, y C# tampoco lo admite.

Se excluye en la regla unica. **Lo que lo hace obligatorio y no opcional: el diagnostico ENUMERA
los tipos permitidos.** Si `char` se colara, el mensaje estaria mintiendo sobre su propia lista.

### Materializacion de genericos antes de resolver la sobrecarga -- limitacion conocida, no arreglada

Con `F<T>(T)` y `F<T>(T[])`, una llamada `F<int>(...)` materializa **las dos** especializaciones, y
solo despues se resuelve la sobrecarga por tipos. Si el cuerpo de la que NO se iba a elegir es
invalido para ese `T`, la compilacion falla por un candidato que nadie pidio.

Es una consecuencia deliberada del diseno del ID 132: esta pasada corre **antes** del modelo
semantico y por tanto no conoce los tipos de los argumentos; materializar todas y dejar que la capa
que si los conoce elija es lo que permitio que las sobrecargas genericas por tipos existan.

**No se arregla aqui, y la razon esta en el criterio del proyecto:** el fallo es un RECHAZO ruidoso
--el error apunta al cuerpo ofensivo-- y no una respuesta falsa. Arreglarlo de verdad significa
resolver la sobrecarga por tipos ANTES de instanciar, o sea una pasada dirigida por tipos delante
del modelo semantico. Eso es un cambio de otro tamano y necesita su propia entrega. Queda anotado
en `mkdown/114_decisiones_pendientes.md`.

---

## 9. Estado medido tras la revision

* Barrido del corpus: **OK=183 FAIL=5** (los cinco negativos intencionados).
* Los diez anchos de enum verificados leyendo el IR, incluidos los dos extremos de 64 bits.
* Orden de alias verificado en el IR antes y despues.
* Rechazos: valor que no cabe (explicito y por auto-incremento), tipo no entero, puntero, `char`,
  y el rechazo de la ruta de fichero unico.

**Lo que sigue sin medirse: la ejecucion en Windows.** `c34` no corre desde la reescritura del ID
129, `c37` no ha corrido nunca, `c38` es nueva. Las tres entran en la proxima pasada, y hasta que
esa pasada este verde este documento describe codigo compilado y leido, no codigo ejecutado.


---

## 10. Segunda revision: tres mas, y uno de ellos era mio

La segunda pasada de revision encontro tres. **Los tres eran reales, y el segundo lo introduje yo
al arreglar el cuarto de la ronda anterior** -- que es exactamente el riesgo de tocar el manejo de
extremos.

### P1 -- Los metadatos declaraban todo enum como 4/4

`LibraryMetadata.cpp` escribia `record.size = 4; record.alignment = 4;` para todo enum. Era el
**septimo** sitio con la regla a mano, y de los que quedaban **el mas caro**: los metadatos
alimentan a los generadores de host --el enlace P/Invoke de C# y el de C++/CLI-- asi que un 4/4
fijo se propaga a codigo que **otro compilador genera** y que ya nadie relaciona con este fichero.
Un `enum : byte` exportado hacia que el host reservara cuatro bytes donde el modulo pone uno.

Sale ahora de `valueLayoutForTypeKey`, la misma consulta que usan el modelo MIR y el HIR, de modo
que los metadatos no pueden discrepar del layout que el propio modulo materializa.

La cabecera C se arreglo en la ronda anterior y esta salida no. **La leccion: "el contrato ABI" no
es un sitio, son todas las salidas que lo describen** -- la cabecera, los metadatos, y cualquier
generador que lea de ellos. Arreglar una y darla por cerrada es lo mismo que arreglar cuatro copias
de seis.

### P1 -- El sucesor de `ulong.MaxValue` repetia el maximo. Mio.

`enumValueNext` saturaba: sin sucesor representable devolvia el mismo valor. El resultado:

```csharp
public enum E : ulong { Max = 18446744073709551615, Siguiente }   // compilaba
```

y `Siguiente` valia **lo mismo que `Max`**, en silencio. Dos nombres indistinguibles y un valor que
nadie escribio: **una respuesta falsa**, y del tipo que este proyecto persigue -- introducida
mientras arreglaba una respuesta falsa distinta.

Peor: el comentario que escribi al lado decia que `enumValueFits` lo rechazaria. **Era falso** --
2^64-1 cabe perfectamente en `ulong`, por eso pasaba. Un comentario que afirma una comprobacion que
no ocurre es peor que no tener comentario, porque el siguiente que lea el codigo se fia.

Hace falta un tercer estado, no una saturacion: `EnumSuccessor` lleva ahora `hasValue`, y un
enumerador implicito sin sucesor es un error con su propio mensaje:

```
enum member 'Siguiente' has no value: the previous member is the largest value of the
underlying type 'builtin:ulong', so there is no next one to count to
```

### P1 -- El descubrimiento automatico de fixtures podia quedarse obsoleto

Dos formas, las dos cerradas:

1. **La lista se lee en tiempo de configure**, y el `.cpp` no era dependencia del configure.
   Anadir una fixture y compilar no habria registrado su prueba: corrida verde midiendo una
   fixture menos. `CMAKE_CONFIGURE_DEPENDS` lo cierra.
2. **El umbral `LESS 30` era un numero escrito a mano** cuando habia treinta y seis fixtures. Un
   suelo fijo envejece: con treinta y ocho en el arbol, un regex roto que reconociera treinta y una
   pasaba el guardia y se perdian siete sin decir nada.

El guardia compara ahora contra **el directorio**, que es la unica fuente de verdad que no se puede
olvidar de actualizar: toda carpeta de `tests/cc1` con un `.nsproj` tiene que tener su prueba
registrada. Eso no envejece **y ademas caza el fallo que de verdad ocurrio dos veces este dia** --
una fixture escrita y nunca inscrita (`c37`), que no es un regex roto sino un olvido, y que ningun
suelo numerico habria detectado.

Comprobado provocando los dos fallos:

```
tests/cc1 tiene fixtures que Cc1CorrectnessTests.cpp no registra: cc1-c39-sin-inscribir.
Una fixture escrita y no inscrita no mide nada, y la corrida sale verde igual
```

### La cobertura que faltaba

La revision senalo que las pruebas existentes pasaban **y aun asi no detectaban ninguno de los dos
defectos de logica**: la de metadatos solo usaba un enum por omision de 32 bits, y `c38` no ponia
un miembro implicito detras de `ulong.MaxValue`. Las dos cosas estan ahora medidas:

* `Stage48_2LibraryMetadataTests` lleva un `enum : byte` (1/1) y un `enum : long` (8/8), y sigue
  comprobando que el enum sin declarar es 4/4.
* `checkRejected cc1-enum-underlying-no-successor`.

**Una prueba verde que no puede ver el defecto no es cobertura.** Es la misma leccion que `c34` --
que fija el orden de los alias y no lo caza porque no corre-- vista desde el otro lado.

---

## 11. Tercera revision: la suite en rojo, y dos huecos mas

### P1 -- La prueba usaba un nombre que el parser rechaza antes

`--rest` daba **1 de 349 en rojo**, y el fallo era mio: `cc1-size-on-class` declaraba
`public size(8) class C`, y **`C` es palabra reservada** (`extern C`). El parser rechazaba por el
NOMBRE antes de llegar a comprobar el modificador de layout:

```
expected class name, found 'C'          <- lo que pasaba
pack/align/size layout modifiers apply only to struct and union declarations   <- lo que medía
```

Lo que enseña no es el nombre. Es que **`checkRejected` puede pasar por el motivo equivocado**: si
el fragmento esperado hubiera sido mas laxo --"expected", pongamos-- la prueba habria salido verde
midiendo el parser en vez del modificador. Aqui salio roja porque el fragmento era especifico, que
es exactamente para lo que sirve exigir el texto del diagnostico y no solo el fallo.

### P1 -- Los metadatos seguian perdiendo el signo

Arregle el TAMANO del enum en los metadatos y lo di por cerrado. **`enum : byte` y `enum : sbyte`
miden los dos un byte**: un consumidor que solo lea el tamano no puede distinguirlos, y un
generador de host que eligiera el signo contrario convierte un 200 en un -56.

El registro de tipo lleva ahora `enumUnderlyingTypeKey` --la clave, que trae ancho y signo juntos y
es lo que ya circula por todo el compilador-- por el mecanismo de version menor que el formato ya
tenia para `ownership`: **minor 2**, escrito y leido bajo `formatMinor >= 2`.

Es la tercera vez en este trabajo que doy por cerrado un contrato ABI habiendo arreglado una sola
de sus caras. Primero la cabecera C sin los metadatos; luego los metadatos sin el signo.

### P2 -- Una fixture nueva no despertaba al guardia

El guardia del §10 compara contra el directorio con `file(GLOB)`, y **un GLOB normal se evalua una
sola vez, en el configure**. Crear una fixture nueva no lo despertaria hasta que otra cosa
provocara un reconfigure -- o sea que el guardia escrito para cazar "una fixture escrita y nunca
inscrita" **no habria visto precisamente el caso recien escrito**, que es el unico momento en que
ese olvido ocurre. Un guardia que solo funciona cuando ya no hace falta.

`CONFIGURE_DEPENDS` lo cierra: cmake comprueba el glob en cada build y vuelve a configurar si
cambio.

### Y el `PASS: 0 checks`

Un selector con una errata terminaba en `PASS: 0 checks`, que ctest cuenta como **verde**. Una
prueba que pasa sin comprobar nada es la unica forma de rojo que no se ve. Ahora un selector que no
nombra ninguna fixture registrada es un fallo con su propio mensaje.

---

## 12. Cuarta revision: la version del formato, repetida

Subir `.nsmeta` de 1.1 a 1.2 dejo **cuatro comprobaciones en rojo**, y las cuatro por el mismo
motivo: **el numero de version estaba escrito a mano en cada sitio que lo miraba.**

* `Stage48_2` exigia literalmente `format: NSMETA 1.1`.
* `Stage50_9` lo exigia en tres sitios mas -- el texto, la identidad decodificada, y, el mas
  instructivo, **la prueba de "un minor del futuro se rechaza", escrita con un `2` literal**. Al
  pasar el formato a 1.2, esa prueba dejo de comprobar un minor del futuro y paso a comprobar el
  minor ACTUAL, que obviamente se acepta.

Es la misma forma de defecto que este documento entero persigue --un valor escrito varias veces--
solo que en un numero de version en vez de en un ancho de enum. La correccion no es cambiar los
literales: **`kMetadataFormatMajor`/`kMetadataFormatMinor` son ahora publicos**, la prueba del
futuro dice `kMetadataFormatMinor + 1`, y el proximo minor no rompera nada.

Y de paso salio una segunda fuente de verdad dentro del propio fichero: el ENCABEZADO escribia la
constante mientras el CUERPO se codificaba segun `metadata.formatMinor`. Un encabezado que dijera 2
sobre un cuerpo escrito como 1 produce un fichero ilegible. El encabezado escribe ahora el minor del
modelo.

### La compatibilidad hacia atras, medida y no afirmada

`verifyCurrentCompiler` solo mira el numero: dice que un 1.1 es aceptable, **pero no decodifica
nada**. El riesgo real del minor 2 esta en el cuerpo -- un lector que leyera el campo nuevo sin
mirar el minor se comeria los bytes del campo siguiente y todo el registro saldria corrido.

Para poder medirlo hace falta poder PRODUCIR un fichero de la version anterior, asi que el codec
tiene ahora `writeModel(modelo, ruta)`, que escribe en la version menor que el modelo dice. No es
andamiaje de pruebas: sin ello, "un lector de 1.2 lee un fichero de 1.1" es una afirmacion sobre
codigo que nadie ejecuto -- y este proyecto no cuenta esas. Cada minor futuro hereda la prueba.

Ejecutado de verdad en el contenedor:

```
minor=1 types=2 t0=OldFormat.Mode/4 enumkey='' t1=OldFormat.Pair/8   verify=ok
12: minor=2 enumkey='builtin:sbyte' size=1 t1=OldFormat.Pair
```

El segundo tipo va DETRAS del enum a proposito: si el lector se hubiera comido bytes de mas al
decodificar el primero, este saldria corrido. Un solo tipo no probaria nada.

### Y la huella ABI

Comprobado que `enum : byte` y `enum : sbyte` --que miden lo mismo-- producen huellas ABI
**distintas**:

```
byte  0ad63454f67aa573
sbyte e7585f06d34e1032
```

Si la huella solo cubriera el tamano, dos modulos incompatibles tendrian la misma y el mecanismo que
existe para detectar un cambio de ABI dejaria pasar justo este.

### P2 -- El selector se usaba en `remove_all` antes de validarse

`argv[1]` entraba directo en la ruta a la que se le hace `remove_all`. Un argumento como
`../../algo` sale de `%TEMP%` y **borra otra cosa**. La comprobacion de que el selector nombra una
fixture registrada existia, pero llegaba despues: el borrado ya habria ocurrido.

La forma se valida ahora ANTES de tocar el disco -- `cc1-` y despues minusculas, digitos y guiones,
nada mas -- y ese orden es el punto, no la expresion.

---

## 13. Quinta revision: el objetivo, un campo mas alla

`writeModel` escribia `kWindowsX64Target`, `kPointerSize` y `kPointerAlignment` --las constantes--
por encima de lo que el modelo dijera. **Es exactamente el defecto que acababa de arreglar en el
encabezado con el minor, un campo mas alla**, y lo escribi yo en la misma funcion, en la misma
sesion, despues de dejar escrito por que estaba mal.

Y aqui pesa mas que en cualquier otro sitio: `writeModel` existe **para medir round-trips**. Un
campo que se sobrescribe en silencio hace que la prueba diga "el objetivo sobrevivio" cuando lo que
sobrevivio es la constante. Una prueba que se miente a si misma es peor que no tener prueba, porque
la primera se cuenta como cobertura.

Los tres salen ahora del modelo, y como NativeSharp 0.1 tiene un solo objetivo, lo correcto es
**rechazar cualquier otro con su numero delante** en vez de producir un fichero que el lector
rechazaria despues:

```
cannot write NativeSharp metadata for target 7 with pointer 8/8;
NativeSharp 0.1 targets Windows x64 only
```

El marcador de endianidad **si** sigue siendo una constante, y la distincion importa: es una
propiedad de la CODIFICACION, no del modelo. No hay campo que pueda decir otra cosa.

Medido, no razonado:

```
ok write=1   target=1 ptr=8/8
bad target write=0 err=... target 7 ... Windows x64 only
bad ptr    write=0 err=... pointer 4/8 ... Windows x64 only
minor1: minor=1 target=1 enumkey=''
```

### Lo que cinco rondas de revision dejaron claro

Trece defectos en total, y **cuatro los introduje arreglando otros**: la saturacion del sucesor, el
`class C` reservado, el minor escrito a mano en cuatro sitios, y este. La forma se repite: un
arreglo correcto, un comentario que explica bien por que, y **el mismo error cometido de nuevo una
linea mas abajo**.

La leccion util no es "revisar mas". Es que **la duplicacion se cuenta buscandola, no
recordandola** -- vale para los seis sitios del ancho de enum, para los cuatro del numero de
version, y para los tres campos del encabezado. Cada vez que este documento dijo "y ahora esta en
un solo sitio" sin haber hecho la busqueda, quedaba al menos uno fuera.

---

## 14. La corrida de Windows: 117 de 118, y lo que el verde de c39 MIDE

Primera pasada completa con todo esto dentro. **117 trabajos en verde de 118.** El unico rojo es el
bug de los enums estrechos con signo:

* `c38 -O0` -> 42.
* `c38 -O2` -> **39**, o sea `OpacoConSigno(ConSigno.MenosTres).ToString()`.
* La pasada uso `lld-link`, y con NSLink da lo mismo: **el fallo ocurre antes del enlazador, o al
  margen de el.**

### El verde de c39 no es "una prueba mas que pasa": es la medida del alcance

`c39_enum_nombre_o2` pasa a -O0 **y a -O2**, y eso acota el defecto por MEDICION. Lo que c39 cubre
y responde bien con optimizacion:

| Forma | Cubierto por c39 a -O2 |
|---|---|
| `enum : byte` con 200 y 255 -- constante, por local, por funcion, no plegable | verde |
| `enum : sbyte` con valores POSITIVOS (2, 127) | verde |
| `enum : ushort` (60000), `enum : long` (5000000000), `enum` sin declarar | verde |
| el camino de RESERVA (valor fuera del enum -> nombre del tipo), en dos anchos | verde |
| el operador de cadenas sobre las mismas cadenas ya verificadas byte a byte | verde |

De ahi salen tres exclusiones que ya no son conjetura:

1. **NO es "un enum de un byte"**: `enum : byte` con 200 y 255 pasa. Un byte no basta.
2. **NO es "un tipo subyacente con signo"**: `enum : sbyte` con 2 y 127 pasa. El signo del TIPO no
   basta.
3. **NO es el camino de reserva ni el operador de cadenas**: los dos pasan a -O2 en c39.

Queda una sola combinacion: **tipo estrecho CON signo Y valor NEGATIVO**. Sumado a lo que la
primera corrida de triage ya habia demostrado --que basta un receptor CONSTANTE, luego no es la ABI
del retorno de ocho bits-- el defecto esta acotado a un punto, y todo por medicion.

### Lo que falta, y es UN numero

La linea de la etapa 5:

```
[VIVO] b9_enum_estrecho_o2 -O2 -- exit N
```

* **20-26** -> el contenido producido es incorrecto. El defecto esta en la produccion de la cadena.
* **30-36** -> el contenido es correcto byte a byte y falla el operador de comparacion.
* **10-16** -> el valor ya era otro antes de pedir el nombre.

Y el arreglo se escribe sabiendo cual de las tres es, en vez de tocar `lowerEnumName` a ciegas.
