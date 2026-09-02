# 94 — Bloque 3: vistas y tiempo de vida

**Fecha:** 2026-09-02
**Qué cierra:** el Bloque 3 entero — **SHR-010** (`StringView`), **SHR-011/012**
(`Span<T>`/`ReadOnlySpan<T>`), **SHR-017** (`Optional<T>`) y **SHR-013** (escape checking para
vistas), que es una regla del compilador y no un tipo de biblioteca.
**Estado:** entregado. Compilado, bajado a LLVM IR y **verificado** en el contenedor sobre los 167
proyectos del árbol; los números de Windows los pone `all.bat`.

---

## 1. El criterio, y por qué tiene dos mitades que se miden en sitios distintos

El doc 81 lo dejó escrito antes de empezar:

> **Terminado cuando:** el lexer guarda lexemas como vistas sin copiar **y el compilador rechaza
> devolver una vista de un local**.

Son dos afirmaciones de naturaleza distinta y **no caben en el mismo tipo de prueba**:

| mitad | dónde se mide | por qué ahí |
|---|---|---|
| «el lexer guarda lexemas como vistas sin copiar» | `tests/cc1/c23_views_and_lifetimes`, ejecutando, salida 42 | Es una propiedad de un programa que **corre** |
| «el compilador rechaza devolver una vista de un local» | `tests/unit/Stage58ViewLifetimesTests.cpp`, 31 comprobaciones | Un programa rechazado **no produce ejecutable**: no hay código de salida que mirar |

La segunda no se puede documentar con un comentario en una fixture. Se mide compilando cada forma
prohibida y exigiendo **el diagnóstico exacto**, no sólo que falle: un proyecto puede fallar por
veinte razones, y una prueba que sólo mirara el booleano pasaría igual si el rechazo viniera de una
errata en la propia prueba.

**Y «sin copiar» tampoco se afirma.** La fixture compara el puntero de cada lexema contra
`String.CharacterPointer(fuente, desplazamiento)`. Si alguna vista copiara, su puntero caería fuera
del buffer de la cadena original y la fixture lo vería. Eso es aritmética de punteros, no una
promesa en un comentario.

---

## 2. SHR-013 — `borrowed struct`, y el hueco que no se veía leyendo la regla vieja

### 2.1 El hueco

Stage 50.7 ya tenía **escape checking**, y es bueno: rechaza devolver un `borrowed Resource`
sacado de un `raii` local, guardarlo en un campo, capturarlo en un lambda. Treinta y seis
comprobaciones, todas verdes.

**No atrapaba nada de este bloque, y no por descuido.** Su comprobación está detrás de una
condición:

```cpp
const bool returnTypeOwnable =
    isReferenceTypeKey(method->declaredTypeKey) || method->declaredTypeKey == "builtin:string";
```

Una vista es un **struct por valor**. Contesta que no, y la comprobación entera se apaga.

Ese hueco **no se ve leyendo la regla**: la regla es correcta para todo lo que existía cuando se
escribió. Se ve preguntándole por un tipo que hasta el Bloque 3 no existía.

### 2.2 La decisión: un modificador explícito, no inferencia

Un tipo vista se declara:

```csharp
public borrowed struct StringView { ... }
```

La alternativa medida era **deducirlo**: «un struct con un campo puntero es una vista». Se
descartó, y la razón no es de gusto: convertiría en vista **cada struct que hoy lleva un puntero
sin haberlo pedido**, cambiando el significado de código ya escrito y ya verde. Un modificador que
el autor escribe no rompe nada existente y dice la intención en la declaración, que es donde se
lee.

Es la misma palabra que ya califica a un local o a un parámetro — `borrowed` significa «esto no es
mío» en los dos sitios — sólo que aquí la lleva **el tipo** y no cada uso, porque una vista no deja
de serlo según dónde se guarde. Es lo que C# llama `ref struct`.

**Dónde vive la marca:** el texto auxiliar de la declaración, junto a `union`, `pack=N` y
`align=N`, separados por comas. Un canal que ya existía, un `AstNodeKind` menos, y ningún `switch`
sobre declaraciones que tocar.

### 2.3 Las tres reglas, y por qué son tres y no una

| # | regla | por qué |
|---|---|---|
| **R1** | Un **campo** no puede tener tipo vista, ni ser arreglo de vistas | Un campo vive tanto como su objeto, y ese objeto no tiene relación con el dueño de lo prestado. **No hay comprobación que hacer**: no hay dos tiempos de vida que comparar, sólo uno conocido y otro cualquiera |
| **R2** | Un **retorno** de tipo vista no puede estar enraizado en un local del método | El local muere al volver. Es la línea del criterio |
| **R3** | Una vista no puede ser **capturada por un lambda** | El delegate se puede guardar, devolver o llamar horas después; lo prestado no lo sabe |

**La excepción de R1 no es una concesión:** una vista **dentro de otra vista** sí se permite.
Componer dos vistas da una vista, y el valor compuesto muere donde muera el que lo contiene, que es
un local o un parámetro y está comprobado como tal.

**Y una consecuencia que sale gratis:** `Optional<StringView>` **no compila**. Al monomorfizar,
el campo `value` pasa a tener tipo vista dentro de un agregado que no es vista, y R1 lo rechaza
ahí mismo — en el sitio donde de verdad es peligroso, con el mensaje que explica por qué. Es el
mismo motivo por el que C# prohíbe un `ref struct` como argumento genérico, y aquí no hubo que
escribir esa regla: cae de la que ya estaba.

### 2.4 El análisis de enraizamiento

R2 necesita contestar: **¿de qué local presta esta expresión?**

El análisis recorre el árbol de la expresión y se queda con el primer identificador que nombre un
**local cuyo tipo lleve un tiempo de vida** — una cadena, un arreglo, un puntero, un dueño `raii`,
u otra vista ya enraizada en un local.

Tres decisiones dentro, y las tres importan:

* **Un `int` no cuenta.** Un índice no es un préstamo. Sin esa distinción la regla sería inútil:
  casi toda expresión menciona algún local.
* **Una vista local se CONSULTA, no se asume.** `StringView v = StringView.Of(texto); return v;`
  con `texto` de parámetro es **correcto**, y una regla que dijera «un local de tipo vista
  contamina» lo rechazaría. Por eso cada local de tipo vista lleva grabado de qué se enraiza, y el
  análisis se lo pregunta.
* **Se recalcula en cada asignación**, incluida la de un **elemento de arreglo**. `v = otra;`
  cambia de qué presta `v`; y `tokens[i] = vista;` es la misma fuga con un nivel de indirección —
  un `StringView[]` local lleno de vistas de otro local y devuelto después. Sin eso, el arreglo era
  el camino obvio para rodear las otras dos reglas.

**Lo que NO hace, dicho para que nadie lo lea como más de lo que es:** no sigue el puntero. Una
vista que pasa por una llamada cuyo argumento es un local se considera enraizada en ese local
aunque la llamada no lo use. Es conservador a propósito, y en la dirección correcta: de los dos
errores posibles, **rechazar un programa correcto se ve al compilar y se arregla; aceptar uno
incorrecto es un uso-después-de-liberar que no se ve nunca**. Un análisis exacto necesitaría
parámetros de tiempo de vida en las firmas, que es otro problema y no el de este bloque.

### 2.5 Lo que sigue compilando

Una regla de rechazo sin la mitad positiva no dice si rechaza *lo correcto* o simplemente rechaza.
La primera versión de esta regla habría pasado los cinco casos negativos prohibiendo las vistas por
completo. `Stage58ViewLifetimesTests` mide también las ocho formas que **tienen** que seguir
compilando: enraizada en un campo (la forma de un lexer), en un parámetro, reenviada desde otra
llamada, con un índice local, consumida localmente sin salir, un arreglo llenado desde un
parámetro, y la composición vista-dentro-de-vista.

---

## 3. SHR-010 — `StringView`, y la operación núcleo que hizo falta

`StringView` es `{ char* start; int count; }`. Dieciséis bytes — el IR lo enseña como
`sret([16 x i8])`, devuelta por puntero oculto según el ABI de Windows x64 — y cero asignaciones.

### 3.1 Por qué hubo que tocar el compilador

`&texto[0]` **no compila**, y está bien que no compile: el operador `&` de este lenguaje exige un
local, un parámetro o un campo direccionable, y un carácter de una cadena no es ninguno de los
tres. Abrir `&` para cadenas habría dado un puntero **escribible** a algo que el compilador
declara inmutable (el HIR rechaza `texto[i] = c` con *«Stage 41 strings are immutable»*).

Así que el bloque añadió **una** operación núcleo, con su cadena de seis capas:

```
MethodCoreOperationKind::StringCharacterPointer
  -> ProjectOopHirNodeKind::StringCharacterPointer
  -> ProjectOopMirNodeKind::StringCharacterPointer
  -> lowerArrayElementAddress(node, /*string*/ true)
```

**No hay aritmética nueva en el backend.** Es la misma llamada que `StringElementLoad` ya hacía,
sin el `load` final: mismo receptor, mismo índice, **misma comprobación de rango**, mismo GEP con
paso 1 y desplazamiento 8. Si algún día el layout de una cadena cambia, cambia en un sitio y las
dos operaciones lo siguen.

Medido en el IR generado, para `String.CharacterPointer(texto, 1)` y `(texto, 0)`:

```llvm
%native.index.length = load i64, ptr %texto
%native.index.inrange = icmp ult i64 1, %native.index.length
br i1 %native.index.valid, label %native.index.ok, label %native.index.trap
...
%native.element.addr = getelementptr i8, ptr %texto, i64 9    ; 8 + 1
%native.element.addr7 = getelementptr i8, ptr %texto, i64 8   ; 8 + 0
```

**La comprobación de rango se queda.** Un puntero fuera de rango es peor que un carácter fuera de
rango, no mejor. Una vista vacía se construye con puntero nulo, no pidiendo la dirección de un
carácter que no existe.

### 3.2 Lo que la operación núcleo cuesta, dicho aquí

Devuelve `char*`, que es **escribible**, sobre bytes de una cadena que el lenguaje trata como
inmutable. Escribir a través de ese puntero corrompe una cadena que alguien más puede estar
leyendo. Existe para construir vistas — `StringView.ns` es su único consumidor previsto — no para
modificar texto. El tipo no lo dice; el comentario en `String.ns` sí.

### 3.3 La medición de «sin copiar», ejecutando

```csharp
string source = "let x = 42 + foo ;";
char* origin = String.CharacterPointer(source, 0);
// ... siete tokens ...
if (tokens[index].Data() != origin + offsets[index]) { return false; }
```

Siete lexemas, siete desplazamientos escritos a mano contra la cadena. Y en el IR,
`MiniLexer.NextToken` **no contiene ni una llamada al asignador**: sus únicas llamadas son sus
propios auxiliares, las trampas de receptor nulo y `StringView.Of`, que es un retorno por `sret` de
dieciséis bytes.

---

## 4. SHR-011/012 — `Span<T>` y `ReadOnlySpan<T>`, y por qué son dos tipos

Tienen los mismos dos campos y la misma aritmética. La única diferencia es que el indexador de uno
tiene `set` y el del otro no. La alternativa era **un solo tipo con un booleano `readOnly`**, y es
peor por dos razones que no son de gusto:

* el booleano cuesta un byte y **una comparación por escritura**, en el tipo cuyo propósito entero
  es no costar nada;
* y sobre todo, mueve a **ejecución** una comprobación que el sistema de tipos hace **gratis** en
  compilación. Un `ReadOnlySpan<T>` que no se puede escribir no falla al escribirse: **no
  compila**.

El precio es duplicar unas cuantas líneas. Se paga.

`AsReadOnly()` va en la dirección segura y la contraria no existe: sería devolver permiso de
escritura que nadie dio.

**`CopyTo` frente a `CopyToOverlapping`**, y la fixture lo mide: la primera copia hacia delante y
se pisa si el destino cae dentro del origen y por detrás; la segunda mira las direcciones y va
hacia atrás en ese caso. Los dos nombres dicen cuál es cuál en vez de que uno mienta.

---

## 5. SHR-017 — `Optional<T>`, y el problema que resuelve de verdad

Hoy el árbol tiene tres formas de decir «no hay», y las tres mienten un poco:

* **un centinela** — `-1`, `null`. Funciona hasta que el centinela es un valor legítimo;
* **`bool` + `out`** — lo que hace `Number`. Correcto, pero obliga a declarar antes, **no compone**
  y no se puede devolver;
* **lanzar** — caro, y convierte «no encontrado», que es un resultado normal, en un fallo.

`Optional<T>` es la cuarta: el «no hay» viaja **dentro** del valor, con su tipo, sin reservar
ningún valor de `T`. La fixture mide exactamente el caso que ningún centinela cubre: buscar `-1` en
un arreglo **que contiene `-1`**, y distinguirlo de no encontrarlo.

**No es una vista**, y por eso no es `borrowed struct`: guarda una copia de `T` por valor. Nada
presta de nadie y nada muere antes de tiempo, así que someterlo al SHR-013 sería mentir en la otra
dirección.

`Value()` **lanza** si está vacío, a propósito: leer el valor de un `Optional` vacío es un error del
programa, no un resultado, y devolver el `T` a cero convertiría un error en un dato plausible —
justo la clase de defecto que este tipo existe para eliminar. Quien no quiera la excepción tiene
`ValueOr`.

---

## 6. Tres huecos del lenguaje encontrados al escribir esto, medidos y anotados

Ninguno se arregla en este bloque. Los tres están **medidos**, no supuestos, y quedan con ID.

### ID 131 — un miembro estático de un tipo genérico cerrado no se puede nombrar

```csharp
class Box<T> { public static Box<T> Make(T v) { ... } }

Box<int> a = Box<int>.Make(1);      // "expected expression" / "expected ';', found 'Make'"
Box<int> b = Helper.Wrap<int>(1);   // compila
```

El parser lee `Box<int>` y se atraganta con el punto. La forma que sí funciona es un **método
genérico** sobre una clase no genérica, porque los argumentos de tipo de una **llamada** sí se
parsean.

**Por qué no se cierra aquí:** haría falta que el modelo de nombres resuelva una instanciación
genérica en posición de **expresión** y que el modelo genérico la monomorfice desde ahí. Es trabajo
del compilador y de otro bloque; meterlo dentro del Bloque 3 sería arrastrar un cambio profundo del
frontend detrás de un tipo de biblioteca.

**Consecuencia visible en la API:** las fábricas viven en `Spans` y `Optionals`, clases no
genéricas, en vez de dentro de los structs. `Spans.Of<int>(items)` en lugar de
`Span<int>.Of(items)`. Cuando el ID se cierre, se pueden mudar sin tocar nada más.

### ID 132 — dos métodos genéricos con el mismo nombre y la misma aridad de tipos no se sobrecargan

```
duplicate generic template identity 'NativeSharp.Core.Spans.Of``1'
```

La identidad de una plantilla genérica es **nombre + número de parámetros de tipo**, sin la lista
de parámetros. Por eso el tramo se llama `OfRange` y no `Of`. Es un nombre peor y está escrito así
a propósito en vez de escondido.

### ID 130 — `uwtable` se pide con valor 0, que significa «sin tabla»

Encontrado montando el arnés, y medido aparte con la propia `LlvmCApi.h` del proyecto:

```cpp
applyUwtableAttribute: LLVMCreateEnumAttribute(context_, kind("uwtable"), 0)
```

En LLVM, `uwtable` es un atributo **entero** cuyo valor es un `UWTableKind`: **0 = None**,
1 = Sync, 2 = Async. El valor 0 no es «por defecto»: es *«sin tabla de desenrollado»*, justo lo
contrario de lo que el nombre del método promete.

Medido en LLVM 18, con un módulo de una función y nada más:

| valor | resultado de `LLVMPrintModuleToString` |
|---|---|
| `0` | **65 bytes y truncado**: `; Function Attrs: "Q\xef\xbd\x09\xef\xbdU]` — el módulo entero se corta en el primer atributo |
| `2` | 133 bytes, correcto: `; Function Attrs: uwtable` … `attributes #0 = { uwtable }` |

**No se toca en este bloque.** Cambiar los atributos de todas las funciones es una decisión del
proyecto con su propia medición en Windows —y en Windows x64 la tabla de desenrollado es lo que
SEH necesita, así que el efecto real puede no ser ninguno si el backend la añade igual—, no un
efecto colateral del Bloque 3.

---

## 7. La regresión que el contenedor cachó, y que habría llegado a Windows

Al añadir `borrowed` como modificador de declaración de tipo, el bucle de modificadores se lo comía
**siempre**. Pero `borrowed` ya significaba otra cosa desde Stage 50:

```csharp
borrowed Resource Observe(owned Resource source)   // funcion GLOBAL, valida desde Stage 50
```

Sin guarda, esa función **perdía su calificativo de retorno en el AST, en silencio**.
`Stage50_1OwnershipSyntaxTests` lo vio: *«borrowed return qualifier is preserved in the AST»*.

El arreglo es la misma forma de guarda que `partial` usa tres bloques más abajo, y por la misma
razón: **una palabra que significa dos cosas se desambigua mirando lo que la sigue**. `borrowed`
sólo se consume como modificador de tipo cuando lo que viene detrás es una declaración de tipo
(`struct`, y también `class`/`enum`/`union`/`delegate` **a propósito**, para poder dar el
diagnóstico que explica por qué ahí no es legal en vez de un «expected a global declaration» que no
ayuda a nadie).

---

## 8. Cómo se verificó, y el cambio de método que esto trajo

**El contenedor ahora compila y ejecuta el compilador entero salvo el enlazador.** Es lo más
importante que pasó en esta ronda, más que cualquiera de los cuatro SHR.

Los 74 `.cpp` del frontend (Lexer, Parser, AST, Sema, Compilation, Diagnostics, Frontend, Source,
Target) más `ProjectOopLlvmLowerer`, `LlvmModule` y `WindowsRuntimeBuilder` compilan con clang 18 y
enlazan contra `libLLVM-18`. Con dos apaños de versión que **no salen del contenedor**:
`LLVMConstStringInContext2` (llegó en LLVM 19; aquí se implementa sobre la de 18) y el `uwtable` del
ID 130, subido a 2 sólo para poder **leer** el IR.

Lo que eso permitió, y no era posible antes:

| capa | qué se comprobó | resultado |
|---|---|---|
| Modelo de cuerpos | los 171 `.nsproj` del árbol | 167 OK, 5 negativos intencionados |
| MIR | los mismos | igual |
| **LLVM IR + verificador** | los mismos | igual |
| Pruebas unitarias que sólo piden `nsc_frontend` | 14 binarios | **14/14**, 372 comprobaciones |
| IR concreto | `NextToken` no llama al asignador; los GEP de la operación núcleo | leído, no supuesto |

**El cambio de método:** hasta esta ronda, «compilar en el contenedor» significaba
`-fsyntax-only` sobre los `.cpp` y el parser real sobre los `.ns` — bueno para errores de sintaxis,
ciego para todo lo demás. Ahora significa **el frontend real, el MIR real y el IR real, verificado
por LLVM**. Los tres archivos nuevos de biblioteca y la fixture de aceptación compilaron **a la
primera** en Windows-menos-el-enlazador porque cada uno se probó aquí primero, incluidas las cuatro
formas prohibidas y las ocho permitidas.

Y siguió aplicándose la regla del cierre anterior: **se relee del disco justo antes de cada
escritura**. El árbol de trabajo se levantó de un snapshot y se sobrescribió con copias frescas de
los ocho archivos con fecha posterior; al terminar, un `diff -rq` contra ese mismo snapshot
enumeró exactamente los veinte archivos tocados y ninguno más.

---

## 9. Qué se tocó

**Compilador (SHR-013 + la operación núcleo):**

| archivo | qué |
|---|---|
| `src/Parser/Parser.cpp` · `include/.../Parser.h` | `borrowed struct`: modificador, guarda de ambigüedad, marca en el texto auxiliar |
| `src/Frontend/ProjectMemberModel.cpp` | **R1** — campo de tipo vista rechazado, con la excepción vista-en-vista |
| `include/.../ProjectMethodBodyModel.h` · `src/Frontend/ProjectMethodBodyModel.cpp` | Los tres predicados de vista, el análisis de enraizamiento, **R2**, **R3**, y el reconocimiento de `String.CharacterPointer` |
| `include/.../ProjectOopHirModel.h` · `src/Frontend/ProjectOopHirModel.cpp` | Nodo HIR de la operación núcleo |
| `include/.../ProjectOopMirModel.h` · `src/Frontend/ProjectOopMirModel.cpp` | Nodo MIR |
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | La bajada: `lowerArrayElementAddress(node, true)` — **sin aritmética nueva** |

**Biblioteca:**

| archivo | qué |
|---|---|
| `lib/NativeSharp.Core/StringView.ns` | **nuevo**, 325 líneas, 23 miembros públicos |
| `lib/NativeSharp.Core/Span.ns` | **nuevo**, 375 líneas — `Span<T>`, `ReadOnlySpan<T>` y `Spans` |
| `lib/NativeSharp.Core/Optional.ns` | **nuevo**, 115 líneas — `Optional<T>` y `Optionals` |
| `lib/NativeSharp.Core/String.ns` | `+1` método: `CharacterPointer`, operación núcleo |

**Pruebas:**

| archivo | qué |
|---|---|
| `tests/cc1/c23_views_and_lifetimes/` | **nueva** fixture de aceptación, nueve bloques, salida 42 |
| `tests/unit/Stage58ViewLifetimesTests.cpp` | **nueva**, 31 comprobaciones: 12 formas prohibidas con su diagnóstico exacto y 8 permitidas |
| `CMakeLists.txt` | registro de `nsc.stage58.view-lifetimes`, pegado al 50.7 |
| `tests/unit/Cc1CorrectnessTests.cpp` · `run_cases.py` | registro de c23 |

**No tocado, y es deliberado:** `lib/NativeSharp.Core/Number.ns`. La tentación era darle
sobrecargas sobre `StringView`, y sería duplicar unas 350 líneas del núcleo de parsing en dos
copias que pueden separarse. **No hace falta:** el núcleo del SHR-024 ya trabaja sobre
`(texto, ref índice)`, así que un lexer lee los números **en sitio sobre la misma cadena** y guarda
los demás lexemas como vistas. Las dos mitades del Bloque 2 y del Bloque 3 componen sin una línea
duplicada, y la fixture lo enseña.

---

## 10. Lo que sigue

**Bloque 4 — sistema operativo y binarios.** Filesystem (SHR-031, y el doc 81 ya midió que el 80 %
es manipulación de rutas: texto, no sistema de archivos), `File.ReadAllBytes`/`WriteAllBytes`
(SHR-028/029/030), `ByteBuffer`/`BinaryReader`/`BinaryWriter` (SHR-025/026/027), **`Process.Run`**
(SHR-032, sin el cual `nsc-next` no enlaza) y `NativeLibrary` (SHR-035).

Depende de este bloque, y ya tiene de qué agarrarse: toda la E/S binaria toma `Span<byte>`.

**Abiertos y anotados, no olvidados:** ID 129 (enums por nombre), **ID 130** (`uwtable`),
**ID 131** (miembro estático de genérico cerrado), **ID 132** (sobrecarga de métodos genéricos). Y
siguen siendo decisiones tuyas, no de ingeniería: ID 87 (`volatile`), SHR-063 (condicionales de
compilación), ID 125 (UTF-8), ID 124 (probablemente falso).
