# Trazas didácticas compiler→library — Stage 52 (P2 #15)

Cuatro trazas paso a paso, del código fuente NativeSharp real hasta el binario ejecutado, para
las cuatro piezas que el checklist de cierre pedía documentar así: `default(T)`, `move(owned T)`,
`Dictionary<int,Payload>` (instanciación genérica sobre un tipo referencia), y
`PriorityQueue<TElement,TPriority>`. Cada línea de código citada es real, con su archivo y número
de línea — no reconstruida de memoria.

## 1. `default(T)` — de la palabra clave al bit cero

### 1.1 Por qué existe

`List<T>.Find(Predicate<T> match)` necesita devolver "nada encontrado" sin una excepción, igual
que el `default(T)` real de C#. Antes de Stage 52 la palabra `default` no existía en ningún punto
del pipeline.

### 1.2 Lexer — `include/nativesharp/Lexer/TokenKind.h:60`, `src/Lexer/Lexer.cpp:97`

```cpp
{"default", TokenKind::KwDefault},
```

Una entrada más en la tabla de keywords — `default` deja de ser un identificador válido de
usuario en cualquier posición, en todo el lenguaje, desde este punto.

### 1.3 Parser — `src/Parser/Parser.cpp:4129`

```cpp
if (at(TokenKind::KwDefault))
```

Se agrega justo después del manejo de `sizeof`, con la misma forma sintáctica: `default(Tipo)` —
sin ambigüedad posible porque `default` siempre toma un tipo entre paréntesis, nunca una
expresión.

### 1.4 AST — nuevo nodo `DefaultValueExpression`

`include/nativesharp/AST/AstNodeKind.h` y `src/AST/AstNodeKind.cpp` ganan el caso — un nodo con
un único hijo: el `TypeSyntax` parseado entre paréntesis.

### 1.5 Semántica — `ProjectMethodBodyModel.cpp`

Nuevo caso en `analyzeExpression`: resuelve el tipo vía `canonicalTypeKey`, y rechaza
`builtin:void` con un diagnóstico explícito (`default(void)` no tiene sentido — no hay "el valor
cero de nada").

### 1.6 HIR — `src/Frontend/ProjectOopHirModel.cpp:1496`

```cpp
return makeNode(ProjectOopHirNodeKind::DefaultValue, expression, type->typeKey);
```

El nodo HIR ya lleva el `typeKey` resuelto colgando — nada de lógica de tipo queda para las capas
de abajo, solo "necesito el valor default de este typeKey exacto".

### 1.7 MIR — `src/Frontend/ProjectOopMirModel.cpp:420` y `:835`

```cpp
case ProjectOopHirNodeKind::DefaultValue: result->kind = ProjectOopMirNodeKind::DefaultValue; break;
```

Traducción 1:1 HIR→MIR, sin transformación — `default(T)` no necesita bajar a nada más simple, ya
es una operación atómica.

### 1.8 LLVM — `src/LLVM/ProjectOopLlvmLowerer.cpp:2719-2728`

```cpp
case frontend::ProjectOopMirNodeKind::DefaultValue:
{
    // Stage 52: default(T) -- LLVMConstNull is uniformly correct for every closed T
    // here: an integer/float/bool/char primitive type gives numeric zero, a pointer
    // type (class, string, delegate) gives a null pointer, and a struct (value) type
    // gives an all-zero-fields aggregate -- exactly "the value every field of T would
    // have if never assigned", with no per-primitive special-casing needed.
    LLVMTypeRef type = llvmType(node->typeKey, error);
    return type != nullptr ? LLVMConstNull(type) : nullptr;
}
```

Un solo `LLVMConstNull` universal — la elegancia de este fix es que LLVM ya tiene una
representación de "todo en cero" para CUALQUIER tipo (entero, puntero, o struct agregado), así
que `default(T)` no necesita ni un `switch` sobre la forma de T.

### 1.9 Verificación real

`Stage52GenericCollectionsCompilerFixesTests.cpp`, instanciado con T=`int` (da `0`) y T=`Payload`
(da `null`) — ambos verificados hasta objeto COFF real, y transitivamente por la ejecución real
de `Stage52Collections.exe` (checksum 149), que llama `List<T>.Find` con y sin match.

## 2. `move(owned T)` — de la sintaxis al `MoveValue` de LLVM

### 2.1 Por qué es distinto de `move()` sobre un `raii` local

Antes de Stage 52, `move()` solo se usaba sobre un local `raii` (siempre tipo referencia). Stage
52 necesita `move()` también sobre un PARÁMETRO `owned T` cuando T puede ser un tipo valor
(`int`, `bool`, un struct) — el repro real que lo destapó es
`LinkedList<int>.AddFirst`'s `created[0].SetValue(move(value))`, con `value: owned int`.

### 2.2 Dos bugs en dos capas distintas (Fix 10a, 10b)

**Resolución de overload** (`ProjectMethodBodyModel.cpp`, loop de viabilidad de
`CallExpression`): la cadena original era `if (Owned && parameterOwnable) ... else if (Borrowed
&& parameterOwnable) ... else /* rechaza */`. Un parámetro owned de tipo VALOR
(`parameterOwnable == false`) caía siempre en el `else` de rechazo. Corregido revisando
`!parameterOwnable` PRIMERO — un T valor no necesita ninguna validación de ownership, se
bypasea por completo.

**Lowering LLVM** — `src/LLVM/ProjectOopLlvmLowerer.cpp:2597-2638` (`MoveValue`):

```cpp
// Stage 52 fix: this unconditionally treated the storage slot as a pointer,
// which was correct for every move() source Stage 49/50 ever produced (always a
// reference-typed RAII owner) but breaks once a generic T can monomorphize a
// move()'d local/parameter to a plain value type (int, bool, a struct, ...) --
// e.g. LinkedList<int>.AddFirst's "move(value)" on an owned int parameter, which
// loaded the i32 storage slot AS a pointer and failed LLVM verification with a
// call-parameter type mismatch. Load/store using the value's REAL type instead,
// and only null out the slot for an actually reference-like source ...
LLVMTypeRef valueType = llvmType(node->typeKey, error);
if (valueType == nullptr) return nullptr;
LLVMValueRef value = LLVMBuildLoad2(builder_, valueType, storage, loadName);
if (node->booleanValue)
{
    LLVMBuildStore(builder_, LLVMConstNull(valueType), storage);
}
return value;
```

El `node->booleanValue` aquí es la bandera `elementIsReferenceLike` — calculada ahora para TODA
forma de fuente de `move()` (local, parámetro, elemento de arreglo), no solo elemento de arreglo
como antes. Para un T valor, `elementIsReferenceLike` es `false`: se carga con el tipo real
(`i32` para `int`, no `ptr`), y NO se anula el slot de origen después (no hay ownership que
proteger en un valor, y anular un `int` "movido" incorrectamente ocultaría lecturas legítimas del
mismo valor que el código todavía puede hacer más adelante).

### 2.3 Dónde se usa en la librería real

```csharp
// List.ns, EnsureCapacity -- el patrón que TODO contenedor con arreglo propio repite:
grown[i] = move(items[i]);   // T referencia: transferencia real. T valor: copia simple, sin costo.
```

### 2.4 Verificación real

`testMoveOfOwnedParameterAsCallArgumentForBothValueAndReferenceT`
(`Stage52GenericCollectionsCompilerFixesTests.cpp`), instanciado con T=`Payload` (referencia) y
T=`int` (valor), hasta objeto COFF real. Y — crucialmente, porque es justo el patrón que el bug
de doble-free de la sección 10.2 de `52_nativesharp_generic_collections.md` rompía en runtime —
verificado también por EJECUCIÓN REAL bajo Wine 9.0 en cada uno de los 14 sitios corregidos
(`List.Add`, `Queue.Enqueue`, `Stack.Push`, etc.), no solo estructuralmente.

## 3. `Dictionary<int, Payload>` — instanciación genérica con T valor Y T referencia simultáneos

### 3.1 Por qué es la prueba de fuego de "generics real, no template C++ superficial"

`Dictionary<TKey, TValue>` es la única clase de Stage 52 donde el checklist de Alberto exige
instanciar con `TKey` valor (`int`, la clave hasheable típica) Y `TValue` referencia (`Payload`,
un objeto real con su propio ciclo de vida `owned`) en el MISMO tipo cerrado, ejercitando ambos
lados de la monomorfización a la vez — a diferencia de `List<T>`, donde T juega un solo rol.

### 3.2 El límite de ABI que casi rompe el diseño — `Dictionary.ns:217-223`

```csharp
public bool TryGetValue(TKey key, TValue[] result)
{
    int index = FindEntry(key);
    if (index < 0) { return false; }
    result[0] = entryValues[index];
    return true;
}
```

El primer diseño devolvía un struct `{bool Found; TValue Value;}` por valor — rechazado por el
compilador (`Stage 40 Windows x64 direct struct value ABI supports only 1, 2, 4 or 8-byte
aggregates`) en cuanto `TValue=Payload` empuja el struct a 16 bytes. Aislado con un repro SIN
generics (`TwoLongs{long A; long B;}`, 16 bytes, cero referencias) para confirmar que el límite
es de Stage 40 (ABI de structs), no de generics ni de ownership. Reescrito con el idioma de
arreglo-de-salida — `result[0] = ...` en vez de `return struct` — que sortea el límite sin tocar
el ABI: un arreglo de un elemento es memoria direccionable normal, nunca un valor de primera
clase que tenga que caber en registros.

### 3.3 Storage interno — monomorfización uniforme

```text
private owned TKey[] entryKeys;      // TKey=int: arreglo de valores planos, sin ceremonia
private owned TValue[] entryValues;  // TValue=Payload: arreglo owned de punteros con move()
```

El mismo campo `owned TValue[]` funciona sin ningún `if` especial para T valor o T referencia —
la diferencia (transferencia real vs. copia simple) vive enteramente en cómo `move()` lowera
según `elementIsReferenceLike` (sección 2 arriba), no en el diseño de la clase.

### 3.4 Verificación real

```bash
nsc --check-project-syntax /tmp/ns52_libtest/Stage52Test.nsproj
nsc --check-project-method-bodies /tmp/ns52_libtest/Stage52Test.nsproj
nsc --check-project-oop-hir /tmp/ns52_libtest/Stage52Test.nsproj
nsc --check-project-oop-mir /tmp/ns52_libtest/Stage52Test.nsproj
nsc --verify-project-oop-llvm /tmp/ns52_libtest/Stage52Test.nsproj
nsc --emit-project-oop-obj /tmp/ns52_libtest/Stage52Test.nsproj -o out.o
# los seis: ok, con Dictionary<int,int> Y Dictionary<int,Payload> en el mismo proyecto
```

Más ejecución real transitiva vía `Stage52Collections.exe` (checksum 149) y — el bug de
doble-free de la sección 10.2 vivía exactamente en este tipo de storage — el fix ya verificado
por ejecución real bajo Wine, no solo por los seis pasos estructurales de arriba.

## 4. `PriorityQueue<TElement, TPriority>` — el binary heap con dos arreglos paralelos

### 4.1 Diseño — el mismo patrón de índices que la implementación interna de C#

Dos arreglos `owned` paralelos, `elements`/`priorities`, con aritmética de índices `2i+1`/`2i+2`
(hijo izquierdo/derecho) y `(i-1)/2` (padre) — exactamente como el `PriorityQueue<TElement,
TPriority>` real de .NET.

### 4.2 `Dequeue()` — el patrón de remoción de raíz sin local `owned` intermedio, `PriorityQueue.ns:184-198`

```csharp
public owned TElement Dequeue()
{
    if (count == 0)
    {
        throw new InvalidOperationException();
    }
    SwapAt(0, count - 1);
    delete priorities[count - 1];
    count = count - 1;
    if (count > 0)
    {
        SiftDown(0);
    }
    return move(elements[count]);
}
```

El patrón real de un binary heap es "sacar la raíz, poner el último elemento en su lugar,
hundirlo (`SiftDown`) hasta restaurar la propiedad de heap" — pero NativeSharp no permite un
local `owned` intermedio para "guardar la raíz mientras se reorganiza el arreglo" cuando T no es
una clase (no hay dónde declarar `owned TElement root = move(elements[0]);` de forma legal para
un T genérico no-clase todavía sin instanciar). La solución real, específica de este compilador:
**`SwapAt(0, count-1)` primero** (la raíz que hay que devolver termina en la posición
`count-1`, la que está a punto de "salir" del arreglo lógico al decrementar `count`), y recién
ENTONCES `move(elements[count])` — el elemento que hay que devolver, ahora en una posición que
el arreglo lógico ya no considera "viva", así que moverlo desde ahí no interfiere con el
`SiftDown` que ya corrió sobre las posiciones `0..count-1` reales.

### 4.3 `TPriority` no se devuelve, solo se descarta — `delete priorities[count - 1]`

`TPriority` casi siempre es un tipo valor (`int`, `float` — una prioridad numérica), así que
`delete` sobre un elemento de arreglo de tipo valor es un no-op estructural (ver la tabla de
descubrimientos en `52_nativesharp_generic_collections.md` sección 3) — se mantiene por
simetría/claridad de intención, no porque libere memoria de verdad en el caso común.

### 4.4 Enumerador — agregado en la ronda de foreach (52.x.2), no en el cierre original

`PriorityQueueEnumerator.ns` — a diferencia de `List<T>`, iterar un `PriorityQueue<TElement,
TPriority>` con `foreach` NO da el orden de prioridad (recorre el arreglo del heap tal cual está
almacenado internamente, igual que el `PriorityQueue<TElement,TPriority>` real de C# documenta
explícitamente: "no se garantiza ningún orden particular"). Quien necesita orden de prioridad
real debe `Dequeue()` en loop, no `foreach`.

### 4.5 Verificación real

`nsc.stage52-6.foreach-collections-enumerators` (16/16, ejecución real, orden verificado contra
cálculo a mano por contenedor). Uso real de punta a punta en el ejemplo académico
`tutorial_23_huffman_priority_queue` (paquete de 23 tutoriales) — un `PriorityQueue<int, int>`
guardando ÍNDICES de un pool de nodos (no referencias de objeto directamente) como `TElement`,
precisamente para no necesitar ningún patrón de ownership de clase sobre el elemento en sí.
