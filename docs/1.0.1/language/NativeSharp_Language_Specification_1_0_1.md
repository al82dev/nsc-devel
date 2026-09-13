# NativeSharp Language Specification 1.0.1

**Estado:** contrato de desarrollo vigente para 1.0.1
**Lenguaje implementado:** NativeSharp 1.0.1
**Compilador de desarrollo:** `nsc 1.0.1-dev`, Stage 67
**Target contractual:** `x86_64-pc-windows-msvc`

## 1. Cómo leer esta especificación

Este archivo define el significado que NativeSharp desarrolla para la línea 1.0.1. El libro
explica cómo programar; los documentos históricos explican por qué se eligió cada diseño; este
archivo decide qué debe aceptar, rechazar y producir una implementación conforme.

Las palabras **debe**, **no debe** y **se rechaza** son normativas. Un ejemplo no sustituye una
regla. Una prueba demuestra una implementación concreta, pero tampoco sustituye la regla. El mapa
`NativeSharp_1_0_1_conformance.map` conecta cada cláusula con sus lugares de implementación y con la
evidencia ejecutable que evita que la prosa y el compilador evolucionen por separado.

La versión visible del lenguaje es 1.0.1. Mientras esta edición no tenga un baseline certificado,
el compilador se identifica como `1.0.1-dev+build.<fecha>.<id>`; la carpeta documental fija la línea
que describe, no afirma por sí sola que una construcción concreta esté certificada.

## 2. Conformidad y programa válido

Un programa es válido cuando:

1. su texto forma tokens y sintaxis aceptados por `NativeSharp_Grammar_1_0_1.ebnf`;
2. nombres, tipos, accesibilidad y overloads tienen una resolución única;
3. todas las conversiones requeridas son implícitas válidas o casts explícitos válidos;
4. cada lectura satisface definite assignment;
5. ownership, préstamos, movimientos y escapes satisfacen las cláusulas de este documento;
6. su layout y sus fronteras binarias son representables en el target contractual;
7. no emplea una construcción que esta especificación declara excluida.

Un compilador conforme debe rechazar un programa inválido antes de emitir un producto utilizable.
No puede aceptar una forma y darle silenciosamente un significado distinto por nivel de
optimización. Los errores del usuario deben ser diagnósticos, no fallos internos ni IR inválido.

## 3. Texto, sintaxis, nombres y conversiones

### NS-LEX-001 — Contrato léxico

El fuente distingue mayúsculas de minúsculas. Espacios, saltos de línea y comentarios separan
tokens y no cambian el significado fuera de literales. `//` inicia un comentario hasta el final de
línea; `/*` y `*/` delimitan un comentario de bloque. Un comentario sin cerrar y un carácter que no
forme ningún token válido deben producir diagnóstico.

Los identificadores, palabras reservadas, literales y operadores se enumeran normativamente en
`NativeSharp_Grammar_1_0_1.ebnf`. `C`, `ref`, `out`, `where`, `from`, `pack`, `align` y `at` son
contextuales: sólo adquieren significado especial en la producción que los nombra. Los literales
enteros fuera del rango que su sufijo o contexto permita se rechazan; no se recortan durante lexing.

### NS-SYN-001 — Contrato sintáctico

`NativeSharp_Grammar_1_0_1.ebnf` es parte de esta especificación. Define la superficie de unidades de
compilación, tipos, declaraciones, miembros, statements y expresiones. Las restricciones
semánticas de este documento se aplican después de reconocer una producción. Una extensión del
parser que no aparezca en esa gramática es experimental y no forma parte del contrato 1.0 hasta
actualizar ambos archivos y su mapa de conformidad.

Los operadores respetan la precedencia escrita en la gramática. Asignación y `?:` asocian a la
derecha; los demás operadores binarios listados asocian a la izquierda. Paréntesis forman una
expresión primaria y sustituyen esa asociación.

### NS-NAME-001 — Nombres, ámbitos y accesibilidad

Un nombre simple se busca desde el ámbito léxico más interno hacia afuera, después en imports y
namespaces visibles. Un nombre calificado resuelve cada segmento desde la entidad anterior. Dos
candidatos igualmente válidos producen ambigüedad; el compilador no elige por orden de archivos.

Los miembros `private` sólo son accesibles desde su tipo; `protected`, desde el tipo o sus
derivados; `internal`, desde el proyecto que los declara; `public`, desde cualquier consumidor que
pueda ver el módulo. El acceso no puede ampliarse al materializar metadata precompilada. Tipos
anidados participan con su nombre canónico completo, no sólo con el nombre corto.

### NS-CONV-001 — Conversiones y selección de overload

Una conversión identidad es preferible a una conversión implícita. Una conversión implícita sólo
es válida si preserva el dominio definido por el tipo de origen o corresponde a una relación
nominal válida. Las conversiones numéricas que estrechan, reinterpretaciones de puntero y
conversiones declaradas `explicit` requieren cast escrito.

En una comparación de igualdad o desigualdad, el literal entero `0` puede convertirse
implícitamente a cualquier enum. Ningún otro literal entero recibe esa conversión y dos enums
nominalmente distintos siguen siendo incompatibles.

La selección de overload considera aridad, argumentos genéricos, constraints, modos
`ref`/`out`/`in`, ownership y calidad de conversión. Debe existir un mejor candidato único. Ningún
candidato o varios candidatos igualmente buenos son errores; el orden de declaración no desempata.

### NS-DIAG-001 — Rechazo de programas inválidos

Un programa que incumple léxico, gramática o semántica debe terminar con código de error y al menos
un diagnóstico que identifique causa y ubicación disponible. La recuperación puede continuar para
encontrar errores independientes, pero no puede materializar un producto utilizable ni convertir
el fallo original en una cascada sin relación.

### NS-PROJ-001 — Proyectos, namespaces y parciales

Un proyecto fija nombre, fuentes y referencias. Sus fuentes forman una sola unidad semántica; el
orden textual de los archivos no altera la resolución. Un `namespace` califica identidades pero no
crea almacenamiento ni un objeto en runtime. Una referencia forma un grafo acíclico y sólo expone
la superficie pública del productor.

Las partes de una clase `partial` con la misma identidad canónica se agregan antes de construir sus
miembros. Modificadores, bases y miembros incompatibles entre partes se rechazan. Dos declaraciones
no parciales con la misma identidad no se convierten accidentalmente en una clase parcial.

### NS-TYPE-002 — Categorías de tipos

Una clase es un tipo nominal por referencia con identidad, posible herencia y destrucción. Un
struct es un valor con layout compuesto por sus campos; copiarlo copia su valor conforme a las
reglas de ownership de sus hojas. Una interface declara una superficie de dispatch sin agregar
datos a la instancia. Un enum es un conjunto nominal de constantes representado por un entero. Un
delegate es un valor invocable con firma exacta y posible contexto.

El inicializador explícito de un miembro enum es una expresión constante integral. Puede combinar
literales y miembros anteriores del mismo enum mediante los operadores integrales admitidos; el
resultado debe caber en el tipo subyacente. Una llamada, una referencia dinámica o cualquier forma
que no pueda resolverse completamente durante la compilación se rechaza.

La igualdad de nombres cortos no iguala tipos. La identidad canónica incluye namespace, anidamiento,
módulo de origen y, cuando aplica, argumentos genéricos.

### NS-TYPE-003 — Structs readonly

`readonly struct` declara que el estado de instancia de un valor queda congelado después de su
construcción. Todo campo de instancia declarado por el struct debe llevar `readonly`; un campo
mutable hace inválida la declaración completa. Los campos estáticos no pertenecen al valor y se
rigen por sus propios modificadores.

El constructor del mismo struct puede asignar sus campos readonly mediante el receptor `this`
actual. Una vez terminada la construcción, una asignación directa o mediante miembro se rechaza.
Esta entrega no vuelve readonly el receptor de cada método automáticamente: ese contrato se define
separadamente para permitir diagnosticar mutaciones transitivas sin alterar el ABI del valor.

`readonly` como modificador de tipo sólo es válido en structs no-union, incluidos structs anidados.
No cambia tamaño, alineación, orden de campos ni convención de llamada. NSMETA conserva la propiedad
para que un consumidor reconstruya la misma restricción semántica.

`readonly struct` declares that instance state is frozen after construction. Every instance field
must be readonly, while static fields remain governed independently. The declaring constructor may
initialize the current receiver; later assignments are rejected. The modifier is valid only for
non-union structs, changes no ABI property, and is preserved in NSMETA.

### NS-CALL-002 — Receptores readonly

Un receptor de struct es readonly cuando procede de un parámetro `in`, de un campo readonly o de
`this` dentro de un miembro readonly. Todos los miembros de instancia de un `readonly struct`, salvo
su constructor durante inicialización, tienen receptor readonly implícito.

Un receptor readonly puede leer campos e invocar métodos, getters de properties o getters de
indexers que también tengan contrato readonly. No puede ser destino de una asignación, ni invocar
un método o accessor mutador. La restricción se propaga al acceder a un campo struct: una ubicación
alcanzada a través de almacenamiento readonly no recupera mutabilidad por encadenar otro miembro.

En un struct mutable se puede escribir `readonly` sobre un método de instancia, una property sin
setter o un indexer sin setter. El modificador es inválido en miembros estáticos, miembros de clase,
properties con setter e indexers con setter. La llamada conserva la dirección original del valor;
el compilador no crea una copia defensiva para hacer que una llamada mutadora parezca segura.

A struct receiver is readonly when it comes from an `in` parameter, a readonly field, or `this`
inside a readonly member. Reads and calls to readonly members remain valid; stores and calls to
mutating members are rejected. Readonly structs apply the receiver contract implicitly, and the
compiler does not manufacture defensive copies.

### NS-MEMBER-001 — Miembros y modificadores

Campos almacenan estado; métodos ejecutan cuerpos; propiedades e indexadores son pares de accessors;
operadores son métodos estáticos sujetos a las formas admitidas; constructores inicializan una
instancia; destructores descargan obligaciones del objeto. Un miembro `static` no recibe `this`.

Un campo `const` es estático e inmutable y su inicializador debe resolverse completamente durante
la compilación. Se admiten constantes integrales, `bool`, `float`, `double` y `string`; un literal
`string` o una referencia a otro `const string` se materializa como almacenamiento literal
inmutable y no ejecuta un inicializador estático en runtime.

Una auto-propiedad de instancia que declara únicamente `get;` puede recibir una asignación simple
dentro de un constructor del mismo tipo, por nombre implícito o mediante `this`. Esa escritura
inicializa su backing field generado; no sintetiza un setter y cualquier escritura desde otro
método, otro objeto o un tipo externo se rechaza.

`virtual` introduce o participa en dispatch; `override` debe encontrar una ranura compatible;
`abstract` no aporta cuerpo concreto; `sealed` impide la extensión que nombre. Una combinación de
modificadores sin significado se rechaza, no se ignora. Un accessor ausente no se sintetiza salvo
la forma automática que la gramática admite expresamente.

### NS-INHERIT-001 — Herencia, interfaces y dispatch

Una clase tiene como máximo una base de clase y puede implementar interfaces visibles. Los ciclos
de herencia son inválidos. Una conversión hacia una base o interface conserva la identidad dinámica
del objeto. Una llamada virtual usa la ranura congelada por el layout; una llamada no virtual o a un
miembro de struct se resuelve estáticamente.

Una implementación de interface debe aportar una firma compatible. El orden de fuentes o módulos
no puede cambiar las ranuras de un contrato ya publicado.

Las interfaces pueden declarar parámetros de tipo y cláusulas `where`. Una interfaz cerrada se
monomorfiza con identidad nominal, puede heredar otras interfaces cerradas y puede usarse como
constraint o como superficie de dispatch sin sustitución tardía en LLVM.

Interfaces may declare type parameters and `where` clauses. A closed interface is monomorphized
with nominal identity, may inherit other closed interfaces, and may be used as a constraint or a
dispatch surface without late LLVM substitution.

### NS-ARRAY-001 — Arrays nativos

Un array dinámico contiene elementos contiguos y su almacenamiento se crea explícitamente con
`new T[n]` o un inicializador inferido. `delete[]` descarga el buffer cuando el programa posee esa
obligación. `raii T[]` liga esa descarga al ámbito. La propiedad del buffer y la propiedad de cada
elemento se analizan por separado; mover un elemento no mueve el buffer.

Un fixed array forma parte del valor que lo contiene y su cantidad es contractual. No puede
degradarse a un puntero para cambiar layout o lifetime silenciosamente. La forma
`T[d0, d1, ...]` declara dos o más dimensiones fijas; cada dimensión debe ser una expresión
constante entera positiva.

### NS-CALL-001 — Paso de argumentos

Un parámetro ordinario recibe un valor. `ref` recibe un slot legible y escribible; `out` recibe un
slot que el callee debe asignar antes de retornar normalmente; `in` recibe acceso de sólo lectura.
El modo forma parte de la firma y debe coincidir en la llamada. Reenviar un `out` conserva su estado
de definite assignment cuando el callee satisface el contrato.

Ownership es una dimensión adicional: `ref`, `out` e `in` no conceden por sí solos propiedad ni
autorizan conservar un préstamo.

### NS-DELEGATE-001 — Delegates, closures y eventos

Un delegate contiene una firma, un destino invocable y, cuando corresponde, contexto. La firma
incluye tipos, modos de parámetro, ownership y calling convention. Una lambda sin captura puede
materializarse sin contexto; una lambda con captura posee o presta su entorno conforme al análisis
de cada captura.

Una lambda es *target-typed*: debe existir un delegate destino. Sus parámetros pueden omitir el
tipo, en cuyo caso lo reciben de ese delegate, o escribirlo expresamente. Un tipo o modo
`ref`/`out`/`in` escrito debe coincidir exactamente con la firma destino. El tipo explícito participa
en la selección de overload. Como una lambda sólo puede convertirse a un delegate, los overloads
cuyo parámetro correspondiente no es delegate no impiden contextualizarla. Una lambda sin tipos
recibe contexto únicamente cuando queda un solo tipo delegate destino; no resuelve por sí sola dos
tipos delegate distintos igualmente aplicables.

Las variables ordinarias capturadas se representan mediante celdas compartidas. El método creador,
las lambdas hermanas y las lambdas anidadas observan el mismo slot: una escritura realizada por
cualquiera es visible para las demás. Una lambda anidada transporta a través de su padre las celdas
de parámetros, locales o capturas transitivas que necesita. La celda conserva el tamaño y la
alineación reales del valor, incluidos structs grandes y fixed arrays; no existe un límite de una
palabra de máquina.

El entorno posee referencias contadas a esas celdas. Destruir el delegate libera su referencia y
la activación creadora libera la suya tanto al retornar normalmente como al salir por excepción.
Una captura no convierte silenciosamente ownership: owners `raii`, valores `borrowed`, vistas y
parámetros `owned` no se pueden capturar mientras no exista una transferencia explícita válida.

Un `extern C delegate` no tiene canal de entorno. Por ello admite una lambda sin captura, que se
materializa como function pointer protegido por la frontera de excepciones C, y rechaza de forma
diagnosticada cualquier lambda con captura.

Un evento controla la lista de invocación mediante sus operaciones admitidas; no cambia el lifetime
de un delegate de manera implícita. Copiar, almacenar o transferir un delegate debe respetar la
propiedad publicada por su tipo y por la operación.

### NS-PTR-001 — Punteros y reinterpretación

Los punteros son valores nativos del ancho del target. Aritmética, dereference, `->`, `stackalloc`
y casts de puntero son operaciones explícitas cuya validez depende del tipo y del almacenamiento
apuntado. Un cast puede reinterpretar bits o firma sólo en las formas admitidas; no prolonga el
lifetime ni crea ownership.

Una conversión entre entero nativo, `void*` y function pointer requiere una forma explícita cuando
la firma o el tipo se pierde. El compilador garantiza representación y ABI, no que una dirección
inventada por el programa sea válida.

## 4. Target y representación

### NS-TARGET-001 — Target soportado

La conformidad 1.0 se define para Windows x64: triple LLVM `x86_64-pc-windows-msvc`, ABI de
Microsoft, objetos COFF y ejecutables PE32+. Un comportamiento dependiente del target sólo puede
considerarse normativo si aparece expresamente en esta sección o en una cláusula de layout.

### NS-TYPE-001 — Representación de tipos primitivos

El target es little-endian. `bool`, `sbyte`, `byte` y `char` ocupan 1 byte; `short` y `ushort`, 2;
`int`, `uint` y `float`, 4; `long`, `ulong`, `nint`, `nuint`, `double` y punteros, 8. `char` es un
entero sin signo de 8 bits; no representa por sí solo un punto Unicode. Los enums usan por defecto
4 bytes, salvo un tipo subyacente explícito válido.

`sizeof`, offsets, alineación, paso de arreglos y headers generados deben observar esas medidas. Un
backend no puede escoger el tamaño natural del host que está ejecutando el compilador.

## 5. Números y conversiones

### NS-NUM-001 — Overflow entero

El overflow entero con y sin signo envuelve módulo 2^N, donde N es el ancho del tipo de resultado.
Para tipos con signo, los bits se interpretan en complemento a dos. Es comportamiento definido. El
IR no debe añadir `nsw` ni `nuw` a una operación cuya semántica pública permita wrap.

Las promociones se deciden antes de operar. Dos operandos estrechos promovidos a `int` desbordan,
si lo hacen, en el ancho de `int`, no en el ancho original.

La misma regla gobierna evaluación constante, inicializadores `const`, miembros enum, etiquetas de
`switch` y ejecución. En particular, `MinValue / -1` produce `MinValue` y `MinValue % -1` produce
cero también durante compilación; una etapa no puede rechazar una expresión que otra materializa.
Las dependencias acíclicas entre constantes no tienen un límite semántico de longitud. Los ciclos se
rechazan por identidad de miembro, no mediante un corte que también descarte cadenas válidas.

### NS-NUM-002 — Conversiones enteras explícitas

Un cast entero explícito normaliza el valor al ancho de destino: conserva los N bits bajos y los
interpreta con el signo del destino. Nunca satura ni lanza. Una conversión implícita que pudiera
perder rango debe rechazarse; el truncamiento requiere que el programador escriba el cast.

### NS-NUM-003 — División y residuo enteros

Dividir por cero produce un trap explícito tanto en O0 como en O2. No entrega un valor y no se
convierte en una excepción capturable del lenguaje. `MinValue / -1` devuelve `MinValue`, coherente
con el wrap definido. El backend debe corregir el comportamiento de la instrucción física cuando
ésta atraparía o produciría poison.

El residuo sigue el cociente truncado hacia cero. El plegado constante y la ejecución deben aplicar
las mismas reglas.

### NS-NUM-004 — Desplazamientos

El operando izquierdo determina el ancho del resultado. El contador debe ser un entero válido y el
frontend debe rechazar un contador constante negativo o mayor o igual al ancho del valor. Para LLVM,
el backend debe convertir el operando derecho al ancho exigido por el izquierdo antes de emitir
`shl`, `lshr` o `ashr`. El desplazamiento derecho de un tipo con signo es aritmético; el de un tipo
sin signo es lógico.

### NS-FLOAT-001 — Punto flotante

`float` y `double` siguen IEEE 754. La compilación ordinaria no habilita fast-math, no reasocia
operaciones, no reemplaza automáticamente división por recíproco y no contrae operaciones a FMA.
NaN, infinitos y cero con signo conservan la semántica de IEEE 754. O0 y O2 deben producir el mismo
comportamiento observable salvo las variaciones que el propio estándar permite.

## 6. Evaluación e inicialización

### NS-EVAL-001 — Evaluación condicional

`&&` y `||` cortocircuitan. En `condición ? verdadero : falso` se evalúa la condición y exactamente
una rama. Una implementación no puede evaluar ambas ramas y seleccionar después. Las expresiones
condicionales se asocian a la derecha.

Cuando una rama tiene tipo `string-literal` y la otra `builtin:string`, el resultado tiene tipo
`builtin:string` y la rama literal usa la conversión implícita de literal a cadena nativa. Esta
regla no introduce una búsqueda general de tipo base común entre referencias.

Las llamadas evalúan receptor y argumentos una sola vez. Cuando una construcción promete orden de
izquierda a derecha, los efectos observables deben conservar ese orden en O0 y O2.

### NS-EVAL-002 — Inicializadores de arreglos

`new T[] { e0, e1, ... }` infiere la longitud del número de elementos. Cada expresión se evalúa una
sola vez, de izquierda a derecha, y se almacena en el índice correspondiente. Si una conversión,
construcción o transferencia falla semánticamente, el inicializador completo se rechaza.

### NS-INIT-001 — Locales y definite assignment

El almacenamiento local nace con bits cero para impedir lecturas de pila indeterminada. Además, un
escalar, puntero, referencia o delegate debe haber sido asignado en todos los caminos antes de ser
leído; el frontend rechaza lo contrario. La inicialización física a cero no vuelve válida una
lectura prohibida por definite assignment.

Los agregados pueden inicializarse parcialmente. Sus partes no escritas conservan cero. En Release,
el optimizador puede eliminar stores de cero que demuestre no observables.

### NS-INIT-002 — Inicialización estática

Los inicializadores estáticos de un tipo se ejecutan como máximo una vez antes de su primer uso que
los requiera. La publicación del estado terminado debe ser visible para los demás hilos. Un segundo
hilo espera al inicializador activo; una reentrada desde el mismo hilo no se bloquea esperando a sí
misma. El guard de inicialización forma parte de la corrección, no del layout público del tipo.

## 7. Ownership, préstamos y destrucción

### NS-OWN-001 — Calificadores y slots propietarios

`owned T` expresa una obligación de destrucción transferible. `borrowed T` expresa observación sin
adquirir esa obligación. `raii T` crea un dueño local cuyo cleanup pertenece al ámbito. Un campo
`owned` pertenece al objeto que lo contiene y se descarga cuando dicho objeto se destruye.

Una asignación a un slot propietario no es una copia implícita. Debe recibir un dueño fresco o una
transferencia explícita válida. Sobrescribir un owner destruye exactamente una vez el valor anterior
después de evaluar de forma segura el nuevo valor.

### NS-OWN-002 — Movimiento

`move(x)` transfiere la obligación desde un local `raii` o un parámetro `owned`. Después del
movimiento, el origen queda consumido y no puede leerse, prestarse ni moverse otra vez hasta recibir
un nuevo valor válido. El destino pasa a ser el único dueño.

Retornar un owner local o reenviar un parámetro propietario requiere movimiento explícito, salvo
que la expresión produzca directamente un owner fresco cuyo contrato de retorno ya sea `owned`.

### NS-OWN-003 — Préstamos y escape

Un préstamo nunca puede sobrevivir a su raíz. No puede retornarse una vista enraizada en un local,
guardarse en almacenamiento más longevo ni capturarse en un closure que pueda escapar. Un préstamo
temporal producido dentro de una expresión completa permanece válido hasta terminar esa expresión
y se limpia después, también por la ruta excepcional.

La regla de expresión completa se aplica igual a argumentos de métodos y constructores. Un literal
de cadena puede prestarse directamente porque su almacenamiento tiene duración de proceso; un
resultado `owned string` fresco recibe un dueño temporal oculto hasta que termina la llamada o la
construcción.

Los `borrowed struct` son valores vista: copiar la vista no copia ni prolonga el almacenamiento que
observa. Arrays o agregados que transportan vistas conservan esa restricción transitivamente.

### NS-OWN-004 — Estado estático propietario excluido

Un campo estático no puede declararse `owned`. El runtime no ejecuta finalización de módulo y, por
tanto, no existe un punto honesto donde descargar automáticamente esa obligación. El frontend debe
rechazar tanto el campo directo como el backing field de una propiedad estática propietaria. El
estado global que administre recursos debe usar un protocolo explícito y no fingir ownership
automático.

### NS-CLEAN-001 — Cleanup determinista

Los destructores de locales `raii`, parámetros `owned`, campos propietarios y regiones `using` se
ejecutan exactamente una vez al abandonar su ámbito por caída normal, `return`, `break`, `continue`
o excepción. Dentro de un ámbito se destruye en orden inverso a la adquisición. Un movimiento
anula la obligación del origen antes de que su cleanup pueda observarla.

## 8. Excepciones

### NS-EH-001 — Excepciones nativas tipadas

`throw` crea una salida excepcional; `catch (T x)` sólo captura un objeto compatible con `T`;
`finally` se ejecuta en salida normal y excepcional. La selección de handler no puede omitir
cleanups exteriores. `rethrow` conserva la excepción activa. Salir estructuradamente de un `catch`
limpia su estado antes de continuar.

La implementación contractual usa EH nativo de Windows x64. Las excepciones no son códigos de
retorno escondidos ni requieren un recolector.

### NS-EH-002 — Frontera de callback C

Una excepción NativeSharp no puede atravesar una función exportada con calling convention C ni un
callback C. El compilador debe rechazar una frontera que no garantice contención o generar el
adaptador definido que detenga la propagación. Permitir que el unwinder cruce código extranjero sin
contrato es inválido.

## 9. Modelo de memoria y concurrencia

### NS-MEM-001 — Accesos ordinarios

Una lectura o escritura ordinaria sólo promete comportamiento secuencial dentro del hilo. No
publica datos ni sincroniza con otro hilo. Dos hilos que acceden a la misma posición sin
sincronización, con al menos una escritura, constituyen un error del programa. El compilador no usa
la existencia de esa carrera como permiso para transformar otras rutas en resultados arbitrarios.

### NS-MEM-002 — Operaciones atómicas

Las operaciones públicas `Load`, `Store`, `Increment`, `Decrement`, `Add`, `Exchange` y
`CompareExchange` de `AtomicInt32` y `AtomicInt64` son atómicas y secuencialmente consistentes.
Incremento, decremento y suma devuelven el valor nuevo; intercambio y comparación-intercambio
devuelven el valor anterior. La implementación debe emitir operaciones LLVM `seq_cst`; una llamada
ordinaria con nombre parecido no satisface esta cláusula. La propiedad `Value` es una observación
ordinaria sin publicación y no sustituye a `Load` en un protocolo entre hilos.

No existe selección pública de ordenamiento por operación en 1.0. Tampoco existen fences públicos.

### NS-MEM-003 — Sincronización y publicación

Liberar un mutex, monitor, semáforo o evento publica las escrituras protegidas; adquirirlo permite
observarlas. `Thread.Join` establece que, después de retornar, el hilo que une puede observar las
escrituras anteriores del hilo unido. Estas garantías no se extienden a accesos que el programa
deja fuera del protocolo de sincronización.

## 10. Layout y ABI

### NS-LAYOUT-001 — Agregados

Un struct coloca cada campo en el siguiente offset que satisfaga su alineación efectiva y redondea
el tamaño final a la alineación del agregado. `pack=N` limita la alineación efectiva; `align=N`
eleva la alineación del agregado; un offset explícito fija la posición y permite overlap. En una
union, todos los campos no explícitos comienzan en cero y el tamaño cubre el miembro de mayor
extensión. Los valores de `pack` y `align` deben ser potencias de dos válidas.

### NS-LAYOUT-003 — Alineación explícita por campo

`align(N)` aplicado a un campo de instancia establece una alineación mínima para ese campo. La
alineación natural se limita primero mediante `pack(N)` y después se eleva, si es necesario, hasta
`N`; por tanto, una alineación explícita no puede debilitar el requisito natural y puede superar el
pack del contenedor. El offset automático se redondea a esa alineación, que también participa en la
alineación y el padding final del agregado u objeto.

`N` debe ser una potencia de dos positiva. Un campo estático no pertenece al layout de una instancia
y no admite este modificador. Si el mismo campo declara `at(offset)`, el offset debe ser múltiplo de
su alineación efectiva. NSMETA, el header C y el backend deben conservar el mismo valor; no es válido
reconstruir un consumidor con una alineación menor aunque los offsets coincidan accidentalmente.

Un fixed array ocupa `stride(elemento) * cantidad`. Su layout forma parte del agregado; no es una
referencia ni lleva cabecera oculta. Un arreglo `T[d0, d1, ...]` es contiguo y row-major: su
representación canónica equivale a un arreglo exterior de `d0` elementos, cada uno de los cuales
contiene la dimensión siguiente. Su tamaño es `sizeof(T) * d0 * d1 * ...`, comprobado contra
overflow en cada nivel. La primera dimensión escrita es la exterior y la última es la contigua.

La expresión `array[i0, i1, ...]` consume una dimensión fija por índice, de izquierda a derecha.
Cada índice debe ser integral y se escala por el tamaño de la dimensión que queda debajo: en
`T[rows, columns]`, `array[row, column]` desplaza primero `row * sizeof(T[columns])` y después
`column * sizeof(T)`. La forma es equivalente en dirección y resultado a `array[row][column]`,
pero conserva la declaración de rango en una sola expresión. Un índice constante fuera de la
extensión correspondiente se rechaza durante compilación; los accesos dinámicos pertenecen al
nivel raw y no insertan una comprobación de rango en ejecución.

### NS-ARRAY-003 — Arreglos locales de longitud variable

Una declaración local `T[length] values;`, donde `length` no es una constante de compilación,
evalúa `length` al alcanzar la declaración y reserva `length * sizeof(T)` bytes contiguos en el
frame de la función. La reserva no tiene cabecera, no usa el heap y no requiere `delete`; deja de
existir al retornar la función. Los accesos `values[index]` usan el stride canónico de `T` y
pertenecen al mismo nivel raw que el indexado de punteros.

La extensión debe tener tipo integral y el elemento debe tener tamaño y lifetime trivial: no puede
ser `void`, declarar destructor ni contener ownership que la desaparición cruda del frame dejaría
sin descargar. La declaración no admite inicializador agregado; sus elementos se inicializan de
forma explícita. Esta sintaxis se permite sólo en variables locales. Campos, parámetros y retornos
siguen siendo contratos de layout/ABI y no admiten una extensión calculada en ejecución.

Una extensión constante conserva la semántica distinta del fixed array inline. Por tanto,
`int[4] fixedValues;` es un valor fijo de 16 bytes, mientras `int[length] workingValues;` es una
reserva realizada al ejecutar esa declaración. Ninguna de las dos formas crea un array administrado.

### NS-ARRAY-004 — VLAs multidimensionales

Una declaración local puede combinar dos o más extensiones, constantes o de ejecución, siempre que
al menos una se evalúe en ejecución: `T[rows, columns] matrix;`. Las extensiones se evalúan una sola
vez, de izquierda a derecha, al alcanzar la declaración. El compilador conserva esos valores; una
asignación posterior a `rows` o `columns` no cambia el tamaño ni los strides de `matrix`.

El almacenamiento es una sola reserva contigua row-major de
`rows * columns * sizeof(T)` bytes. `matrix[row, column]` linealiza la posición como
`row * capturedColumns + column`; para más dimensiones aplica
`(((i0 * d1) + i1) * d2 + i2) ...`. La cantidad de índices debe coincidir con el rank y cada índice
debe ser integral. No se crea un arreglo de punteros ni una reserva independiente por fila.

### NS-ARRAY-005 — Seguridad y duración de VLAs

Cada extensión VLA se evalúa exactamente una vez. Una extensión de tipo integral con signo debe ser
mayor o igual que cero antes de convertirse al índice nativo sin signo. El compilador comprueba cada
factor antes de multiplicarlo: ni el total de elementos ni el tamaño en bytes pueden desbordarse.
Una declaración VLA puede reservar como máximo 1 MiB de stack; una dimensión negativa, un producto
excesivo o un tamaño superior a ese límite terminan mediante la trampa
`AllocationSizeOverflow`. Una extensión cero es válida y produce una reserva de cero elementos.

El almacenamiento pertenece exclusivamente al frame que ejecuta la declaración. El valor del VLA
no puede reasignarse como si fuera un puntero ordinario. Un puntero derivado puede usarse como alias
local y pasarse de forma prestada a una llamada, pero no puede retornarse, almacenarse en un campo ni
capturarse por una lambda. La procedencia se conserva al copiar el puntero entre aliases locales; la
API receptora de un préstamo sigue siendo responsable de no retenerlo después de la llamada.

### NS-ARRAY-006 — Almacenamiento trailing/flexible

`T[*] tail;` declara un arreglo inline de extensión incompleta. La forma se permite únicamente como
último campo de instancia de un struct que no sea union. El campo se coloca con la alineación de
`T`, pero contribuye cero elementos al tamaño: `sizeof(Header)` termina en el inicio alineado de la
cola y `offsetof(Header, tail)` devuelve ese mismo comienzo. El propietario debe reservar de forma
explícita y con overflow comprobado `sizeof(Header) + count * sizeof(T)` bytes.

El indexado `header->tail[index]` pertenece al nivel raw: no existe longitud oculta ni bounds check.
La cantidad vive en otro campo o contrato elegido por la API. `T` debe tener lifetime trivial y el
campo no admite ownership, porque el compilador desconoce cuántos elementos tendría que destruir.
La forma no puede aparecer en clases, unions, locales, parámetros o retornos. NSMETA conserva `T[*]`
y un header C lo materializa como el flexible array member `T tail[]`.

### NS-LAYOUT-002 — Objetos y delegates

El layout público de clases, descriptores, tablas de dispatch, interfaces y delegates está
versionado por Native ABI y Runtime ABI. Añadir raíz `Object` no autoriza boxing ni campos ocultos
por instancia que cambien un contrato congelado. La celda delegate actual ocupa el tamaño publicado
en NSMETA y cualquier discrepancia debe rechazarse antes del enlace.

### NS-ABI-001 — Calling conventions

Las funciones NativeSharp usan la convención nativa publicada por su metadata. `extern C` usa la
frontera C del target y debe tener una representación que un header C pueda expresar. El paso y
retorno de agregados sigue Microsoft x64, incluido retorno indirecto cuando corresponde. Bool,
enums, punteros, callbacks y ownership de handles deben conservar su contrato en ambos lados.

### NS-ABI-002 — Versionado binario

Una biblioteca publica identidad de módulo, target, versión NSMETA, Native ABI, Runtime ABI, huella
y superficie de símbolos. Un consumidor debe validar esos valores antes de materializar tipos o
enlazar. Formatos históricos sólo se aceptan conforme a una regla explícita de minor compatible;
un major futuro o un contrato físico distinto se rechaza con causa precisa.

## 11. Tipos, genéricos y control estructurado

### NS-CT-001 — Aserciones estáticas

`static_assert(condición);` y `static_assert(condición, "mensaje");` son statements de tiempo de
compilación válidos dentro de un bloque. La condición debe tener tipo `bool`, debe poder evaluarse
por completo durante la compilación y debe resultar verdadera. Una condición falsa rechaza el
programa e incorpora el literal opcional al diagnóstico; una condición dependiente de estado de
ejecución también se rechaza.

Las consultas `sizeof` y `alignof` dentro de una aserción se resuelven contra el modelo canónico de
layout del target. La aserción no se convierte en HIR ejecutable, MIR ni código LLVM: no tiene costo
ni efectos en tiempo de ejecución. Sus operandos se analizan para nombres y tipos, pero nunca se
ejecutan. En comparaciones con `sizeof`/`alignof`, el literal debe respetar las conversiones
numéricas ordinarias; por ejemplo, `sizeof(int) == 4u` compara `nuint` con un valor sin signo.

### NS-CT-002 — Desplazamiento físico de campos

`offsetof(T, campo)` es una expresión constante de tipo `nuint`. `T` debe ser un struct o union
completo y `campo` debe nombrar uno de sus campos de instancia directos. El resultado es el
desplazamiento en bytes desde el inicio del valor `T` hasta el campo seleccionado.

La operación consulta `ProjectObjectLayoutModel`; no recalcula padding, `pack`, alineación mínima
ni offsets explícitos. Por ello coincide por construcción con `sizeof`, el stride de arreglos, el
acceso a campos emitido por LLVM, NSMETA y los headers C. No acepta clases: sus referencias y su
cabecera de objeto pertenecen al ABI OOP, no al contrato de value layout que expresa `offsetof`.

En una especialización genérica cerrada se mide el layout de la especialización concreta. Un campo
inexistente, estático o perteneciente a un tipo que no sea struct/union es un error de compilación.

### NS-ASM-001 — Frontera gramatical de ensamblador integrado

`asm { ... }` es una sentencia de bloque válida exclusivamente dentro del cuerpo de una función o
método. No requiere `;` posterior. Sus braces externos delimitan la sentencia; braces internos
deben estar balanceados. En la entrega 287 `asm` es contextual: fuera de la forma `asm {` conserva
su validez como identificador ordinario.

El lexer entra en modo assembly únicamente después de la pareja contextual `asm {` y sale en su
brace externo balanceado. Dentro conserva los bytes de cada línea, incluidos espacios, comentarios
y puntuación que no son tokens NativeSharp, además del spelling físico de `CRLF`, `LF` o `CR`. El
nodo padre conserva el cuerpo exacto; cada línea no vacía produce `InlineAssemblyLine`, cuyo rango
empieza en el primer byte significativo y termina después del último. Mientras no estén presentes
los contratos posteriores de la tanda 4, el análisis semántico debe rechazar el nodo con
`InlineAssemblyNotAvailable`; nunca debe llegar a HIR como nodo desconocido ni aceptarse como no-op.

`asm { ... }` is a block statement valid only inside a function or method body and requires no
trailing semicolon. In delivery 287 the spelling is contextual. The AST retains the complete
statement range and exact physical body bytes. Every non-blank line has an AST record beginning at
its first significant source byte. Until the later batch-4 operand and lowering contracts are implemented, semantic analysis must reject it with
`InlineAssemblyNotAvailable`, never as an unknown internal HIR node or a silent no-op.

### NS-ASM-003 — Operandos posicionales de entrada

Un bloque puede terminar en `in(expresión0, expresión1, ...);`. La cláusula debe contener al menos
una expresión y, cuando existe, el semicolon final es obligatorio. Cada expresión se analiza con
las reglas ordinarias de scope, nombres y tipos y se evalúa una sola vez, de izquierda a derecha.
Su ordinal estable empieza en cero y se referencia dentro del texto assembly como `$0`, `$1`, etc.

La ausencia de una cláusula conserva la forma `asm { ... }` sin semicolon. En esta entrega los
operandos no tienen todavía constraint escrito y no llegan a LLVM; el diagnóstico temporal
`InlineAssemblyNotAvailable` continúa después de analizar todas las expresiones para que un nombre
inválido no quede oculto por la frontera incremental.

An assembly block may end in `in(expression0, expression1, ...);`. The non-empty clause and its
trailing semicolon are required together. Expressions use ordinary binding, are evaluated once in
left-to-right order, and receive stable zero-based `$0`, `$1`, ... positions. Constraints and LLVM
lowering remain later batch-4 contracts.

### NS-ASM-004 — Operandos posicionales de salida

La cláusula opcional `out(destino0, destino1, ...)` aparece después del bloque y antes de `in`.
Debe ser no vacía y cada destino debe ser un lvalue modificable conforme a las reglas ordinarias de
asignación. El análisis la considera una escritura: un identificador local completo queda
definitivamente asignado sin exigir que tuviera un valor anterior. Los ordinales son independientes
de las entradas y se escriben `$out0`, `$out1`, etc.; agregar un output no renumera `$0` de entrada.

An optional non-empty `out(target0, target1, ...)` clause follows the block and precedes `in`.
Every target must be an ordinary modifiable lvalue and is analyzed as a write, so a whole local
becomes definitely assigned without a prior read. Output placeholders use their own `$outN`
ordinal space and never renumber input `$N` placeholders.

### NS-ASM-005 — Restricciones de registros

Cada operando puede terminar en `: "constraint"`. Las entradas admiten `"r"` para cualquier
registro general o `"{nombre}"` para fijar uno; las salidas admiten `"=r"` y `"={nombre}"` como
escritura, además de `"+r"` y `"+{nombre}"` para lectura-escritura. Omitir la anotación conserva
compatibilidad y normaliza a `"r"` en inputs o `"=r"` en outputs. El AST guarda siempre la forma
normalizada y distingue si fue explícita. Un constraint debe ser un literal string válido, debe
respetar la dirección del operando y un nombre entre braces sólo puede contener letras, dígitos o
underscore. La disponibilidad del registro en la arquitectura y CPU destino se valida en
NS-ASM-011; esta regla sólo fija el contrato independiente del target.

Each operand may end in `: "constraint"`. Inputs accept `"r"` or a fixed `"{name}"`; outputs
accept write-only `"=r"`/`"={name}"` and read-write `"+r"`/`"+{name}"`. Omitted annotations
normalize to `"r"` for inputs and `"=r"` for outputs. The AST always stores that normalized form
and records whether it was explicit. Constraints must be string literals with a direction valid
for the operand. Target register availability remains the later NS-ASM-011 contract.

### NS-ASM-006 — Inmediatos y operandos de memoria

Una entrada `: "i"` exige una expresión integral reducible a constante en compilación. Una entrada
`: "m"` exige almacenamiento direccionable: local, parámetro, campo, elemento raw o dereference;
un literal, cálculo temporal, llamada o property no representa memoria. Las salidas `"=m"` y
`"+m"` exigen además el lvalue modificable ya requerido por todo output. `=m` es escritura y `+m`
lectura-escritura. El contrato de memoria no convierte una property con setter en una dirección ni
materializa un temporal oculto.

An `: "i"` input must be an integral compile-time constant. An `: "m"` input must denote
addressable storage: a local, parameter, field, raw element, or dereference. Literals, computed
temporaries, calls, and properties do not denote memory. `"=m"` and `"+m"` outputs additionally
retain the ordinary modifiable-lvalue requirement. The compiler never synthesizes hidden storage
to make a non-addressable expression satisfy `m`.

### NS-ASM-007 — Clobbers de registros

La cláusula opcional no vacía `clobber("registro", ...)` aparece después de outputs e inputs y
antes del semicolon final. Cada literal nombra un registro destruido por el bloque. Los nombres se
normalizan a minúsculas, conservan su orden fuente y no pueden repetirse. Un registro fijado por un
constraint `{registro}` no puede aparecer también como clobber. En esta etapa se exige un átomo de
registro bien formado; la existencia en x64 se certifica en NS-ASM-011. Los nombres especiales
`cc` y `memory` pertenecen a NS-ASM-008.

The optional non-empty `clobber("register", ...)` clause follows outputs and inputs before the
final semicolon. Names are lowercased, retain source order, and may not repeat. A register fixed by
an operand constraint may not also be clobbered. This rule validates register atoms; target
availability remains NS-ASM-011, while `cc` and `memory` belong to NS-ASM-008.

### NS-ASM-008 — Clobbers de flags y memoria

Los nombres reservados `"cc"` y `"memory"` usan la misma cláusula `clobber`. `cc` declara que el
bloque destruye condition codes y `memory` declara efectos sobre memoria no descritos por los
operandos `m`. Ambos son identidades únicas, case-insensitive y participan en la prohibición de
duplicados. El AST los clasifica como `flags` y `memory`, separados de `register`, para que el
lowering no dependa de comparar strings ni confunda efectos globales con registros físicos.

Reserved names `"cc"` and `"memory"` use the same clobber clause. `cc` declares condition-code
effects and `memory` declares effects on storage not named by `m` operands. Both are unique,
case-insensitive identities. The AST classifies them separately as `flags` and `memory`, while
physical names remain `register`, so later lowering does not infer semantics from raw spelling.

### NS-GEN-001 — Monomorfización

Los genéricos se especializan estáticamente para argumentos concretos. No existe despacho genérico
dinámico. La identidad de una especialización incluye la identidad canónica de sus argumentos y se
conserva entre productor y consumidor. Constraints se comprueban en compilación y una
especialización inválida se rechaza antes de emitir código.

### NS-GEN-002 — Aliases genéricos transparentes

Una directiva de alias de tipo puede declarar parámetros mediante
`using Nombre<T0, ...> = Tipo;`. Cada uso debe proporcionar exactamente la aridad declarada. Los
parámetros pueden aparecer en la base, argumentos genéricos, punteros y arreglos del tipo destino;
también pueden repetirse o cambiar de posición. Un parámetro del alias oculta cualquier alias de
igual nombre del scope exterior.

La expansión ocurre antes de registrar plantillas y calcular especializaciones. Por tanto, un
alias no declara un tipo nominal, no produce descriptor, layout, símbolo, template ni identidad
ABI propios. `using Twice<T> = Pair<T, T>;` usado como `Twice<int>` es exactamente `Pair<int, int>`
para resolución de miembros, overloads, ownership, NSMETA y generación de código.

Las cadenas de aliases se expanden transitivamente en el scope de declaración. Los modificadores
del destino quedan dentro de los escritos en el sitio de uso: si `Pointer<T>` significa `T*`,
`Pointer<int>[]` significa un arreglo de punteros a `int`. Un ciclo directo o indirecto, una aridad
incorrecta o qualifiers de ownership contradictorios rechazan el programa antes de HIR.

A type alias may declare parameters with `using Name<T0, ...> = Type;`. Every use must provide the
exact declared arity. Expansion precedes template registration and specialization, so aliases
create no nominal, metadata, layout, template, or ABI identity. Alias chains are transitive;
cycles, arity mismatches, and contradictory ownership qualifiers are compile-time errors.

### NS-GEN-003 — Inferencia genérica local

Una llamada a un método genérico puede omitir su lista `<...>` cuando todos los parámetros de tipo
se deducen de sus argumentos de valor. La deducción unifica recursivamente el tipo del parámetro
con el tipo estático local del argumento. Reconoce una aparición directa `T`, contenedores
genéricos como `Box<T>`, y sufijos de puntero o arreglo como `T*` y `T[]`.

La evidencia puede proceder de literales, parámetros, campos, locals de tipo explícito, locals
`var` cuyo inicializador tenga tipo deducible, casts, `default`, construcción de objetos, `new`,
`stackalloc`, expresiones by-reference y otra llamada genérica local ya inferida. Los argumentos
nombrados se emparejan por nombre y los parámetros opcionales omitidos no aportan evidencia.

Una aparición repetida del mismo parámetro debe deducir exactamente el mismo tipo; la inferencia no
inventa un "mejor tipo común" mediante conversiones. `null`, lambdas sin target y parámetros que
aparecen sólo en el retorno no aportan evidencia. Si falta evidencia o existe un empate igualmente
específico con deducciones incompatibles, la llamada se rechaza y el programador escribe `<...>`.
El tipo esperado del resultado nunca participa: `T Make<T>()` requiere `Make<int>()`.

Cuando varias plantillas son viables, una forma estructural (`Box<T>`, `T[]`, `T*`) es más específica
que una aparición desnuda `T`. Después de elegir la deducción, todas las plantillas del grupo se
materializan con esa sustitución y la resolución ordinaria de overloads decide por tipos. La
especialización resultante usa exactamente la misma cache e identidad que una llamada explícita.

A generic method call may omit `<...>` when every type parameter can be inferred from local value
arguments. Inference recursively unifies exact static shapes, including nested generic types,
pointers, arrays, named/default arguments, and by-reference arguments. Return context, null, and an
untargeted lambda provide no evidence. Missing or equally specific incompatible evidence requires
explicit type arguments; inferred and explicit calls share one canonical specialization cache.

### NS-OBJ-001 — Raíz de objetos sin boxing

Las clases pueden compartir la raíz `NativeSharp.Core.Object` y su dispatch virtual. Los structs
reciben operaciones equivalentes resolubles estáticamente sin cambiar su layout. Convertir un valor
struct o primitivo a `Object` no crea una caja implícita; si no existe una conversión explícita
admitida, el programa se rechaza.

### NS-CFLOW-001 — Transferencias de control

`do...while` ejecuta el cuerpo antes de su primera condición y `continue` salta a esa condición.
`switch` no tiene caída implícita entre secciones; una sección que termina sale del switch. `break`
afecta al constructo rompible más cercano y `continue` al bucle más cercano, aun cuando haya un
switch intermedio. Etiquetas duplicadas o no constantes se rechazan.

## 12. Optimización y observabilidad

### NS-INLINE-001 — Contrato semántico de inlining

Cada callable tiene exactamente una política de inlining: automática, preferida (`inline`),
obligatoria (`forceinline`) o prohibida (`noinline`). La política automática deja la decisión al
modelo de costo. La preferida solicita expansión, pero permite rechazarla cuando no sea legal o
segura. La obligatoria exige expandir todo call site cuyo destino sea estáticamente resoluble; si
la expansión no puede realizarse, el compilador debe diagnosticarlo. La prohibida exige conservar
una llamada fuera de línea.

El inlining no cambia el orden de evaluación ni efectos observables, ownership, préstamos,
cleanups, destrucción, propagación de excepciones, dispatch virtual o de interfaz, layout, ABI,
identidad de símbolos ni requisitos de exportación. Un miembro público, exportado o cuya dirección
se toma conserva una definición fuera de línea cuando el programa o ABI pueda necesitarla. Un call
virtual o de interfaz sólo puede expandirse después de demostrar el destino real sin omitir el
dispatch permitido. Las políticas contradictorias se rechazan y una política desconocida en un
artefacto precompilado nunca se interpreta como automática.

Cuando una expansión ocurre con información de depuración, el compilador debe atribuir instrucciones
y variables a scopes inline reales y conservar un sitio de llamada recorrible. La política no
autoriza inventar frames ni ocultar un frame que todavía ejecuta fuera de línea.

Every callable has exactly one inlining policy: automatic, preferred, required, or forbidden.
Inlining never changes observable semantics, ownership, cleanup, exception, dispatch, ABI, symbol,
export, or debugging obligations. Required inlining must diagnose a statically resolvable call that
cannot be expanded; forbidden inlining must retain an out-of-line call.

La política automática no realiza expansión en O0. En O1, O2 y O3 delega rentabilidad al modelo de
costo target-aware del pipeline canónico, con agresividad conservadora, balanceada y agresiva,
respectivamente. El nivel elegido y el target son entradas reproducibles de esa decisión; el orden
de compilación en paralelo no lo es.

El modificador `inline` puede aplicarse a un método concreto que declara cuerpo. Establece política
preferida: eleva su prioridad ante el modelo de costo, pero no exige que el call site se expanda.
No es válido en fields, properties, eventos, tipos, constructores, destructores, imports ni métodos
abstractos. Su presencia no elimina una definición fuera de línea requerida.

El modificador `forceinline` puede aplicarse a un método concreto que declara cuerpo. Establece
política obligatoria: toda llamada cuyo destino sea ese método y esté resuelta estáticamente debe
expandirse, también en O0. No habilita inlining automático para otros métodos. Las ubicaciones sin
cuerpo ordinario, la combinación con `inline` y un ciclo recursivo compuesto exclusivamente por
métodos `forceinline` son errores, porque ninguna emisión podría cumplir esas promesas. El método
conserva una definición fuera de línea cuando su visibilidad, exportación o toma de dirección la
requieren.

El modificador `noinline` puede aplicarse a un método concreto que declara cuerpo. Establece política
prohibida: los call sites conservan una llamada fuera de línea en todos los niveles de optimización.
No cambia la accesibilidad, identidad física, convención de llamada ni exportación del método. No es
válido en una declaración abstracta ni en otra ubicación sin cuerpo ordinario, y no puede combinarse
con `inline` o `forceinline`.

En un método genérico, la política de inlining forma parte de la plantilla semántica. Toda
especialización cerrada —creada por argumentos explícitos o por inferencia local— hereda la misma
política. La sustitución de tipos no puede convertir una preferencia en obligación, eliminar una
prohibición ni tratar la especialización como un método automático.

Los métodos de struct obedecen estas políticas sobre su receptor por dirección, sin boxing ni
cambio de representación. Una llamada de interfaz puede tratarse como llamada concreta sólo si el
compilador demuestra el tipo dinámico exacto del receptor. Esta edición reconoce la conversión
directa de una construcción fresca, por ejemplo `((IValue)new Counter()).Read()`, y selecciona su
implementación mediante la tabla semántica de la clase. Un parámetro, field, local u otra expresión
potencialmente polimórfica conserva el dispatch de interfaz; una política de implementación no basta
para eliminarlo.

Las políticas explícitas forman parte del contrato precompilado. Un método ordinario público
`inline` o `forceinline` conserva su declaración completa y su cuerpo en NSMETA, de modo que un
proyecto que sólo posee `.nsmeta` y `.lib` puede analizarlo y expandirlo con el mismo significado.
La copia emitida por el consumidor comparte identidad física ODR con el productor. `noinline` no
transporta implementación: conserva el símbolo externo y aplica la prohibición a la declaración
importada. Los métodos genéricos siguen transportándose como templates reutilizables y heredan la
política al especializarse.

En O0 con `-g`, una llamada `forceinline` expandida conserva un scope inline CodeView/PDB, las líneas
y locales del cuerpo y la ubicación desde la que fue llamada. Una llamada `inline` que el
optimizador no expandió y una llamada `noinline` conservan procedimientos ordinarios. El depurador
muestra la transformación efectiva; no fabrica un inline site por la sola presencia del modificador.

### NS-SIMD-001 — Vectores SIMD portables

`NativeSharp.Core.Numerics.Float32x4` es un valor de 128 bits formado por cuatro `float`. Su
representación pública es el struct `{ X, Y, Z, W }`; por tanto, layout, metadata, ABI y símbolos
de depuración no dependen del conjunto de instrucciones de la CPU. `Add`, `Subtract` y `Multiply`
operan simultáneamente sobre los cuatro carriles y el backend debe expresarlos como operaciones
vectoriales, sin convertirlos en cuatro llamadas escalares. El target puede legalizar esas
operaciones a instrucciones nativas o a una secuencia equivalente conservando IEEE estricto.

### NS-CONF-001 — O0 y O2 conservan el significado

O0 y O2 deben conservar el mismo resultado observable, orden de efectos exigido, traps definidos,
cleanups, ownership, layout y ABI. Una optimización puede eliminar trabajo sólo cuando demuestra
que ningún comportamiento permitido por esta especificación puede observar la diferencia.

El backend debe verificar el módulo LLVM antes de producir un artefacto de release. Encontrar IR
inválido es un fallo del compilador; no puede delegarse al enlazador ni presentarse como error del
programa.

## 13. Fuentes históricas y autoridad

Los documentos `mkdown/110_contrato_de_overflow.md` y `mkdown/111_contrato_de_memoria.md` conservan
el razonamiento original. Los documentos 132–139 conservan la evolución de bibliotecas y ABI. El
libro es la guía didáctica extensa. En caso de contradicción semántica para una versión 1.0, este
archivo es la autoridad normativa y la contradicción debe abrir un defecto: no se resuelve eligiendo
silenciosamente el texto más conveniente.
