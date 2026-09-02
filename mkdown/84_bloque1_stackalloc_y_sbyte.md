# 84 — Bloque 1: IDs 123 (`stackalloc`) y 89 (`sbyte`)

**Fecha:** 2026-09-02
**Bloque:** 1 del doc 81 — Correctness del frontend.
**Cierra:** el **1a** entero (121 ✅, 122 ✅, 123 ✅) y **la mitad mecánica del 1b** (89 ✅).
**Queda abierto:** ID 82 y el residuo de SHR-005 — §7, con la medición de por qué.
**Estado:** implementado y medido en el host; **falta la corrida en Windows** (`run_bloque1.bat`).

---

## 0. Cómo leer este documento

Cada sección va en el mismo orden: **qué estaba mal**, **cómo se comprobó que estaba mal**, **dónde
se decidió arreglarlo y por qué ahí y no en otro sitio**, y **qué mide la prueba**. Es el orden en
que se hizo el trabajo, no un resumen escrito después.

La regla que gobierna todo lo de abajo es la del proyecto:

> **Ninguna verificación cuenta si el modelo contra el que verifico lo construí yo.**

En la práctica: no hay una sola afirmación aquí que no nombre el archivo y la línea, o el resultado
de haber compilado el código y haberlo llamado.

---

## 1. ID 123 — `stackalloc` no comprobaba casi nada

### 1.1 Qué había, medido

`stackalloc T[n]` sí se validaba, y conviene decir qué se validaba para no repetir el error de
inventar un hueco que no existe. En `ProjectMethodBodyModel.cpp`, en el caso
`StackallocExpression`:

* que el elemento tenga tamaño — `void` no lo tiene;
* que su vida sea trivial — sin destructor y sin campos `owned`, porque un buffer de pila no
  ejecuta destructores y la memoria desaparece bajo los pies del objeto;
* que el contador sea integral — `stackalloc int[unaCadena]` se rechaza.

**Y nada más.** Estas dos líneas pasaban enteras:

```csharp
int* a = stackalloc int[-1];
byte* b = stackalloc byte[1073741824];   // un gigabyte
```

La primera llega a LLVM como un `alloca` con un contador que, leído sin signo, es 2^64-1. La
segunda reserva un gigabyte en una pila de un mega. Ninguna de las dos produce una palabra del
compilador: el programa compila, enlaza, y muere al ejecutarse.

### 1.2 La medición que decidió el reparto — y es la parte que importa

El doc 60 §8 enumeraba cuatro comprobaciones que faltaban. La pregunta interesante no era cuáles,
sino **dónde**. Y ahí la respuesta no era libre:

**Dos de las cuatro necesitan el TAMAÑO DEL ELEMENTO, y el modelo semántico no lo tiene.** No es
una carencia que se pueda tapar: el orden de construcción del pipeline es *modelo de cuerpos de
método primero, `ProjectObjectLayoutModel` después*, y `ProjectMethodBodyModel::build()` ni
siquiera recibe el modelo de layout. Está escrito en el árbol, en el comentario de
`SizeofConstant` en `ProjectMethodBodyModel.h`, y por la misma razón: `sizeof` tuvo este problema
antes y se resolvió igual.

Las salidas posibles eran tres, y dos son malas:

| Salida | Por qué no |
|---|---|
| Reordenar el pipeline para que el layout corra antes | Un cambio estructural en todo el frontend para añadir dos comprobaciones |
| Duplicar las reglas de tamaño en el modelo semántico | **Exactamente** la duplicación que el milestone CC1 existe para quitar. Una segunda tabla de tamaños puede divergir de la primera, y ese es el defecto ID 78 otra vez |
| **Repartir por capa según lo que cada una sabe** | Elegida |

**El reparto:**

```
ProjectMethodBodyModel   contador constante negativo     necesita solo el contador
(modelo semántico)       stackalloc dentro de un bucle   necesita solo el contexto

ProjectOopHirModel       que la reserva quepa            necesita el tamaño del elemento
(lowering a HIR)
```

**Lo que se gana y lo que se pierde con ese reparto, dicho sin adornos.** Las dos que están en el
modelo semántico salen por el motor de diagnósticos: llevan el rango del código fuente y un texto
escrito para la persona. La del HIR sale por su cadena `error`, que habla de etapas internas. Es
una limitación real de esa capa —el HIR no tiene motor de diagnósticos— y no una elección; queda
anotada como deuda.

### 1.3 Las comprobaciones, una por una

**(1) Contador constante negativo.**

```csharp
int* buffer = stackalloc int[-1];
// error: stackalloc element count is negative (-1); a reservation cannot have
//        fewer than zero elements
```

**Esta comprobación no se podía escribir antes de esta ronda**, y merece la pena entender por qué:
hasta el ID 119, el unario `-` no propagaba la constancia de su operando, así que `-1` **no llegaba
al análisis como constante**. No había nada que mirar. El 119 lo cerró, el 121 y el 122 hicieron
que además se plieguen las formas compuestas, y sólo entonces `stackalloc byte[2 - 5]` se puede
rechazar. Las cuatro tareas son una cadena, no una lista.

**La decisión fina: se mira el TIPO, no los bits.** Un contador `uint` con el bit alto puesto —
`3000000000u`— no es negativo. Si la comprobación leyera `bits < 0`, ese literal perfectamente
legítimo se rechazaría. El chequeo es
`isSignedInteger(typeKey) && asSigned() < 0`, en ese orden.

**(2) `stackalloc` dentro de un bucle.**

```csharp
while (round < 3)
{
    short* pair = stackalloc short[4];   // warning
    ...
}
```

Es la trampa clásica de C: un `alloca` dentro de un bucle reserva **otra vez** en cada vuelta y
**nada se libera hasta salir de la función**. La pila crece con las iteraciones, no con la
profundidad de llamada. Mil vueltas con `stackalloc int[256]` son un mega de pila, y el programa
parece correcto leyéndolo.

**Aviso y no error, y es deliberado.** La forma es legítima cuando el bucle da pocas vueltas, y
este lenguaje no tiene con qué demostrar cuántas da. Un error prohibiría código correcto; callarse
deja pasar un desbordamiento de pila con forma de programa sano. El aviso es lo único honesto.

> **Es el primer warning del compilador.** `DiagnosticSeverity::Warning` existía y
> `DiagnosticEngine::warningCount()` también, pero **ningún sitio del árbol los usaba** — barrido
> completo: cero apariciones de `DiagnosticSeverity::Warning` en `src/`. Comprobado antes de
> emitirlo que no rompe nada: el éxito se decide con `hasErrors()`, que sólo cuenta los de
> severidad `Error`.

**(3 y 4) Que la reserva quepa — y por qué son UNA sola comprobación.**

El doc 60 pedía dos: una cota superior, y el desbordamiento del producto `count * elementSize` en
64 bits. La forma obvia de escribirlas es:

```cpp
if (elements * elementSize > kLimite)   // MAL
```

y esa línea **desborda justo en los casos que quiere atrapar**: `stackalloc int[1<<62]` da un
producto que envuelve a 0 y pasa la comprobación tan campante. Por eso hay que vigilar el
desbordamiento aparte… **si se multiplica**. Dividiendo:

```cpp
if (elementSize != 0 && elements > kMaximumStackallocBytes / elementSize)
```

la comparación es exacta para todo `elements` de 64 bits, **no se multiplica nunca**, y el
desbordamiento del producto deja de existir **por construcción** en vez de vigilarse con una
segunda comprobación. Dos ítems de la lista se cierran con una línea porque la línea está bien
escrita, no porque se haya recortado el alcance. La prueba negativa
`cc1-stackalloc-count-overflows-product` fija exactamente ese caso.

**De dónde sale el límite.** Nada en el árbol le pasa `/STACK` a `lld-link` —comprobado: cero
apariciones en `src/Linker/`— así que el PE se queda con la reserva por omisión del enlazador, que
en Windows x64 es 1 MiB. **Una sola reserva de 1 MiB no puede caber ahí**, porque el marco de la
función también ocupa. El límite rechaza sólo lo que falla seguro y no opina sobre lo que quizá
quepa; y si algún día el enlazador recibe un `/STACK` mayor, la constante se mueve con él y lo dice
en su comentario.

**Sólo cuando el contador es constante.** Con un contador en tiempo de ejecución no hay nada que
decidir en compilación, y este lenguaje no promete comprobar la pila.

---

## 2. ID 89 — `sbyte`

### 2.1 Por qué el tipo existe

No es simetría de catálogo. `signed char` es un parámetro corriente en las cabeceras de C, y sin
`sbyte` **no había forma de declararlo honestamente**: había que mentir con `byte` y dejar que el
signo se perdiera en la frontera. Este lenguaje existe, entre otras cosas, para declarar APIs de C.

### 2.2 Por qué es arriesgado y no mecánico

`sbyte` no es una fila más en una tabla. Es **un ancho que ya existía —8 bits, el de `byte`— con el
signo cambiado**. Cada capa que decide algo *por el signo* tiene que enterarse, y son varias,
escritas por separado **a propósito**: el modelo semántico, el evaluador de constantes, el
clasificador de conversiones, el MIR y el backend tienen cada uno su propia tabla, para poder
contestar sin depender de los demás.

**Si una sola se queda sin la fila, no hay error de compilación.** Hay un `sbyte` que se divide como
si fuera `byte`, o que se extiende con ceros donde tocaba extender el signo, y el programa da el
resultado equivocado **en silencio**. Es la forma exacta del defecto C-002, que dividía un `ulong`
con signo.

### 2.3 El inventario, medido

El doc 60 §10 listaba once capas. El barrido del árbol dio **doce sitios**, y uno de ellos no
estaba en la lista:

| # | Dónde | Qué |
|---|---|---|
| 1 | `include/.../Lexer/TokenKind.h` | El enumerador `KwSByte` |
| 2 | `src/Lexer/Lexer.cpp` | La palabra `sbyte` en la tabla de keywords |
| 3 | `src/Lexer/TokenKind.cpp` | Su nombre, para los volcados |
| 4 | `include/.../Target/TargetDataLayout.h` | `BuiltinLayoutKind::SByte` |
| 5 | `src/Target/TargetDataLayout.cpp` | Su tamaño y alineación: `{1, 1}` |
| 6 | `src/Frontend/ProjectObjectLayoutModel.cpp` | Keyword → layout kind, y typeKey → tamaño |
| 7 | `src/Frontend/ProjectTypeConversion.cpp` | **Siete tablas**: numéricos, integrales, ancho/signo, ensanchamientos, promoción integral, `constantFits`, preferencia |
| 8 | `src/Frontend/ProjectIntegerEvaluator.cpp` | La tabla de ancho y signo del evaluador |
| 9 | `src/Frontend/ProjectMethodBodyModel.cpp` | Signo, ancho, y la promoción del shift |
| 10 | `src/Frontend/ProjectOopMirModel.cpp` | Signo y ancho |
| 11 | `src/LLVM/ProjectOopLlvmLowerer.cpp` | Signo, ancho y el tipo LLVM (`i8`) |
| 12 | `src/Interop/CHeaderGenerator.cpp` | `int8_t` |
| **13** | **`src/Frontend/ProjectMemberModel.cpp`** | **`isStage47CAbiType` — la lista de tipos que cruzan la ABI de C** |

**El decimotercero es el que hace que el tipo sirva para algo.** Salió de un barrido de
`"builtin:ushort"` por todo `src/` —usando un tipo estrecho existente como sonda para encontrar
listas que enumeren la familia— y no de leer la lista del doc 60. Si `sbyte` estuviera en las doce
primeras y no en esa, tendría ancho, signo, layout, conversiones y tipo LLVM correctos… y
`extern C void f(sbyte x);` se rechazaría. El tipo no serviría para lo único que se pidió.

### 2.4 El sitio que el enumerador NO puede ocupar, y por qué

`isBuiltinTypeKeyword` es **una comparación de rango**, no una lista:

```cpp
return kind >= TokenKind::KwVoid && kind <= TokenKind::KwString;
```

Un enumerador de tipo colocado **fuera** de ese rango compila sin una queja y luego no se reconoce
como tipo en ninguna de las ocho llamadas del parser. Uno colocado **dentro** por accidente
convertiría en tipo a lo que no lo es. `KwSByte` va entre `KwBool` y `KwByte`: con su familia y
dentro del rango.

**Riesgo colateral, y cómo se controla:** las dos funciones `…NodeKindName` no tienen `default`, así
que un `case` que falte se caza con `-Wswitch`. Por eso `run_bloque1.bat` compila con
warnings-as-errors **antes** de correr un solo test.

### 2.5 El error que la medición atrapó

La primera versión pasaba todas las tablas de ancho y signo, y **fallaba** en el probe con este
caso:

```
sbyte + int(3)   →  builtin:int      (esperado: builtin:sbyte)
```

`sbyte` no estaba en la lista de **destinos estrechos** de la regla del ID 120 —la que hace que un
operando constante se adapte al otro— así que `sbyte s = 10; sbyte t = s + 3;` no habría compilado,
mientras que la misma línea con `short` sí. La lección, escrita en el propio archivo:

> Un tipo nuevo tiene que entrar en **todas** las reglas que nombran a su familia, no sólo en las
> tablas de ancho y signo.

Esa falla la vio un probe, no una lectura. Es la diferencia entre las dos formas de trabajar.

### 2.6 Las decisiones de diseño

**`sbyte` ensancha SÓLO a destinos con signo** — `short`, `int`, `long`, `nint`, `float`, `double`.
Nunca a `ushort`, `uint`, `ulong`, `nuint`. Meterlos ahí haría que `uint u = miSByte;` compilara y
envolviera en silencio. Y por el mismo motivo `byte` **no** ensancha a `sbyte`: 200 no cabe en un
entero de 8 bits con signo.

**Sus límites son -128 y 127**, con el mínimo uno más en magnitud que el máximo, igual que `short` e
`int`. Lo decide la rama de negativos de `constantFits`, que ya existía desde el ID 119.

**En la tabla de preferencia va al final**, y es la posición más conservadora que existe: el bucle
devuelve el **primer** candidato al que llegan los dos operandos, así que una entrada al final no
puede cambiar ninguna respuesta que la tabla ya daba. Sólo contesta donde antes se devolvía
`nullopt`.

---

## 3. Cómo se midió, aquí y ahora

**Compilando `ProjectTypeConversion.cpp` sola con `-Wall -Wextra` y llamándola** — el mismo método
con el que se cerraron los IDs 120, 121 y 122, y con el que se descubrió que la primera versión de
la regla del 120 rompía `(-1) + 1L`.

**29 casos, cero fallas.** Nueve son regresiones de los IDs 119, 120, 121 y 122; veinte son nuevos:

| Grupo | Casos |
|---|---|
| Tipo común con `sbyte` | `sbyte + int(3)` → sbyte · `sbyte + int` → int · `sbyte + byte` → int · `sbyte + long` → long · `sbyte + uint` → long · `sbyte + ushort` → int · `sbyte + double` → double |
| Qué cabe | 127 sí · 128 no · 200 no · -128 sí · -129 no · (y `byte = -1` sigue rechazándose) |
| Ensanchamientos | a `short`, `int`, `double` sí · a `uint` no · a `byte` no · `byte`→`sbyte` no |

Diez de esos casos quedaron escritos como aserciones en `tests/unit/Cc1CorrectnessTests.cpp`, no en
un programa de usar y tirar, y **el bloque entero se compiló contra el `ProjectTypeConversion.cpp`
real con `-Wall -Wextra` y pasa**.

**Lo que NO está medido todavía, y hay que decirlo:** ni `c18` ni `c19` se han compilado ni
ejecutado, y las nueve pruebas negativas tampoco se han corrido. Todo eso se mide en la corrida de
Windows. En particular hay **un riesgo conocido**: las dos pruebas negativas del tamaño
(`cc1-stackalloc-too-large` y `cc1-stackalloc-count-overflows-product`) esperan el texto
*"does not fit on the stack"*, que sale por la cadena `error` del HIR; si el driver lo envuelve con
otro formato, el fragmento a buscar hay que ajustarlo. Es una prueba que puede salir roja **por su
propia forma** y no por un defecto — la primera cosa a mirar si sale roja.

---

## 4. Qué se tocó

| Archivo | ID | Qué |
|---|---|---|
| `src/Frontend/ProjectMethodBodyModel.cpp` | 123 | Contador constante negativo, y el aviso de bucle |
| `src/Frontend/ProjectOopHirModel.cpp` | 123 | Que la reserva quepa; `<cstdint>`, `<optional>` y `<string>` explícitos |
| `include/nativesharp/Lexer/TokenKind.h` | 89 | `KwSByte`, dentro del rango |
| `src/Lexer/Lexer.cpp` | 89 | La palabra |
| `src/Lexer/TokenKind.cpp` | 89 | El nombre |
| `include/nativesharp/Target/TargetDataLayout.h` | 89 | `BuiltinLayoutKind::SByte` |
| `src/Target/TargetDataLayout.cpp` | 89 | `{1, 1}` |
| `src/Frontend/ProjectObjectLayoutModel.cpp` | 89 | Keyword → layout, typeKey → tamaño |
| `src/Frontend/ProjectTypeConversion.cpp` | 89 | Siete tablas |
| `src/Frontend/ProjectIntegerEvaluator.cpp` | 89 | Ancho y signo |
| `src/Frontend/ProjectOopMirModel.cpp` | 89 | Ancho y signo |
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | 89 | Ancho, signo y `i8` |
| `src/Interop/CHeaderGenerator.cpp` | 89 | `int8_t` |
| `src/Frontend/ProjectMemberModel.cpp` | 89 | **La ABI de C** |
| `tests/cc1/c18_stackalloc_hardening/` | 123 | Fixture nueva |
| `tests/cc1/c19_sbyte/` | 89 | Fixture nueva |
| `tests/unit/Cc1CorrectnessTests.cpp` | ambos | Registro, 9 negativas, 2 aceptadas, 10 aserciones |
| `run_bloque1.bat` | ambos | Build con `/WX`, los tres ctest |

**No se tocó `CMakeLists.txt`:** las fixtures `cc1` se registran dentro de
`Cc1CorrectnessTests.cpp`, no ahí.

---

## 5. Qué mide cada fixture

**`c18_stackalloc_hardening` es la mitad POSITIVA, y esa es toda su razón de ser.** Las reservas
imposibles no caben en una fixture que tiene que compilar entera; van en `checkRejected`. Lo que
esta fixture mide es lo que **ninguna prueba negativa puede medir: que el endurecimiento no tiene
falsos positivos.** Un límite escrito demasiado estrecho, o una comprobación de signo que lea los
bits en vez del tipo, rompen código correcto — y eso no se ve en las pruebas negativas, se ve aquí,
porque el archivo deja de compilar. Contiene: un contador sin signo con el bit alto puesto, una
reserva constante razonable, un contador de ejecución, cero elementos, un contador plegado (que se
apoya en el 121), y un `stackalloc` en un bucle que **avisa y compila**.

**`c19_sbyte` enfrenta a `sbyte` con `byte`** en cada operación donde el signo cambia el resultado y
no sólo su lectura: extensión (`sext` contra `zext`), división y módulo (`sdiv`/`srem` contra
`udiv`/`urem`), desplazamiento a la derecha (`ashr` contra `lshr`), comparación (predicado con
signo contra sin signo) y el cast que estrecha (`(sbyte)200` vale -56, `(byte)200` vale 200). Más
el layout dentro de un agregado, con `sizeof` y el paso de un puntero enfrentados entre sí como
hace `c16` con `enum`.

---

## 6. Cómo se corre

```
run_bloque1.bat
```

Compila con warnings-as-errors —el ID 89 añade un enumerador y `-Wswitch` es la red que caza un
`case` olvidado—, luego `nsc.cc1.integer-evaluator` y `nsc.cc1.correctness` por separado, y al
final la suite entera. Deja `_logs\bloque1\`.

Para **ejecutar** las fixtures en Windows con su código de salida real, después:
`run_windows_checklist.bat`.

---

## 7. Lo que queda del Bloque 1, y por qué no está aquí

Dos ítems, y en los dos la medición dice exactamente qué falta.

### ID 82 — compuesta sobre propiedad e indexador

**Dónde se rechaza hoy:** `ProjectMethodBodyModel.cpp`, en `analyzeAssignment`:

```
"compound assignment to a Stage 38 property/indexer is deferred;
 use an explicit read-modify-write assignment"
```

**Por qué no es un permiso que se pueda quitar.** `p.X += 3` significa
`set(receptor, get(receptor) + 3)`, y **el receptor y los argumentos del indexador tienen que
evaluarse UNA sola vez**. Esa es la propiedad que el ID 81 cerró para destinos con dirección, y que
la fixture `c14` mide contando llamadas en el IR: `buffer[Probe()] += 1` llama a `Probe` una vez.

El HIR baja una asignación a propiedad con `lowerResolvedCall(setterCall, arguments)`, donde
`arguments` son **nodos de sintaxis**. Bajar el mismo índice dos veces —una para el getter y otra
para el setter— lo evaluaría dos veces. Y el árbol HIR se construye con `unique_ptr`, así que **un
nodo no se puede compartir entre dos padres**: no hay forma de bajarlo una vez y usarlo dos.

**Lo que falta, nombrado:** el HIR no tiene ningún nodo que ligue un valor a un temporal y lo reuse
dentro de la misma expresión. Los 130 `ProjectOopHirNodeKind` incluyen `CompoundStore` (lee, opera
y guarda **sobre una dirección** — una propiedad no la tiene) y `TemporaryAddress` (da dirección a
un valor sin casa, pero no permite reusarlo). **Cerrar el 82 es añadir esa facilidad al HIR y
sostenerla en el MIR y en el backend** — tres capas y un tipo de nodo nuevo. Es una ronda propia,
no un parche.

### SHR-005 — residuo de owned destination

**Dónde está, y es una sola expresión.** `ProjectMethodBodyModel.cpp`, línea ~8783:

```cpp
ownedFieldOwnsStorage = ownedFieldMoveTarget &&
    (isReferenceTypeKey(targetMember.declaredTypeKey) ||
     targetMember.declaredTypeKey == "builtin:string");
```

Un campo `owned` cuyo tipo declarado sea un **arreglo fijo** (`T[n]`) o un **agregado por valor**
(un struct) **no** cuenta como «posee almacenamiento», así que se salta la exigencia de expresión de
transferencia. Es exactamente el residuo que el doc 62 describe, y ahora está localizado en una
condición de dos términos.

**Por qué no se cierra en esta entrega.** La otra mitad de esa regla —liberar el valor desplazado—
vive en el HIR y se apoya en `requiresOwnedDestruction(node->typeKey)`. Saber si las dos mitades
quedan de acuerdo para un arreglo fijo y para un struct por valor **se verifica contando
destrucciones en ejecución**, que es como este proyecto verifica ownership
(`Stage50_11OwnershipExecutionTests`, 174 comprobaciones). Escribirlo sin poder ejecutar esa suite
sería exactamente el trabajo que la regla de método prohíbe: un modelo construido por mí,
verificado contra sí mismo.

**Queda especificado hasta la línea**, que es el estado en el que los IDs 121 y 122 entraron y por
el que salieron rápido.
