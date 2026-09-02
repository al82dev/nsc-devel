# 78 — Huecos del lenguaje, barrido completo

**Fecha:** 2026-09-01 · **Alcance:** el compilador. Sólo el compilador.
**Pregunta que contesta:** *«me refiero a todo el lenguaje, busca gaps»*.
**Referencia contra la que se midió:** la lista oficial de palabras clave de C#
(`learn.microsoft.com/en-us/dotnet/csharp/language-reference/keywords/`), **77 reservadas y 48
contextuales**, recorrida una por una (§0.1), más las formas sintácticas que no son palabras clave
(`??`, `?.`, `$"…"`, `[Attr]`, `{ X = 1 }`, `[,]`, `#if`).

---

## 0. Cómo se midió, y por qué importa

Este barrido lo hago después de que dos inventarios míos de esta misma semana salieran **falsos**:
uno contó etiquetas `case` y dijo «queda una forma», cuando `++` muere *dentro* de un `case` que sí
existe; otro grepeó operadores en el HIR y dio ausentes `/ % << | ^ ~ && || ?:`, que sí funcionan,
porque el HIR pasa el token sin nombrarlo. La regla del milestone CC1 aplica igual aquí:

> **Ninguna verificación cuenta si el modelo contra el que verifico lo construí yo.**

Así que cada afirmación de este documento nombra **la capa que decide** y **la línea que lo dice**:

| Pregunta | Capa que decide | Cómo se leyó |
|---|---|---|
| ¿existe la palabra? | `src/Lexer/Lexer.cpp`, tabla de keywords | las 75 entradas, enteras |
| ¿existe como palabra contextual? | `Parser.cpp`, `atContextualKeyword("…")` | las 8 ocurrencias, más `lexeme() == "from"` — 9 en total |
| ¿existe el símbolo? | `include/nativesharp/Lexer/TokenKind.h` | los 52 tokens, enteros |
| ¿la parsea? | `Parser.cpp` | el sitio concreto, leído |
| ¿la tipa? | `ProjectMethodBodyModel.cpp` | el sitio concreto, leído |
| ¿la baja? | `ProjectOopHirModel.cpp` | **el `case` y su cuerpo**, no sólo la etiqueta |

Y una lección nueva, que es la razón de que este barrido no se pudiera hacer sólo con la tabla de
keywords:

> **`ref` y `out` NO son palabras clave de este lexer.** No están en la tabla. La fixture
> `tests/interop_contract/c4_ref_out_in` las usa y pasa en todas las corridas. Se resuelve leyendo
> el parser: `Parser.cpp:3375` y `:4825` las reconocen **por lexema**, con
> `atContextualKeyword("ref")` / `("out")`. `in` sí es keyword (`KwIn`), reusada de `foreach`.

Es decir: **la tabla de keywords, sola, miente en las dos direcciones.** Hay palabras que funcionan
sin estar en ella, y hay palabras en ella cuya forma muere tres capas más abajo.

---

## 0.1 El diff mecánico contra la página de Microsoft

Las dos listas se cruzaron con un script, no a ojo, porque a ojo ya me equivoqué dos veces esta
semana. La lista de Microsoft se transcribió entera y se contó: **77 reservadas** (no 79, que es la
cifra que se repite por ahí y que yo mismo había escrito antes de contarla) y **48 contextuales**.
El árbol se leyó de su propia fuente: **75 keywords** en la tabla de `src/Lexer/Lexer.cpp` y **9
palabras contextuales** en `Parser.cpp` (`align`, `at`, `from`, `out`, `pack`, `partial`, `ref`,
`union`, `where`).

**Reservadas de C# que este lenguaje no tiene — 12 de 77:**

`checked` · `decimal` · `fixed` · `goto` · `lock` · `object` · `params` · `sbyte` · `typeof` ·
`unchecked` · `unsafe` · `volatile`

Las otras 65 están, incluidas `ref` y `out`, que están como contextuales y no como keywords (§0).

**Contextuales de C# que este lenguaje sí tiene — 8 de 48:**

`from` · `get` · `nint` · `nuint` · `partial` · `set` · `var` · `where`

Con una diferencia que importa para portar código: **en este lenguaje `get`, `set` y `var` son
palabras clave de verdad, no contextuales.** En C# `int get = 3;` es legal; aquí no compila. Y
`from` no es la de LINQ: es la de `extern C from "kernel32.dll"`.

**Contextuales de C# que no existen — 40 de 48, y la mayoría no cuenta.** De esas 40, **33 no son
core**: son LINQ (`from` como query, `select`, `where`, `join`, `group`, `orderby`, `let`, `into`,
`by`, `on`, `equals`, `ascending`, `descending`), patrones (`and`, `or`, `not`, `when`, `notnull`,
`allows`, `unmanaged`, `managed`), `async`/`await`, `record`, `dynamic`, `with`, `init`, `required`,
`scoped`, `file`, `extension`, `field`, `global`, `alias`, `args`, `closed`, `safe`. Superficie de
C# moderno o de consultas, que este lenguaje nunca se planteó y que **no bloquea escribir nada**.

Las **7 que sí son core** y no están: `nameof` (§3.17), `add`/`remove` (accesores de evento propios),
`when` (filtro de `catch`, §3.15), `init`, `global`, `yield` (§3.6). Y `value` **no falta**: no es
palabra del lexer, pero el parser inyecta el parámetro implícito con ese nombre en los `set`, así
que la forma funciona igual que en C#.

**Palabras que este lenguaje añade y C# no tiene — 11:**

`borrowed` `delete` `export` `move` `owned` `raii` (propiedad y vida) · `align` `at` `pack` `union`
(layout explícito, C2) · `C` (en `extern C`)

**El saldo, quitando lo que no es core:** de las 125 palabras de C#, faltan 52. **33 de ellas son
LINQ, patrones, `async` y `record`** — ruido para este proyecto. Quedan **19 ausencias de core**: las
12 reservadas de arriba más `nameof`, `add`/`remove`, `when`, `init`, `global` y `yield`. Ésas son
las que se discuten en §1.

---

## 1. El core, ordenado por lo que muerde

Quitando LINQ, patrones, `async` y `record` — que no son de este proyecto — lo que falta **del core**
son 30 cosas, y no valen lo mismo. Ordenadas por consecuencia real, no por esfuerzo:

### Nivel 1 — rompen o falsean un programa correcto

| Qué | Por qué está aquí | Ref |
|---|---|---|
| `++` `--` y `+= -= *= /= %= &= \|= ^= <<= >>=` | **el hueco más grande del lenguaje.** Parsea, tipa, y muere en el HIR. Todo bucle se escribe `i = i + 1`. Un consumidor externo choca en la primera hora | 79, 81 |
| **`volatile`** | **hay hilos de verdad** (`Thread`, `Mutex`, `Monitor`, `Semaphore`, `AtomicInt32`) y **no hay forma de expresar una lectura que el optimizador no pueda mover ni cachear.** La propia `Atomics.ns` lo dice: `AtomicInt32.Value` no emite barrera y hay que usar `CompareExchange` consigo mismo para leer con garantías. Eso es un rodeo, no una solución | 3.5 |
| `checked` / `unchecked` | el desbordamiento entero **no se puede pedir ni suprimir**: es siempre el del backend, sin forma de decirlo en el código | 3.4 |
| `sbyte` | **no hay entero de 8 bits con signo.** Un `signed char` de una API de C no se declara honestamente hoy | 3.1 |

### Nivel 2 — se nota al escribir una biblioteca

`p->campo` y `p->Metodo()` (80, 83) · restricciones genéricas `where` (3.9) · interfaces genéricas
(3.10) · **tipos anidados** (3.11) · constructores estáticos (3.12) · `enum : byte` (3.24) ·
`params`, parámetros opcionales, argumentos con nombre (3.7) · métodos de extensión (3.8) ·
propiedades sólo de escritura (3.23) · `+=` sobre propiedad (82) · lambdas anidadas (84) ·
delegados a virtual y a struct (85, 86) · atributos (3.18) · `goto` (3.2)

### Nivel 3 — comodidad; se vive sin ello

interpolación `$"…"` y verbatim `@"…"` (3.25) · `??` y `?.` (3.16) · `is T x` y `when` (3.14, 3.15) ·
`new T { X = 1 }` (3.19) · miembros `=> x` (3.20) · `int[,]` (3.21) · `for` con varios incrementos
(3.22) · `typeof`/`nameof` (3.17) · separadores `1_000` y escapes `\u` (3.25) · preprocesador (3.26)
· `lock` (hay `raii ScopedLock`, que hace lo mismo con más letras) · `object`, `decimal`, `unsafe`,
`fixed` como palabras (el nivel crudo aquí no lleva ceremonia, y el arreglo fijo se escribe
`float[4] x`)

---

## 1.1 Las dos categorías

Lo de arriba está ordenado por daño. Lo de abajo está ordenado por **dónde muere**, que es lo que
decide cuánto cuesta arreglarlo:

* **HUECOS** (§2) — la forma **se escribe, se parsea y se tipa**, y muere en el lowering. Son
  peligrosos: el programa parece válido hasta el último momento, y el mensaje habla de etapas
  internas («Stage 34»), no del lenguaje. **Son 8.**
* **AUSENCIAS** (§3) — la forma **no existe**: no hay token, no hay `case`, no hay nada. El error es
  un error de sintaxis honesto, en la primera capa. **Son 24 familias.**

Y lo que **sí funciona de punta a punta** está en §4, porque una lista de huecos sin la lista de lo
que hay no se puede leer.

---

## 2. HUECOS — la mitad delantera existe, la trasera no

Este es el defecto recurrente de este árbol. Ya lo cerré cuatro veces en CC1 (`sizeof`,
`stackalloc`, `new T`, aritmética de punteros). Éstos son **los que quedan**, todos medidos leyendo
el cuerpo del `case`, no la etiqueta.

| ID | Forma | Parsea | Tipa | Baja | Dónde muere, literal |
|---|---|:--:|:--:|:--:|---|
| **79** | `++x` `--x` `x++` `x--` | sí | **sí** | **no** | `ProjectOopHirModel.cpp:1801` — *«Stage 34 OOP lowering does not yet lower ++/--»* |
| **80** | `p->campo` | sí | sí | **no** | `ProjectOopHirModel.cpp:3494` — mensaje propio, nombra `(*p).campo` |
| **81** | `+= -= *= /= %= &= \|= ^= <<= >>=` | sí | **sí** | **no** | `ProjectOopHirModel.cpp:2231` — *«currently requires simple '=' assignment; compound assignment remains in procedural HIR»* |
| **82** | `x += h` sobre **propiedad o indexador** | sí | **no** | — | `ProjectMethodBodyModel.cpp:9052` — *«deferred; use an explicit read-modify-write»* |
| **83** | `p->Metodo()` | sí | **no** | — | `ProjectMethodBodyModel.cpp:4167` — *«not part of Stage 34 instance-callable semantics»* |
| **84** | lambda anidada (lambda dentro de lambda) | sí | **no** | — | `ProjectMethodBodyModel.cpp:9966` — *«nested lambdas are deferred beyond Stage 43»* |
| **85** | delegado a método **virtual** | sí | **no** | — | `ProjectMethodBodyModel.cpp:10198` — *«virtual delegate thunks are deferred»* |
| **86** | delegado con receptor **struct** | sí | **no** | — | `ProjectMethodBodyModel.cpp:10207` — *«struct receiver lifetime capture is deferred»* |

Notas que cambian la lectura de la tabla:

**79 y 81 son el mismo hueco.** El HIR pide `TokenKind::Equal` y rechaza todo lo demás; `++` cae por
la rama gemela unas líneas antes. Un consumidor externo que escriba C# normal choca con los dos en
la primera hora. Medida colateral, y es la que explica por qué nadie lo había notado dentro:
**el propio código NativeSharp del árbol tiene cero usos reales de `++`, `+=`, `*=` o `/=`** — las 8
apariciones de `++` están todas en comentarios — contra **185** instancias de `x = x + 1`. El
compilador nunca se ha compilado a sí mismo contra esta parte de su propio lenguaje.

**81 tiene una excepción que sí funciona:** `evento += manejador` y `evento -= manejador`. Ésos van
por otra ruta entera (`ProjectMethodBodyModel.cpp:8713`) y están completos. Es decir, `+=` funciona
**exactamente** en el único caso donde no es aritmética.

**82 es distinto de 81** aunque se escriba igual: aquí muere una capa antes, en la semántica, porque
la lectura y la escritura de una propiedad son dos llamadas y nadie decidió aún el orden de
evaluación del receptor. Vale como ID propio.

**Consecuencia práctica de 79 sobre `for`:** como `++` no baja, todo `for` de este lenguaje se
escribe `for (int i = 0; i < n; i = i + 1)`. Ver también la ausencia 3.22.

---

## 3. AUSENCIAS — la forma no existe

Aquí no hay hueco: hay una decisión (explícita o por omisión). El error llega en la primera capa,
que es lo correcto. Las agrupo por familia, con el mecanismo que lo demuestra.

### 3.1 Tipos primitivos que no existen
`sbyte`, `decimal`, `object` **no están en la tabla de keywords**. `byte` existe y es sin signo; **no
hay entero de 8 bits con signo en el lenguaje**. `object` como palabra no existe, aunque el tipo raíz
`NativeSharp.Core.Object` sí (C8). `nint`/`nuint` sí existen (no son de C# reservado, son
contextuales allá).

### 3.2 `goto` y etiquetas
No hay token. `Parser::parseStatement` despacha exactamente 14 formas y ninguna es una etiqueta.

### 3.3 `lock`
No hay token. No hay primitiva de sincronización en el lenguaje.

### 3.4 `checked` / `unchecked`
No hay token. La aritmética entera **no tiene forma de pedir comprobación de desbordamiento** ni de
suprimirla: es siempre la del backend.

### 3.5 `unsafe`, `fixed` (sentencia), `volatile`

**`volatile` es la ausencia más seria de todo este documento, y es de nivel 1.** No es teoría: el
árbol **tiene hilos de verdad** — `lib/NativeSharp.Core.Threading/` trae `Thread`, `Mutex`,
`Monitor`, `Semaphore`, `Event`, `ScopedLock` y `Atomics` (con cinco operaciones núcleo
interlocked bajadas a LLVM). Con hilos y sin `volatile`, **no hay forma de escribir en este lenguaje
una lectura que el optimizador no pueda mover, cachear en un registro o eliminar.**

Lo dice el propio árbol, en `Atomics.ns:89`:

> *«Lectura NO atomica … esta propiedad NO emite ninguna instruccion con prefijo `lock` ni ninguna
> barrera de memoria/fence … codigo que necesita el valor mas reciente con garantias de orden entre
> hilos debe usar Exchange/CompareExchange con el mismo valor como ambos argumentos»*

Es decir: la biblioteca **ya se topó con el hueco y lo rodeó**, documentando el rodeo. Un
`CompareExchange` consigo mismo para leer un `int` es correcto y es caro; y una bandera `bool`
compartida entre hilos, escrita en el lenguaje llano, **hoy se puede optimizar hasta desaparecer**.
Lo mismo vale para un registro mapeado en memoria en código de interop.

Al lado de eso, `unsafe` y `fixed` no faltan de verdad: **el nivel crudo aquí no está detrás de una
palabra**, por diseño. Punteros,
`stackalloc`, `&x`, `delete` y la aritmética de punteros son parte del lenguaje llano, sin ceremonia.
Es una decisión de diseño coherente con el objetivo (interop con C), no un olvido — pero significa
que **no hay forma de marcar una región como insegura**, ni de anclar un objeto.
*Ojo con el nombre:* `float[4] albedo` (arreglo fijo dentro de un struct) **sí existe** y se escribe
con el tamaño en el tipo, no `fixed float albedo[4]`. Ver `tests/interop_contract/c2_11_fixed_field_array`.

### 3.6 `yield`, `async`, `await`
Ninguno tiene token. No hay iteradores ni asincronía en el lenguaje.

### 3.7 `params`, parámetros opcionales, argumentos con nombre
`Parser::parseParameter` (línea 3366) parsea exactamente `[ref|out|in] Tipo nombre`. **No hay valor
por omisión, no hay `params`, no hay `this` de método de extensión, no hay atributos.** Y en la
llamada no hay `nombre:` — no aparece en ningún sitio del parser.

### 3.8 Métodos de extensión
Consecuencia de 3.7: no hay modificador `this` en un parámetro.

### 3.9 Restricciones genéricas `where`
**Sí se parsea y se rechaza a propósito**, en tres sitios (`Parser.cpp:1190`, `:1243`, `:3088`):
*«generic constraints are deferred beyond Stage 44; monomorphization currently requires unconstrained
type parameters»*. Es la única ausencia de esta sección con mensaje propio, y por eso es la mejor
hecha: quien la escriba sabe qué pasó.

### 3.10 Interfaces genéricas
`parseGenericParameterList` se llama en clases/structs (1354), delegados (1183, 1236), métodos (3081)
y funciones libres (3311). **No en `parseInterfaceDeclaration`.** El propio árbol lo dice en un
comentario (`Parser.cpp:3813`). `interface IComparable<T>` no se puede declarar.

### 3.11 Tipos anidados
`parseClassMember` (1574–3150) **nunca** mira `KwClass`, `KwStruct`, `KwEnum`, `KwInterface` ni
`KwDelegate`. Un tipo sólo se declara en un namespace o al tope. No hay `class A { class B {} }`.

### 3.12 Constructores estáticos
No existen. `Parser.cpp:1951` los rechaza explícitamente junto con otros modificadores en
constructores. No hay inicialización estática de tipo.

### 3.13 `record`, tipos anónimos, tuplas, deconstrucción
Ninguna palabra, ningún token. `(a, b)` como tipo o como destino de asignación no se parsea: `(` abre
una expresión entre paréntesis y la coma la termina.

### 3.14 Patrones
`is` existe pero es **sólo una prueba de tipo** (`Parser.cpp:4536`): `is T` produce
`TypeTestExpression` y **no declara variable**. No hay `is T x`, ni `is not`, ni patrones de
propiedad, ni `switch` de expresión, ni `when`.

### 3.15 Filtro `when` en `catch`
`parseCatchClause` (4234) acepta `catch { }`, `catch (T nombre) { }` y **nada más**. Ni siquiera
`catch (T)` sin nombre: exige el identificador.

### 3.16 `??`, `??=`, `?.`
No hay tokens. El único uso de `?` en la gramática es el ternario, y el propio `TokenKind.h` lo dice
en el comentario de `Question`.

### 3.17 `typeof` y `nameof`
No hay tokens. Hay una operación núcleo `ObjectTypeName` (C8) que lee el nombre del descriptor en
tiempo de ejecución, pero **no hay forma de escribir `typeof(T)` en el lenguaje**.

### 3.18 Atributos
No hay ninguna ruta que lea `[` en posición de declaración. `[Attr]` no existe en ninguna forma.

### 3.19 Inicializadores de objeto y de colección
`new T(args)` produce `ObjectCreationExpression` (`Parser.cpp:4732`) y ahí termina: **no se mira `{`
después**. No hay `new Foo { X = 1 }` ni `new List<int> { 1, 2 }`.
Sí existe el inicializador agregado de C7 en declaración: `V v = {1, 2};`.

### 3.20 Miembros con cuerpo de expresión
`FatArrow` (`=>`) se usa **sólo** para lambdas (`Parser.cpp:4470`; los tres sitios anteriores son
lookahead para reconocerlas). No hay `int P => x;` ni `int F() => x;`.

### 3.21 Arreglos multidimensionales
`TypeModifierSyntaxKind` tiene exactamente tres valores: `Pointer`, `Array` (tamaño fijo) y
`DynamicArray`. **No hay rango.** `int[,]` intenta parsear la coma como expresión y falla. Los
escalonados (`int[][]`) sí forman dos modificadores.

### 3.22 `for` con varios inicializadores o incrementos
`parseForStatement` llama a `parseExpression` **una vez** por hueco. `for (int i = 0, j = n; …; i = i + 1, j = j - 1)`
no se parsea. No hay operador coma.

### 3.23 Propiedades e indexadores sólo de escritura
Rechazados con mensaje propio: `Parser.cpp:2506` y `:2696`. La lectura de una propiedad resuelve por
el getter, así que una propiedad sin getter no se puede ligar.

### 3.24 `enum` con tipo base explícito
`parseEnumDeclaration` (3148) va de `enum Nombre` directo a `{`. **No lee `: byte`.** El tipo
subyacente no es elegible.
*Relacionado con la tarea 78 abierta:* `valueLayoutForTypeKey` dice que un `enum` mide 8 y
`layoutNamedType` dice 4, y sigue **sin medir**.

### 3.25 Literales y cadenas
* **No hay interpolación** `$"…"`: no hay token `$`.
* **No hay cadenas verbatim** `@"…"`: no hay token `@`.
* **No hay cadenas crudas** `"""…"""`.
* **No hay separador de dígitos** `1_000`: el bucle de dígitos sólo acepta dígitos.
* **No hay sufijos** `d`, `D`, `m`, `M`. Sí `f`/`F` (float) y `u`/`U`/`l`/`L` en enteros.
* **No hay escapes `\u` ni `\U`.** Sí `\0 \a \b \f \n \r \t \v \\ \' \" \xHH`.
* Sí hay literales **hexadecimales** (`0x…`) y **binarios** (`0b…`).

### 3.26 Directivas de preprocesador
No hay ninguna. `#if`, `#region`, `#pragma`, `#line`, `#define` no existen: el lexer nunca mira `#`.

### 3.27 Otras palabras contextuales de C# que no existen
*(La lista completa y contada está en §0.1; aquí sólo lo que hace falta explicar.)*
`global`, `dynamic`, `init`, `required`, `with`, `scoped`, `and`/`or`/`not` (patrones),
`add`/`remove` (accesores de evento), `from`/`where`/`select`/… de LINQ. **`value` sí existe**: el
parser inyecta el parámetro implícito con ese nombre en los tres sitios donde nace un `set`
(`Parser.cpp:2295`, `:2433`, `:2649`), igual que C#. De ésas, la única que este árbol reusa es **`from`**, y no para LINQ: para
`extern C from "kernel32.dll"` (`Parser.cpp:1796`).

### 3.28 Palabras contextuales que este lenguaje **añade** y C# no tiene
Vale anotarlas porque son la otra mitad del inventario: `align`, `at`, `pack`, `union` (C2, layout
explícito) y `partial` (que en C# también existe). Como keywords propias: `raii`, `move`, `owned`,
`borrowed`, `export`, `delete`, `C` (en `extern C`).

---

## 4. Lo que sí funciona de punta a punta

Sin esta lista, la de arriba es ilegible. Todo lo de aquí llega al backend.

**Tipos:** `void bool byte char short ushort int uint long ulong nint nuint float double string`,
punteros `T*`, arreglos fijos `T[n]`, dinámicos `T[]`, `struct`, `class`, `interface`, `enum`,
`delegate`, genéricos por monomorfización (sin restricciones), alias `using X = Y;`, `union`/`pack`/
`align`/`at` para layout explícito.

**Sentencias:** bloque, `if/else`, `while`, `do…while`, `for`, `foreach`, `switch` (**sin
fall-through, por diseño**), `break`, `continue`, `return`, `try/catch/finally`, `throw`, `using`,
declaración, expresión.

**Expresiones:** todos los operadores binarios y unarios `+ - * / % & | ^ << >> == != < <= > >=
&& || ! ~`, ternario `?:`, asignación simple, llamada, índice, miembro, `(*p).campo`, `this`, `base`,
casts, `is`, `as`, lambdas (no anidadas), `sizeof(T)` y `sizeof(expr)`, `default(T)`,
`new T`, `new T[n]`, `new Clase(args)`, `delete`, `stackalloc T[n]`, `move`, aritmética de punteros,
`&x`, inicializador agregado `{1, 2}`.

**Miembros:** campos, métodos, constructores, destructores (clase), propiedades (auto y explícitas,
con getter), indexadores (con getter), eventos (`+=`/`-=`), operadores sobrecargados (los 20 tokens
binarios y unarios, **menos `++`/`--`**), conversiones `implicit`/`explicit`, `virtual`/`override`/
`abstract`/`sealed`, `static`, `readonly`, `const`, las cuatro accesibilidades.

**Interop:** `extern C`, `extern C from "dll"`, `export extern C`, `ref`/`out`/`in`, punteros a
función, structs por valor, arreglos fijos en campo, uniones.

**Propiedad y vida:** `owned`, `borrowed`, `move`, `raii`, asignación definida, análisis de flujo con
snapshot/join en `?:`, `&&`, `||`, `do…while`.

---

## 5. Qué haría yo con esto, en orden

No es una promesa de calendario, es el orden que sale de medir. Sólo core; LINQ y compañía no entran.

0. **`volatile`** (§3.5), porque es lo único de esta lista que hace que un programa **correcto se
   comporte mal en silencio**, y porque la biblioteca de hilos ya existe y ya lo está rodeando. Todo
   lo demás de aquí abajo falla ruidosamente; esto no.
1. **79 + 81 juntas, no por separado.** Son el mismo sitio del HIR y el mismo mecanismo: leer el
   lvalue una vez, operar, escribir. Hacer `++` sin `+=` deja al consumidor a la mitad del camino, y
   ya cometí ese error de análisis una vez en la tarea 62. Cierra además `operator ++`/`--`
   (`Parser.cpp:2780`), que está deferido por exactamente esta razón escrita.
2. **80** (`p->campo`) es pequeña y ya tiene mensaje propio; entra donde entró `(*p).campo`.
3. **82 y 83** cuelgan de 1 y 2 respectivamente.
4. **3.9 (`where`)**, **3.10 (interfaces genéricas)** y **3.11 (tipos anidados)** son las tres
   ausencias que más pesan para escribir una biblioteca real. Ninguna es una corrección: son fases.
5. **3.1 (`sbyte`)** es barata y cierra un agujero de interop: una API de C con `signed char` hoy no
   se puede declarar honestamente. **3.4 (`checked`)** entra aquí también.
6. Lo demás es nivel 3 (§1). Cabe decidir explícitamente **que no se va a tener**, y escribirlo, en
   vez de dejarlo como pendiente tácito. Y LINQ, patrones, `async` y `record` **no son de este
   proyecto**: no vuelven a aparecer en ningún documento como pendiente.

---

## 6. Lo que este barrido corrige de documentos anteriores

* `69_huecos_de_runtime_y_lenguaje.md` daba por presentes formas mirando sólo la mitad delantera del
  pipeline. Ya se corrigió §1 en CC1; este documento reemplaza su inventario de lenguaje entero.
* Mi propia respuesta de la ronda pasada sobre operadores decía «los 20 tokens sobrecargables». Es
  cierto **y** incompleto: `++`/`--` están en la lista de tokens y **rechazados a propósito** en
  `Parser.cpp:2780`. Queda dicho aquí.
* Ningún documento anterior de este árbol registraba que **`ref` y `out` no son keywords**. Es el
  hallazgo que obligó a rehacer el método de este barrido.
