# Stage 56 / C7 — Definite assignment e inicializador agregado

**Origen:** la corrección de alcance de Alberto después de C6 — *"ese tipo de cosas son las que
quiero resolver de fondo"*. C6 había cerrado con una lista de cosas que él tendría que ajustar al
portar; eso es exactamente lo contrario de lo que pide. Esta ronda va al fondo del agujero más
grande que le quedaba al lenguaje, y no deja lista de pendientes para nadie.

**Resultado:** `ctest` **236/236**, build limpio con `-DNSC_WARNINGS_AS_ERRORS=ON`, **128 proyectos
de ejemplo recompilados sin un solo falso positivo**, y un bug silencioso de C6 encontrado y
arreglado por el camino.

---

## 1. El agujero

```
V v;
return v.a;     // devolvía lo que hubiera en la pila
```

`alloca` y nada más. Sin store, sin ceros, sin diagnóstico. En **cualquier `-O`**, y para
**cualquier tipo de local**: un `int`, un `int*`, una referencia de clase, un `T[]`.

No era una limitación de agregados. Era la lectura de basura, silenciosa, por omisión, para todo
el lenguaje. Es la clase de defecto más cara que puede tener un compilador: no falla, responde
mal.

Medido antes de tocar nada:

| Programa | Antes de C7 |
|---|---|
| `int x; return x;` | compila; devuelve basura |
| `int[] a; a[0] = 1;` | compila; deref de null en runtime |
| `int* p; *p = 1;` | compila; escritura a dirección arbitraria |
| `Node n; return n.v;` | compila; deref de null en runtime |
| `V v; v.a = 1; use(v.b)` | compila; `v.b` es basura |

---

## 2. La respuesta: dos mitades que sólo sirven juntas

### Mitad 1 — Almacenamiento definido (backend)

Toda local declarada sin inicializador se pone en **cero** en el punto donde se ejecuta su
declaración.

En la declaración, **no** junto al `alloca` izado. Esa distinción no es cosmética: una local
declarada dentro de un bucle es una variable nueva en cada vuelta y su valor por omisión tiene que
restablecerse en cada una. Los `alloca` se izan al bloque de entrada para que `mem2reg` los
promueva; la semántica vive en la declaración.

```llvm
%local0.addr = alloca [12 x i8], align 4
...
store [12 x i8] zeroinitializer, ptr %local0.addr, align 4   ; en la declaración
```

Con esto NativeSharp **no tiene lecturas no inicializadas en ninguna parte**, en ningún nivel de
optimización.

**Y no cuesta nada donde no hace falta.** En un programa que el análisis de la mitad 2 acepta,
cada lectura está dominada por un store real, así que el store de ceros es código muerto y SROA lo
borra. Medido: el ejemplo empaquetado entero, con structs, arreglos fijos, bucles y `out`, se
pliega en `-O2` a

```
xorl %eax, %eax
retq
```

Se paga sólo donde el programa **depende** del valor por omisión — un agregado llenado campo por
campo, que es justo el idioma de C que la mitad 2 permite a propósito.

### Mitad 2 — Definite assignment (front end)

Análisis MUST sensible a caminos, sobre la misma maquinaria de flujo que ya usaba el verificador de
ownership (`OwnershipFlowSnapshot`, `captureOwnershipFlow`, `joinOwnershipFlow`). El conjunto de
locales asignadas viaja con ese snapshot y se une por **INTERSECCIÓN** en cada confluencia — es el
único conjunto MUST de la estructura; todos los demás son MAY.

Leer una local **escalar, puntero, referencia o delegate** por un camino donde nada la escribió es
**error de compilación**.

```
error: use of unassigned local variable 'x'
```

Lo que caza:

| Programa | Diagnóstico |
|---|---|
| `int x; return x;` | sí |
| `int x; if (c) { x = 1; } return x;` | sí — la unión es intersección |
| `int x; while (c) { x = i; } return x;` | sí — el bucle puede correr cero veces |
| `int x; x += 1;` | sí — la asignación compuesta lee primero |
| `int x; f(ref x);` | sí — `ref` da derecho a leer |
| `int[] a; a[0] = 1;` | sí — **deref de una referencia sin valor**, no una asignación de `a` |
| `int* p; *p = 1;` | sí — ídem |
| `Node n; n.v` | sí |
| `int x; f(out x); use(x);` | **no** — `out` sí asigna |
| `catch (E e)`, `foreach (T x in ...)`, parámetro de lambda | **no** — los liga el constructo |

Las tres filas de deref son la razón por la que la regla ingenua *"cualquier escritura a través de
la raíz la marca asignada"* habría sido **incorrecta**. Escribir a través de un arreglo dinámico,
un puntero o una referencia sin valor no asigna esa local: la **desreferencia**. Tratar el store
como asignación habría tapado tres bugs reales.

### Por qué la mitad 1 es lo que hace asequible a la mitad 2

Los **agregados no se reportan nunca**. Un `struct` por valor y un arreglo fijo `T[N]` quedan
fuera del análisis.

Eso no es una debilidad, es la regla del lenguaje que pidió Alberto: *"struct si no lleva métodos
se comporta como en C"*. `Color c; c.tag = 1;` es el idioma de C, y llenar un agregado por partes a
través de ramas y bucles es exactamente la forma que un análisis MUST no puede seguir sin rechazar
programas correctos:

```
Shape shape;
if (ancho) { shape.cx = 40; } else { shape.cy = 40; }
while (i < 2) { shape.cy = shape.cy + 1; i = i + 1; }
return shape.cx + shape.cy + shape.depth;   // depth nunca se escribe
```

Un análisis estricto rechaza esto. C lo acepta y devuelve basura en `depth`. **NativeSharp lo
acepta y devuelve cero en `depth`**, de forma determinista. Esa es la mitad 1 haciendo su trabajo:
es C, pero definido.

Rigor donde caza bugs; comportamiento definido en todas partes.

### La sutileza del snapshot

Un snapshot no puede decir nada de una local que **no existía** cuando se tomó, y restaurarlo no
debe des-asignarla. Lo forzó el caso de `catch`: `analyzeTry` captura su estado de entrada a los
handlers, **después** declara la local de excepción (que liga el propio handler, así que nace
asignada), y **después** restaura. Una asignación simple del conjunto habría tirado esa ligadura y
reportado cada uso de una excepción atrapada como lectura de variable sin asignar.

Los ordinales de local sólo crecen, así que "declarada después de este snapshot" es exactamente
"ordinal ≥ su `localCount`". Arrastrar esos bits no puede tapar un error real: por alcance léxico,
una local declarada después del punto del snapshot no es nombrable en el camino que se restaura.

---

## 3. Inicializador agregado

La forma explícita de lo que el valor por omisión da implícitamente. Es la forma de **C**, a
propósito:

```
Vec2 full    = {3, 4};
Vec2 partial = {7};              // y lo que la lista no alcanza conserva el cero
Vec2 empty   = {};               // "todo en cero", escrito
Rect nested  = { {1,2}, {8,9}, 5 };
int[4] cells = {10, 20,};        // coma final
int[3] scaled = {w, w*2, w*3};   // los valores no tienen por qué ser constantes
```

Y **también en campos**, con el mismo binder que las locales, para que las dos formas no puedan
discrepar sobre aridad, tipos de elemento ni relleno parcial:

```
class Holder { public Vec2 offset = {5, 6}; }
static Vec2 shared = {30, 1};
```

El anidamiento se resuelve en el HIR, contra el mismo `StructValueLayout` del que el generador de
headers C emite offsets — un inicializador y un host C no pueden discrepar sobre qué bytes ocupa un
campo. Lo que rechaza:

| Programa | Diagnóstico |
|---|---|
| `One v = {1, 2};` (un solo campo) | `has 2 values but the type takes 1` |
| `One v = {"x"};` | `cannot initialize element 0 ... from 'string-literal'` |
| `Node n = {1};` (clase) | `requires a struct or fixed-array target` |
| `int[] a = {1,2};` (arreglo dinámico) | ídem — es una referencia, no almacenamiento |
| `var v = {1,2};` | `'var' cannot infer a type from an aggregate initializer` |

`var` no puede porque un inicializador con llaves **no tiene tipo propio**: se llena *contra* un
tipo declarado, exactamente por la misma razón por la que `var` no puede ligar una lambda pelada.

---

## 4. El bug de C6 que encontró esta ronda

```
Big g = new Big(1.0f);    // Big = 20 bytes -> agregado INDIRECTO en el ABI de Win64
```

`lowerValueConstruction` cortocircuitaba en `isIndirectStructTypeKey` y devolvía la **dirección**
del valor construido. La declaración de la local hacía entonces:

```llvm
store ptr %struct.new, ptr %local0.addr   ; guarda un PUNTERO en los 20 bytes de la local
```

y cada lectura de campo posterior leía **bits de puntero como floats**. Valores mal, sin
diagnóstico, y el verificador de IR sin nada que objetar: `store ptr, ptr` está perfectamente bien
formado.

El invariante que restaura el arreglo, y que ya estaba escrito en el compilador — en el comentario
del caso `TemporaryAddress`, de cuando el mismo error se cometió ahí y el verificador sí lo cazó:
**una expresión de tipo agregado entrega en este nivel el CARRIER CARGADO, nunca un puntero**, que
es lo mismo que hace una llamada que devuelve uno después de escribir su slot `sret`.

Clavado con un test de regresión que además comprueba la forma del IR (`load [20 x i8]`, y la
ausencia de `store ptr %struct.new,`).

---

## 5. Qué se tocó

| Archivo | Qué |
|---|---|
| `include/.../ProjectMethodBodyModel.h`, `src/Frontend/ProjectMethodBodyModel.cpp` | el conjunto `assignedLocals_` en el snapshot de flujo, la unión por intersección, el `localCount` del snapshot, el chequeo en el único punto donde un nombre se vuelve lectura, las marcas de escritura (`=`, `out`, declaración con inicializador, `catch`/`foreach`/lambda), y el binder del inicializador agregado |
| `include/nativesharp/AST/AstNodeKind.h`, `src/AST/AstNodeKind.cpp` | `AggregateInitializerExpression` |
| `include/nativesharp/Parser/Parser.h`, `src/Parser/Parser.cpp` | `parseAggregateInitializer`, en declaraciones de local y de campo |
| `include/.../ProjectOopHirModel.h`, `src/Frontend/ProjectOopHirModel.cpp` | `AggregateInitializer` / `AggregateElement`, y el aplanado a offsets absolutos |
| `include/.../ProjectOopMirModel.h`, `src/Frontend/ProjectOopMirModel.cpp` | `AggregateInitialize` / `AggregateElementStore` |
| `include/.../ProjectObjectLayoutModel.h`, `src/Frontend/ProjectObjectLayoutModel.cpp` | `valueLayoutForTypeKey`, una sola regla de "cuánto mide uno de estos", compartida con el modelo MIR |
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | `emitDefaultLocalStorage`, `lowerAggregateInitializer`, y el arreglo del agregado indirecto en `lowerValueConstruction` |
| `tests/unit/Stage56C7DefiniteAssignmentTests.cpp` | 77 checks |
| `tests/interop_contract/c7_definite_assignment/` | caso contractual |
| `examples/project_stage56_c7/` | ejemplo empaquetado |

---

## 6. Cómo se verificó

- **`ctest` 236/236**, con `-DNSC_WARNINGS_AS_ERRORS=ON`.
- **128 proyectos** (`examples/`, `book_examples/`, `tests/interop_contract/`) recompilados a `-O0`
  y `-O2`: **cero** diagnósticos `use of unassigned local variable`. Ni un falso positivo.
- **Forma del IR**: el `zeroinitializer` está en la declaración, con la alineación real del
  agregado.
- **Código máquina**: dos escenarios comprueban que `-O2` pliega el cuerpo entero a un `mov eax,
  <n>; ret`. Es la afirmación más fuerte disponible — para plegarlo, el optimizador tuvo que
  demostrar cada comparación por separado — y es también la medida de que el puesta-en-cero no
  cuesta nada.
- **Ejecución real**: pendiente de Wine o Windows, como el resto de la matriz de ejecución de C5.
  Donde no hay Wine, la mitad de ejecución de cada escenario degrada a un *skip* explícito, nunca a
  un check verde que no verificó nada.
