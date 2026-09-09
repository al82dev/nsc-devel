# NativeSharp Language Specification 1.0

**Estado:** contrato normativo candidato para 1.0  
**Lenguaje implementado al redactarlo:** NativeSharp 0.1  
**Compilador de desarrollo:** `nsc 0.9.0`, Stage 67
**Target contractual:** `x86_64-pc-windows-msvc`

## 1. Cómo leer esta especificación

Este archivo define el significado estable que NativeSharp pretende publicar como 1.0. El libro
explica cómo programar; los documentos históricos explican por qué se eligió cada diseño; este
archivo decide qué debe aceptar, rechazar y producir una implementación conforme.

Las palabras **debe**, **no debe** y **se rechaza** son normativas. Un ejemplo no sustituye una
regla. Una prueba demuestra una implementación concreta, pero tampoco sustituye la regla. El mapa
`NativeSharp_1_0_conformance.map` conecta cada cláusula con sus lugares de implementación y con la
evidencia ejecutable que evita que la prosa y el compilador evolucionen por separado.

La versión visible del lenguaje continúa siendo 0.1 mientras se prepara 1.0. Llamar a este archivo
“Specification 1.0” identifica el contrato candidato; no promueve por sí solo la versión pública.

## 2. Conformidad y programa válido

Un programa es válido cuando:

1. su texto forma tokens y sintaxis aceptados por `NativeSharp_Grammar_1_0.ebnf`;
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
`NativeSharp_Grammar_1_0.ebnf`. `C`, `ref`, `out`, `where`, `from`, `pack`, `align` y `at` son
contextuales: sólo adquieren significado especial en la producción que los nombra. Los literales
enteros fuera del rango que su sufijo o contexto permita se rechazan; no se recortan durante lexing.

### NS-SYN-001 — Contrato sintáctico

`NativeSharp_Grammar_1_0.ebnf` es parte de esta especificación. Define la superficie de unidades de
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

### NS-ARRAY-001 — Arrays nativos

Un array dinámico contiene elementos contiguos y su almacenamiento se crea explícitamente con
`new T[n]` o un inicializador inferido. `delete[]` descarga el buffer cuando el programa posee esa
obligación. `raii T[]` liga esa descarga al ámbito. La propiedad del buffer y la propiedad de cada
elemento se analizan por separado; mover un elemento no mueve el buffer.

Un fixed array forma parte del valor que lo contiene y su cantidad es contractual. No puede
degradarse a un puntero para cambiar layout o lifetime silenciosamente.

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

Las operaciones públicas de `AtomicInt32` y `AtomicInt64` son atómicas y secuencialmente
consistentes. Incremento, decremento y suma devuelven el valor nuevo; intercambio y
comparación-intercambio devuelven el valor anterior. La implementación debe emitir operaciones
LLVM `seq_cst`; una llamada ordinaria con nombre parecido no satisface esta cláusula.

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

Un fixed array ocupa `stride(elemento) * cantidad`. Su layout forma parte del agregado; no es una
referencia ni lleva cabecera oculta.

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

### NS-GEN-001 — Monomorfización

Los genéricos se especializan estáticamente para argumentos concretos. No existe despacho genérico
dinámico. La identidad de una especialización incluye la identidad canónica de sus argumentos y se
conserva entre productor y consumidor. Constraints se comprueban en compilación y una
especialización inválida se rechaza antes de emitir código.

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
