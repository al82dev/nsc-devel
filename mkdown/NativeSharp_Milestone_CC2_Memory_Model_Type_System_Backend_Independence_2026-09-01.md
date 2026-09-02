# NativeSharp — Milestone CC2

## Memory Model, Type-System Completion and Backend Independence

**Abierto:** 2026-09-01 · **Alcance:** el compilador y su biblioteca. Sólo eso.
**Predecesor:** CC1 (`77_milestone_cc1.md`), cerrado 2026-08-31.
**Entrada:** `78_huecos_del_lenguaje_completo.md` (barrido del lenguaje contra C#),
`79_ids_79_80_81_83.md` (cierre de `++`/`--`, compuestos y `p->`), y las mediciones de esta sesión.

---

## 1. Evaluación global de madurez

Empiezo por lo que me hace desconfiar de mi propia evaluación: **en esta sesión, tres de los
hallazgos nuevos salieron de fixtures que escribí para otra cosa.** No de leer código, no de
planificar: de ejecutar. Eso dice más sobre el estado del proyecto que cualquier adjetivo, y en las
dos direcciones — el compilador aguanta que lo empujen, y todavía hay cosas que sólo aparecen cuando
se empuja.

| Área | Madurez | Sobre qué se apoya el juicio |
|---|---|---|
| **Interop / ABI Win64** | **Alta** | 11 fixtures de contrato que ejecutan a `-O0` y `-O2`: punteros a función, structs por valor, arreglos fijos, uniones, `ref`/`out`/`in`, dos librerías con el mismo símbolo |
| **EH** | **Alta** | funclets reales, `catch`/`finally`/`using`, cinco fixtures de salida estructurada, y EH-001 medido **en el IR** porque el TLS no es observable desde el lenguaje |
| **Ownership / lifetimes** | **Alta** | `owned`/`borrowed`/`move`/`raii`, asignación definida, snapshot/join en `?:`, `&&`, `\|\|`, `do…while` |
| **Backend LLVM** | **Media-alta** | genera y ejecuta, cero avisos con `/W4 /WX`; pero `LlvmCApi.h` es una superficie declarada a mano de 242 símbolos y no hay capa intermedia |
| **OOP** | **Media-alta** | clases, interfaces, virtuales, propiedades, indexadores, eventos, delegados; sin tipos anidados y sin constructores estáticos |
| **Generics** | **Media** | monomorfización real y cruzando proyectos; **sin restricciones** y **sin interfaces genéricas** |
| **Sistema de tipos** | **Media** | 14 primitivos, punteros, arreglos fijos y dinámicos; **sin `sbyte`**, sin `decimal`, `char` de 8 bits |
| **Memoria / concurrencia** | **Baja** | hay hilos, mutex, monitores, semáforos y atómicos **en la biblioteca**, y **ningún modelo de memoria en el lenguaje** |
| **Unicode** | **Baja** | `char` es un byte; `String` declara explícitamente alcance ASCII |
| **Reproducibilidad** | **Alta** | manifiesto con generador, verificador y auditoría; identidad de build al día |

**El juicio en una frase:** el núcleo es sólido y está bien medido; lo que queda no son bugs sueltos
sino **tres decisiones de diseño sin tomar** — el modelo de memoria, el ancho de `char`, y la
frontera con LLVM. CC2 es el milestone que las toma.

---

## 2. Qué mejoró respecto a la auditoría anterior

| | Auditoría (2026-08-31) | Hoy |
|---|---|---|
| IDs del registro CC1 | 27 abiertos | **23 cerrados**, 4 diferidos con razón escrita |
| checklist de Windows | 45/45 | **67/67** |
| `ctest` | 239 | **242** |
| formas con «mitad delantera presente, trasera ausente» | 4 conocidas | **0 conocidas** (`sizeof`, `stackalloc`, `new T`, punteros — y ahora `++`/`--`, compuestos, `p->`) |
| bugs reportados vivos | 16 abiertos | **0** |
| inventario del lenguaje | no existía | **completo y contado** contra las 125 palabras de C# |

**Lo cerrado desde la auditoría, con su documento:** los cinco P1 de constantes y `char` (C-001…
C-005), los cinco de flujo (`?:`, `&&`, `||`, `do…while`, ownership del ternario), EH-001, las
cuatro formas del pipeline moderno, y la reproducibilidad. Más, en esta sesión: `++`/`--`, los diez
operadores compuestos, `p->campo`, `p->Metodo()` y el layout de `enum`.

---

## 3. Estado actual, área por área

### 3.1 Memoria
Locales alojadas en memoria (`alloca`), structs indirectos como `[N x i8]`, `stackalloc`, `new T`
crudo, `delete`, aritmética de punteros con paso del tipo. **No hay** `volatile`, `checked`, ni
ninguna barrera expresable desde el lenguaje. Ver §6.

### 3.2 OOP
Completo salvo **tipos anidados** (el parser de miembros nunca mira `class`/`struct`/`enum`/
`interface`/`delegate`) y **constructores estáticos** (rechazados explícitamente). Structs sin
destructor y sin virtuales, **por decisión escrita y medida**, no por falta: con la puerta abierta
compilan y se portan mal en silencio.

### 3.3 Generics
Monomorfización real, con especialización cacheada por `typeSpelling` y soporte cross-project.
**`where` se parsea y se rechaza** en tres sitios con mensaje propio. **Las interfaces no toman
parámetros de tipo**: `parseGenericParameterList` se llama en clases, structs, delegados, métodos y
funciones libres — nunca en `parseInterfaceDeclaration`.

### 3.4 EH
Sin cambios desde CC1 y es de lo mejor del árbol. Un hueco medido queda en §7.

### 3.5 Interop
Lo más maduro. `ref`/`out` son palabras **contextuales**, no keywords — se reconocen por lexema en
`Parser.cpp`. Anotado porque ninguna documentación anterior lo decía y la tabla de keywords, sola,
miente.

### 3.6 Ownership
Sin cambios. `OwnershipFlowSnapshot` con reachability, moves must/may, borrows y `assignedLocals`.

---

## 4. CC2-A — El bug P1 de layout de `enum`

**Estado: CERRADO en esta sesión.** Se documenta porque la forma del defecto es la que importa.

Esta clase tiene **dos** servicios de tamaño:

* `layoutNamedType` → da el offset de un campo dentro de un struct. Para un `enum`: **4**.
* `valueLayoutForTypeKey` → da el paso de un arreglo, el resultado de `sizeof`, el tamaño de un
  `stackalloc` y el paso de la aritmética de punteros. Para un `enum`: **8**.

El enum caía hasta un `return {{8, 8}}` final que existe para las **referencias** — clase, interfaz,
delegado — porque todas ellas miden un puntero. Un enum es un tipo **por valor**.

**Por qué es P1 y no una fealdad.** Con `struct S { Color a; Color b; }`, el campo `b` está en el
offset 4, pero `Color* p = &s.a; p + 1` avanzaba **ocho** bytes: pasado `s.b` por completo, fuera
del struct. Escribir por ahí corrompe memoria ajena.

**Cómo se cerró:** `valueLayoutForTypeKey` pregunta ahora por `BuiltinLayoutKind::Enum`, el mismo
sitio del que sale la respuesta de `layoutNamedType`. Una sola regla de tamaño.

**Cómo se mide:** `tests/cc1/c16_enum_layout` no compara contra un número que yo haya escrito —
**enfrenta los cuatro consumidores del tamaño entre sí**: `sizeof(Color)`, `sizeof(Pair)`, la
distancia entre `&pair.First` y `&pair.Second`, el paso de un `Color*` y el de un `Color[]`. Si
vuelven a discrepar, discrepan ahí.

---

## 5. CC2-B — Promociones en evaluación de constantes

### 5.1 Los shifts: ya estaba arreglado
La auditoría pedía revisar «promociones incorrectas de shifts». **Medido: ya está cerrado** por
CC1-103 / C-004. `ProjectIntegerEvaluator` saca los shifts del camino del tipo común: el resultado
toma el tipo del **operando izquierdo** y el contador se valida contra el ancho de ese tipo, no
contra los 64 bits de la representación interna. Los tres casos — `<<`, `>>` con signo, `>>` sin
signo — tienen su rama y su razón escrita. **No hay tarea aquí.**

### 5.2 Lo que sí encontré, midiendo

Compilé `ProjectTypeConversion.cpp` sola y la llamé directamente. Tres hallazgos, dos ya cerrados:

**ID 119 — un literal negativo perdía su constancia. CERRADO.**
El análisis de unario `+` propagaba `integerConstant` y el de unario `-` no. Consecuencia:
`short s = 3;` compilaba y **`short s = -3;` no** — el mismo literal, con signo, dejaba de caber.
Lo destapó la fixture `c13_increment` al declarar `short smallS = -3;`.
Arreglado guardando la **magnitud más una bandera** (`ProjectIntegerConstant::isNegative`), no el
valor negado: `value` se compara contra cotas superiores y `0xFFFF…FD` no pasaría ninguna. Un
negativo sigue sin caber en `byte`, `char`, `ushort`, `uint`, `ulong` ni `nuint`, que es lo que
mantiene `byte b = -1;` rechazado.

**ID 120 — el operando constante no se adaptaba al otro. CERRADO.**
Medido: `commonNumericType("builtin:short", "builtin:int", —, const 3)` devolvía **`builtin:double`**.
Con una constante que cabe, las conversiones existen **en las dos direcciones**, así que ninguna de
las dos reglas de decisión disparaba y se caía al bucle de preferencia, ordenado de más ancho a más
estrecho, que acierta `double` siempre. Consecuencia: **`short s; short t = s + 3;` no compilaba**, y
`byte b; b = b + 1;` tampoco.
Arreglado con la regla de C#: una expresión constante que cabe en el tipo del otro operando toma
ese tipo.
**Y estrechada tras medirla otra vez:** la primera versión decía «si un lado convierte por
constante, gana el otro», y eso rompía `(-1) + 1L` — el `1L` es explícitamente `long`, la conversión
`long → int` por constante existe, y la regla ancha lo habría evaluado en `int`. Un literal que la
persona escribió ancho no debe estrecharse en silencio. La regla final sólo se aplica **desde un
literal `int`** hacia `byte`, `char`, `short` o `ushort`, que es exactamente lo que estaba roto.

**ID 121 — dos constantes enteras de anchos distintos dan `double`. ABIERTO.**
Medido: `commonNumericType("builtin:int", "builtin:long")` con constantes en los dos lados devuelve
**`builtin:double`**, por la misma razón del 120 y en un caso que la regla estrecha no cubre.
Consecuencia: `ProjectIntegerEvaluator` comprueba `isIntegerType(*common)`, ve `double`, y **se
rinde**: `const long x = 1 + 1L;` no se pliega.
**No se arregla aquí a propósito.** Arreglarlo bien es escribir la regla de promoción entera de C#
— si alguno es `long`/`ulong`, el común es `long`/`ulong`; si no, `int` — y eso es un cambio de
diseño en el corazón de la aritmética, no un parche. Es una tarea de CC2 con su propia fase.

**ID 122 — `ProjectIntegerConstant` tiene dos convenciones. ABIERTO.**
`ProjectMethodBodyModel` lo construye con una **magnitud**; `ProjectIntegerEvaluator` lo construye
con `bits`, un **patrón de dos complementos de 64 bits**. La misma estructura, dos significados, y
`constantFits` sólo entiende el primero. Hoy no produce una respuesta incorrecta — un patrón
negativo se lee como una magnitud enorme y simplemente no cabe en nada — pero es la forma exacta de
duplicación semántica que CC1 existe para quitar, y el ID 121 no se puede cerrar bien sin resolverla
antes.

---

## 6. CC2-C — El modelo de memoria y `volatile`

**Es la única cosa de todo el registro donde un programa correcto se comporta mal en silencio.**
Todo lo demás falla ruidosamente.

### 6.1 El hecho medido
`lib/NativeSharp.Core.Threading/` trae `Thread`, `Mutex`, `Monitor`, `Semaphore`, `Event`,
`ScopedLock` y `Atomics` — con diez operaciones núcleo interlocked bajadas a LLVM. **Hay hilos de
verdad.** Y no hay forma de escribir en este lenguaje una lectura que el optimizador no pueda mover,
cachear en un registro o eliminar.

La biblioteca ya se topó con el hueco y lo rodeó, documentándolo. `Atomics.ns:89`, escrito por
alguien de este proyecto:

> *«esta propiedad NO emite ninguna instruccion con prefijo `lock` ni ninguna barrera de
> memoria/fence … codigo que necesita el valor mas reciente con garantias de orden entre hilos debe
> usar Exchange/CompareExchange con el mismo valor como ambos argumentos»*

Un `CompareExchange` consigo mismo para leer un `int` es correcto y es caro. Y una bandera `bool`
compartida entre hilos, escrita en el lenguaje llano, **hoy se puede optimizar hasta desaparecer**.
Lo mismo un registro mapeado en memoria en código de interop.

### 6.2 El diseño propuesto, concreto

**Alcance deliberadamente pequeño.** No un modelo de memoria completo con `acquire`/`release`
seleccionables: eso es una superficie de API que este lenguaje todavía no necesita. Lo que hace
falta es **la garantía de que una lectura o escritura ocurre, una vez, en el orden escrito**.

1. **`volatile` como modificador de campo y de local.**
   `volatile int flag;` — sólo sobre tipos de tamaño ≤ 8 bytes con alineación natural. Un
   `volatile` sobre un struct grande se rechaza con mensaje propio: no hay carga atómica que
   ofrecer y prometerla sería peor que negarla.
2. **Se propaga por el `typeKey`**, no por una tabla lateral: `volatile:builtin:int`. Así lo
   arrastran las conversiones, el layout y el HIR sin que cada capa tenga que acordarse.
   *(Alternativa considerada y descartada: una bandera en el símbolo del campo. Se pierde en cuanto
   el valor pasa por un puntero, que es justo el caso de interop.)*
3. **En el backend**, `LLVMSetVolatile` sobre el `load`/`store` — un símbolo que **hay que barrer
   contra `LlvmCApi.h` antes de escribir una línea**, porque el árbol no incluye las cabeceras de
   LLVM y usar un símbolo no declarado falla en tiempo de compilación. Es la disciplina de CC1 y no
   es negociable.
4. **`volatile` en un puntero:** `volatile int* p` marca el acceso a través de `*p`, no al puntero.
   Es la regla de C y la que sirve para un registro mapeado.
5. **NO se toca `AtomicInt32.Value`** en esta fase. Cuando `volatile` exista, esa propiedad puede
   pasar a ser una lectura volátil y el comentario de `Atomics.ns` se reescribe. Antes no.

**Cómo se mide, y es la parte difícil.** Una fixture de ejecución **no puede** distinguir una
lectura volátil de una normal en un programa de un solo hilo: las dos dan el mismo número. Igual que
con EH-001, la prueba tiene que ser **en el IR**: un bucle `while (flag == 0) { }` sobre un campo
volátil debe emitir un `load volatile` **dentro** del bucle a `-O2`; sobre un campo normal, `-O2`
lo saca fuera y el bucle queda vacío. Esa diferencia es observable, es la propiedad, y es lo que hay
que afirmar.

---

## 7. CC2-D — El edge case de `catch` + `switch` + `continue`

**Medido: las fixtures existen y pasan.** `c4_catch_continue`, `c5_catch_break`, `c6_catch_return`,
`c7_finally_switch` y `c8_loop_catch_switch` ejecutan y devuelven 42 a `-O0` y `-O2`, y EH-001 está
afirmado en el IR con el conteo de bloques `eh.catch.leave.N`.

**Lo que falta no es la combinación, es la profundidad.** Ninguna fixture prueba un `switch` dentro
de un `catch` dentro de un `switch` dentro de un bucle, con `continue` saltando **dos** niveles de
`switch` desde dentro del `catch`. `structuredExitLeavesCatchBody` compara
`loopStack_.size() - 1` contra `loopDepthAtEntry`, y un `switch` no empuja en `loopStack_` — así que
la aritmética es correcta hoy **por una razón que ninguna prueba fija**. Tarea propia.

---

## 8. CC2-E — Hardening pendiente de `stackalloc`

Lo que valida hoy, medido en el código: tipo de elemento con tamaño (`void` rechazado), tipo de
elemento sin destructor y sin campos `owned`, y contador integral.

Lo que **no** valida:

1. **Contador negativo.** `stackalloc int[-1]` con una constante negativa. Con el ID 119 cerrado el
   frontend ya sabe que la constante es negativa; sólo falta rechazarla.
2. **Contador constante desmesurado.** No hay cota. `stackalloc byte[1000000000]` emite un `alloca`
   que revienta la pila en ejecución sin un solo diagnóstico.
3. **`stackalloc` dentro de un bucle.** Cada vuelta reserva otra vez y **nada se libera hasta salir
   de la función**: un `alloca` en un bucle es un desbordamiento de pila con forma de programa
   correcto. Es la trampa clásica de C y merece al menos un aviso.
4. **Desbordamiento del producto** `count * elementSize` en 64 bits.

Ninguno es exótico y los cuatro son baratos ahora que el tamaño lo da un solo servicio.

---

## 9. CC2-F — `++`/`--` y asignaciones compuestas

**Estado: CERRADO en esta sesión.** Documento propio: `79_ids_79_80_81_83.md`.

Lo que queda de ese frente, y es poco:

* **ID 82** — `propiedad += v` sigue deferido en la semántica; necesita secuenciar getter y setter y
  decidir el orden de evaluación del receptor.
* **`operator ++` de usuario** — el rechazo del parser se **mantiene**, con motivo reescrito: la
  semántica de actualización de lvalue ya existe, pero llamar a un operador de usuario y guardar su
  resultado evaluaría el operando dos veces, que es lo único que este lowering existe para evitar.
  Un rechazo con un motivo caducado es peor que un rechazo.

---

## 10. CC2-G — `sbyte`

**No hay entero de 8 bits con signo en el lenguaje.** `byte` existe y es sin signo. Una API de C con
`signed char` no se puede declarar honestamente hoy — y este lenguaje existe, entre otras cosas,
para declarar APIs de C.

Es mecánico y toca muchas capas a la vez, que es justo lo que lo hace arriesgado sin disciplina:
keyword y `TokenKind`, `isBuiltinTypeKeyword` (que es un **rango**, así que el enumerador va en su
sitio o rompe otros tipos), `constantFits`, `isSignedInteger`, `commonNumericType`, la tabla de
preferencia, `BuiltinLayoutKind`, `llvmType`, la ABI de argumentos y el `switch` de conversiones.

**El riesgo concreto, y cómo se controla:** las dos funciones `…NodeKindName` **no tienen
`default`**, y `isBuiltinTypeKeyword` compara por rango. Un enumerador mal colocado no da un error
de compilación, da un tipo que se comporta raro. La regla: **compilar con `-Wall -Wextra` antes de
entregar** — es lo que en CC1 atrapó un `-Wswitch` que habría roto el build con `/W4 /WX`.

---

## 11. CC2-H — UTF-8

**Medido:** `char` mide un byte en el lexer, en `constantFits`, en el layout, en `llvmType` y en la
ABI — CC1-100 lo unificó a propósito, cerrando un agujero por el que un `switch` sobre `char`
aceptaba `case 321:` y el backend emitía dos etiquetas idénticas, que es comportamiento indefinido
en LLVM. `String.ns` declara su alcance en sus propios comentarios: **ASCII**, sin tabla de folding
Unicode, `IsDecimal`/`IsNumeric` coincidiendo porque no hay categorías.

El lexer ya valida UTF-8 bien formado en los literales de cadena — rangos de continuación,
sobrecodificación, sustitutos — así que **el archivo fuente ya es UTF-8**. Lo que no existe es
`\u`/`\U` en los escapes, ni nada por encima del byte en la biblioteca.

**La decisión que CC2 tiene que tomar, y es una decisión, no una tarea:**

* **(a) Comprometerse con UTF-8 y dejar `char` en un byte.** `char` es una **unidad de código**, no
  un carácter; `String` es una secuencia de bytes UTF-8; se añaden `\u`/`\U` codificando a UTF-8 en
  cadenas y rechazándolos en literales de `char` por encima de 0x7F. Es coherente con el objetivo de
  interop con C, es barato, y es honesto siempre que la documentación diga «unidad de código».
* **(b) Pasar `char` a 16 bits, como C#.** Rompe la ABI de todo lo que ya cruza a C, mueve el layout
  de cada struct con un `char`, y obliga a mover la línea de `constantFits` **con** el layout, no
  antes — el propio código lo advierte.

**Recomendación: (a).** (b) es el modelo de C#, y este lenguaje no es C#: es un lenguaje nativo cuyo
contrato es el de C. La deuda de (a) es la documentación; la de (b) es la ABI.

---

## 12. CC2-I — Restricciones genéricas

`where` **se parsea y se rechaza** en tres sitios con el mismo mensaje: *«generic constraints are
deferred beyond Stage 44; monomorphization currently requires unconstrained type parameters»*.

**Por qué esto importa más de lo que parece:** sin restricciones, un `T` sólo puede usarse en lo que
vale para todos los tipos — asignar, pasar, devolver. No se le puede llamar un método, ni compararlo
con `<`, ni construirlo. Es la razón por la que `Comparer.ns` existe como clase separada en vez de
`where T : IComparable<T>`.

Y hay un segundo hueco que lo agrava: **las interfaces no toman parámetros de tipo**. Aunque hubiera
restricciones, no se podría escribir `where T : IComparable<T>` porque `IComparable<T>` no es
declarable.

**Orden obligado:** interfaces genéricas primero, restricciones después. Al revés no se puede.

**Lo que la monomorfización ya da a favor:** con el cuerpo especializado por tipo, una restricción
es un **chequeo**, no un mecanismo de despacho. No hace falta vtable ni diccionario de tipos: basta
comprobar, en el punto de especialización, que el argumento satisface la restricción y dejar que el
cuerpo clonado se ligue como cualquier código concreto. Eso hace esta fase mucho más barata de lo
que suena.

---

## 13. CC2-J — Arquitectura: preparar LIR / NSASM

Las cuatro tareas que CC1 difirió (P-001, P-002, A-001, A-002) **siguen diferidas y siguen siendo
correctas**. Lo que cambia en CC2 es que ahora hay una razón concreta para atacarlas: una
representación propia por debajo del MIR y por encima de LLVM.

**El hecho que lo hace urgente:** `src/LLVM/LlvmCApi.h` declara a mano **242 símbolos** de la API C
de LLVM. El árbol no incluye las cabeceras de LLVM. Cada lowering nuevo empieza por barrer los
símbolos que va a usar contra ese archivo — funciona, es disciplina real, y es también la medida
exacta de cuánto pesa la dependencia.

**Preparación propuesta, y sólo preparación:**

1. **A-002 primero, y sin escribir LIR todavía.** Hacer explícita la frontera: hoy
   `ProjectOopLlvmLowerer.cpp` mezcla decisiones de lenguaje (qué significa `>>` en un tipo sin
   signo) con emisión de LLVM. La extracción de `applyBinaryOperation` en esta sesión es
   exactamente la forma que hay que replicar: **una función por regla del lenguaje, que toma valores
   ya bajados**. Cuando haya suficientes, la frontera se ve sola y LIR se escribe por debajo de
   ellas.
2. **Medir la superficie real.** De los 242 símbolos, ¿cuántos usa de verdad el camino caliente?
   Esa cuenta es el tamaño del LIR mínimo, y hoy nadie la tiene.
3. **P-001 (retirar el mini-parser de type seeds) es independiente y se puede hacer sola.** Es un
   segundo parser heurístico cuya fragilidad la auditoría ya documentó.
4. **A-001 (frontend canónico único) es la más cara y la última.** No entra en CC2.

**Lo que NO se hace en CC2:** escribir NSASM. Un ensamblador propio antes de tener la frontera
medida es cómo se construye una segunda dependencia igual de enredada que la primera.

---

## 14. Testing, reproducibilidad y documentación

**Testing.** 242 tests en `ctest`, 67 casos en el checklist de Windows a `-O0` y `-O2`, 16/16 en
triage de bugs. Lo que falta es de forma, no de cantidad:

* **ID 77 (abierto)** — los logs no dicen **cuántas** comprobaciones corrió una suite, sólo que
  ninguna falló. Un bloque de tests que desapareciera de un `.cpp` pasaría en verde. Es barato y
  protege todo lo demás.
* **La prueba de una propiedad no observable va al IR.** Está establecido y funciona: EH-001 cuenta
  bloques `eh.catch.leave.N`; la evaluación única del destino cuenta llamadas a `Probe`. `volatile`
  (§6) se medirá igual, porque tampoco es observable de otra forma.
* **Un test negativo exige el mensaje, no el fallo.** Ya es regla, y en CC1 destapó que
  `cc1-sizeof-unevaluated` pasaba por la razón equivocada.

**Reproducibilidad.** `verify_manifest.py` tiene `--write`, `--clean` (que **archiva en `_sandbox/`**,
no borra) y `--audit`. El manifiesto **no cubre los `.bat`**; sigue anotado y sin cambiar, porque
ampliarlo es una decisión sobre qué significa el manifiesto.

**Documentación.** Cada ronda deja su `.md` y su `STAGE_*_CHANGED_FILES.txt`. La regla que gobierna
CC2 es la misma que gobernó CC1:

> **Ninguna verificación cuenta si el modelo contra el que verifico lo construí yo.**

En esta sesión esa regla se aplicó tres veces y salvó tres errores: el barrido de `case` que daba un
falso positivo con las lambdas, el grep de operadores que daba falsos negativos, y — el más caro —
la primera versión de la regla del ID 120, que era correcta en el caso que perseguía y **rompía
`(-1) + 1L`**. Lo vi compilando `ProjectTypeConversion.cpp` sola y llamándola, no leyéndola.

---

## 15. El registro de CC2

| ID | Área | Qué | Prioridad | Estado |
|---|---|---|---|---|
| **78** | Layout | `enum` medía 8 en un servicio de tamaño y 4 en el otro | **P1** | ✅ cerrado |
| **119** | Constantes | un literal negativo perdía su constancia | **P1** | ✅ cerrado |
| **120** | Constantes | el operando constante no se adaptaba al otro | **P1** | ✅ cerrado |
| **79/80/81/83** | Lenguaje | `++`/`--`, compuestos, `p->` | P1 | ✅ cerrado |
| **87** | Memoria | no hay `volatile` | **P1** | abierto |
| **121** | Constantes | dos constantes de anchos distintos dan `double`; el folding se rinde | **P1** | abierto |
| **122** | Constantes | `ProjectIntegerConstant` tiene dos convenciones | P2 | abierto |
| **88** | Lenguaje | no hay `checked`/`unchecked` | P2 | abierto |
| **89** | Tipos | no hay `sbyte` | P2 | abierto |
| **123** | Memoria | `stackalloc`: contador negativo, cota, bucle, desbordamiento | P2 | abierto |
| **124** | EH | falta la fixture de `switch` anidado dentro de `catch` con `continue` | P2 | abierto |
| **91** | Generics | las interfaces no toman parámetros de tipo | P2 | abierto |
| **90** | Generics | restricciones `where` | P2 | abierto |
| **77** | Testing | los logs no dicen cuántas comprobaciones corrieron | P2 | abierto |
| **125** | Unicode | decidir (a) o (b) y escribirlo; `\u`/`\U` si (a) | P2 | abierto |
| **82** | Lenguaje | `+=` sobre propiedad/indexador | P3 | abierto |
| **92** | OOP | tipos anidados | P3 | abierto |
| **93** | OOP | constructores estáticos | P3 | abierto |
| **A-002** | Arquitectura | frontera backend-neutral, por extracción | P3 | abierto |
| **P-001** | Arquitectura | retirar el mini-parser de type seeds | P3 | abierto |

---

## 16. Orden de ejecución

El orden no es por esfuerzo, es por **qué falla en silencio**.

**Fase 1 — lo que hace que un programa correcto mienta.**
`87` (`volatile`) sola. Es la única de todo el registro donde no hay síntoma: el programa compila,
corre, y da el resultado equivocado un día de cada mil. Va primero, y va sola.
*Terminado cuando:* el IR de un bucle `while (flag == 0) { }` a `-O2` contiene un `load volatile`
dentro del bucle, y el mismo bucle sobre un campo no volátil no lo contiene.

**Fase 2 — la aritmética de constantes, de una vez.**
`122` antes que `121`: no se puede escribir la regla de promoción entera mientras la estructura que
la alimenta tenga dos significados. Las dos juntas, en una fase.
*Terminado cuando:* `const long x = 1 + 1L;` se pliega; `commonNumericType` no devuelve `double`
para ningún par de enteros; y hay un test por cada par de anchos.

**Fase 3 — el hardening barato, todo junto.**
`123` (`stackalloc`), `124` (fixture de EH) y `77` (conteo en los logs). Ninguna es difícil, las tres
protegen lo que ya existe, y `77` protege a las otras dos de desaparecer sin ruido.
*Terminado cuando:* cuatro rechazos nuevos con su mensaje; una fixture de EH con dos `switch`
anidados; y cada suite imprime `N/N`.

**Fase 4 — el sistema de tipos.**
`89` (`sbyte`) y `88` (`checked`). `sbyte` primero: cierra un agujero de interop y ejercita las mismas
tablas que `checked` va a tocar.
*Terminado cuando:* una fixture declara y usa `sbyte` cruzando a C, y `checked` desborda con
diagnóstico donde `unchecked` envuelve.

**Fase 5 — generics, en el único orden posible.**
`91` (interfaces genéricas) y luego `90` (restricciones). Al revés no se puede: sin
`IComparable<T>` declarable no hay `where T : IComparable<T>` que escribir.
*Terminado cuando:* `Comparer.ns` se reescribe con una restricción y sigue pasando sus pruebas.

**Fase 6 — decisiones, no código.**
`125` (Unicode) escrito y decidido. `A-002` avanzando por extracción, una regla del lenguaje por
función, sin escribir LIR todavía.

**Fuera de CC2, dicho para que no vuelva a aparecer como pendiente tácito:** LINQ, patrones,
`async`/`await`, `record`, `dynamic` y las 33 palabras contextuales de esa familia **no son de este
proyecto**.

---

## 17. Criterios de terminado del milestone

CC2 está cerrado cuando, y sólo cuando:

1. **Cero P1 abiertos.** Hoy son dos: `87` y `121`.
2. **El checklist de Windows pasa entero**, a `-O0` y `-O2`, con los casos nuevos de cada fase.
3. **`ctest` al 100%**, y cada suite imprimiendo cuántas comprobaciones corrió (ID 77).
4. **Cero avisos** con `/W4` y `NSC_WARNINGS_AS_ERRORS=ON`.
5. **`python verify_manifest.py --write` seguido de una verificación en verde.**
6. **Cada propiedad no observable desde el lenguaje, medida en el IR** — `volatile` es el caso de
   esta ronda.
7. **Cada fase con su `.md` y su `STAGE_*_CHANGED_FILES.txt`**, y cada tarea que resulte no
   implementable como está escrita, **anotada con lo que se midió** en vez de cerrada en el papel.
   En CC1 fueron seis de ochenta; asumir que en CC2 serán cero es el error que este documento
   intenta no cometer.
