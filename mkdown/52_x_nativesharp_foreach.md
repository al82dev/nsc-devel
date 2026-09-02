# 52.x — NativeSharp `foreach`

> **ACTUALIZACIÓN (ronda `STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md`)**: una revisión
> técnica dedicada a `foreach`, enviada después de esta entrega original, encontró cuatro gaps
> semánticos (higiene de temporales, fuente ownership-aware, elemento readonly, `const` discretado
> en silencio) y dos gaps de compatibilidad (`Current` debía ser propiedad no método;
> `GetEnumerator()` faltaba en el resto de Generic Collections). Todos corregidos -- ver
> `52_x_2_foreach_revision_tecnica_paso_a_paso.md` para la bitácora completa paso a paso. En
> particular: cada `Current()` que aparece más abajo en ESTE documento (secciones 3.3 y 7) refleja
> la forma ORIGINAL (método, con paréntesis) -- la forma actual, real, es `Current` como propiedad
> get-only, sin paréntesis. Se deja el texto original sin reescribir para preservar la bitácora
> histórica tal como se escribió en su momento; la actualización vive en el documento nuevo, no
> aquí, por la misma razón que `52_x_stage52_cierre_bug_owned_move.md` corrigió una sección
> equivocada de este mismo documento en vez de reescribirlo por completo.

## 1. Objetivo

Alberto pidió `foreach` de forma explícita y urgente ("de verdad, necesito el foreach"), con una instrucción de calidad igual de explícita: debe ser didáctico, cada cambio documentado y por qué, sin escatimar esfuerzo. Este documento es esa documentación.

`foreach` es, en la superficie, "una forma más de recorrer una colección". Por debajo toca casi todo lo que este compilador ya tiene: parser, análisis semántico, ownership/RAII, HIR, MIR, LLVM, y la librería de colecciones de Stage 52. La decisión de diseño más importante de esta entrega fue precisamente **no** tocar casi nada de eso — ver sección 2.

## 2. Decisión de diseño: `foreach` estructural, no `IEnumerable<T>`

C# real NO exige `IEnumerable<T>` para `foreach` — eso es un malentendido común. La especificación del lenguaje resuelve `foreach` en tiempo de compilación buscando, por lookup de miembros ordinario, un `GetEnumerator()` accesible sin argumentos que devuelva algo con `bool MoveNext()` y un `Current` de solo lectura — `IEnumerable<T>`/`IEnumerator<T>` son apenas una conveniencia estándar, nunca un requisito del lenguaje. Ese patrón estructural ("duck typing" en tiempo de compilación, sin reflection ni dispatch dinámico) es exactamente lo que NativeSharp puede soportar hoy, verificado contra el código real antes de decidir nada:

- `parseInterfaceDeclaration` nunca llama a `parseGenericParameterList` (a diferencia de `parseClassDeclaration`) — las interfaces de NativeSharp **no pueden llevar parámetros de tipo**. No hay forma de escribir `IEnumerable<T>`/`IEnumerator<T>` en este compilador, punto.
- Incluso un `IEnumerator` NO genérico necesitaría `object Current { get; }` para ser uniforme entre tipos de elemento — y NativeSharp no tiene un tipo `object` universal (ver el propio comentario de clase de `Comparer.ns`, que documenta la misma restricción bloqueando `Comparer<T>.Default`).
- Los constraints genéricos (`where T : ...`) ya están deliberadamente diferidos desde Stage 44 (`Parser.cpp`: "generic constraints are deferred beyond Stage 44; monomorphization currently requires unconstrained type parameters") — construir interfaces genéricas ahora sería una feature a medio construir, sin manera de expresar ni siquiera `where T : IComparable<T>`.

El precedente ya existente de `NativeSharp.Core.IDisposable`/`using` confirma por qué esto funciona para `Dispose()` (firma no genérica, trivial) pero no para `GetEnumerator()`/`Current()` (necesitan tipar por cada `T`): `disposableTypeOrdinal()`/`disposableContractMemberOrdinal()` en `ProjectMethodBodyModel.cpp` resuelven `IDisposable` vía `effectiveMethodMemberOrdinals` de una interfaz — exactamente la clase de mecanismo que `GetEnumerator()` no podría usar sin generics en interfaces.

**Conclusión, aprobada explícitamente por Alberto ("continua como dices entonces, confio en tu juicio")**: `foreach` se resuelve por patrón estructural en tiempo de compilación, sin ninguna interfaz nueva, con dos formas:

1. **Modo arreglo** — indexación + `.Length`, para `T[]`.
2. **Modo enumerador** — `GetEnumerator()`/`MoveNext()`/`Current()`, resuelto por el mismo lookup de miembros que cualquier otra llamada `.Metodo()`.

## 3. Arquitectura: desugaring en tiempo de parseo, cero passes nuevos

La decisión de implementación que más ahorra riesgo: `foreach (T x in expr) { body }` se desazucara **enteramente en el parser**, a nodos AST ORDINARIOS que ya existen y ya están probados (`VariableDeclarationStatement`, `WhileStatement`/`ForStatement`, `MemberExpression`, `CallExpression`, `IndexExpression`, `BinaryExpression`, `AssignmentExpression`, `BlockStatement`). Ni HIR, ni MIR, ni el lowering a LLVM necesitan un nodo nuevo para el control de flujo de `foreach` — heredan gratis TODA la maquinaria de `while`/`for` ya construida (loop stack, RAII a través de back-edges, cleanup regions, `break`/`continue`, SEH funclets).

El parser no puede saber en tiempo de parseo si `expr` es un arreglo o algo con `GetEnumerator()` — esa información sólo existe después del análisis semántico. Por eso `parseForeachStatement` construye **los dos candidatos incondicionalmente** (construir un AST no necesita tipos, sólo forma), y los cuelga de un nuevo nodo `ForeachStatement`:

- `first()` — `var __foreach_source = <expr>;`: evalúa la colección UNA sola vez, sin importar qué candidato se use después.
- `second()` — candidato modo arreglo.
- `third()` — candidato modo enumerador.

`ProjectMethodBodyModel::analyzeForeach` es el ÚNICO lugar que decide cuál candidato es "real": analiza `first()` (resolviendo el tipo real de `__foreach_source` exactamente como cualquier otro `var`), y despacha por `typeKey.ends_with("[]")` (formato de arreglo confirmado en `ProjectGenericModel.cpp: result += "[]";`) a `analyzeBlock` sobre el candidato correspondiente. El candidato descartado nunca se analiza — no registra locals, no genera diagnósticos, nunca llega a HIR/MIR/LLVM; es simplemente memoria de arena sin referenciar.

`ProjectOopHirModel::lowerStatement`'s nuevo `case ForeachStatement` re-deriva la MISMA decisión de forma independiente (no hay canal lateral que la transporte desde semántica): resuelve el tipo de `__foreach_source` vía `localSymbol()`/`localBySyntax_` (exactamente el mecanismo ya existente que conecta un local semánticamente resuelto de vuelta a su nodo AST) y aplica el mismo chequeo de sufijo `"[]"`. Ambas pasadas leen el mismo tipo ya resuelto, así que siempre coinciden.

### 3.1 Reuso de `var` y `raii var`

Los locals ocultos (`__foreach_source`, `__foreach_index`/`__foreach_enumerator`) usan inferencia `var` (feature ya construida en esta misma sesión, Stage 52.2) para nunca tener que nombrar su tipo real en tiempo de parseo. El enumerador se declara como `raii var __foreach_enumerator = ...` — RAII (Stage 49) hace exactamente el trabajo del `Dispose()` implícito que C# genera en un `finally` de `foreach`: el enumerador se destruye automáticamente en CUALQUIER camino de salida (fin normal, `break`, `return`, excepción), sin que `foreach` necesite ninguna maquinaria de `IDisposable`/`using` propia.

### 3.2 Modo arreglo — por qué es un `ForStatement`, no un `while`

```csharp
for (int __foreach_index = 0; __foreach_index < __foreach_source.Length; __foreach_index = __foreach_index + 1)
{
    T <name> = __foreach_source[__foreach_index];
    { body }
}
```

Esto (ver sección 6.1) es la versión CORREGIDA — el primer intento usaba un `while` con el incremento como sentencia final dentro del cuerpo, y tenía un bug real de `continue`.

### 3.3 Modo enumerador

```csharp
raii var __foreach_enumerator = __foreach_source.GetEnumerator();
while (__foreach_enumerator.MoveNext())
{
    T <name> = __foreach_enumerator.Current();
    { body }
}
```

Aquí SÍ es un `while` (correctamente): la re-evaluación de la condición, `MoveNext()`, ES el paso de "avanzar" — no hay una sentencia de incremento aparte que un `continue` pueda saltarse.

## 4. Implementación por capa

- **Lexer** (`TokenKind.h`/`.cpp`, `Lexer.cpp`): dos keywords nuevas, `foreach`/`in`. Verificado antes de agregarlas que ninguna palabra se usaba como identificador en `lib/`/`examples/`.
- **AST** (`AstNodeKind.h`/`.cpp`): un nodo nuevo, `ForeachStatement`.
- **Parser** (`Parser.h`/`.cpp`): `parseForeachStatement()` + 9 funciones auxiliares de fabricación de nodos (`makeIdentifierExpression`, `makeMemberExpression`, `makeZeroArgCallExpression`, `makeIndexExpression`, `makeBinaryExpression`, `makeAssignmentExpression`, `makeExpressionStatement`, `makeVariableDeclaration`, `makeIntegerLiteralExpression`), verificadas una por una contra la forma real de construcción de nodos que ya usa el resto del parser. Despacho agregado en `parseStatement()`.
- **Semántica** (`ProjectMethodBodyModel.h`/`.cpp`): `MethodScopeKind::Foreach` + `analyzeForeach()`. Cero lógica de control de flujo nueva — delega en `analyzeVariable`/`analyzeBlock` ya existentes, que a su vez ya saben manejar `WhileStatement`/`ForStatement` con toda su disciplina de ownership/RAII/back-edges.
- **HIR** (`ProjectOopHirModel.cpp`): un `case ForeachStatement` que re-deriva la decisión arreglo-vs-enumerador y delega en `lowerStatement`/`lowerBlock` ya existentes. Ningún nodo HIR nuevo.
- **MIR** (`ProjectOopMirModel.cpp`): **cero cambios**. El mapeo HIR→MIR es 1:1 por kind, y `Block`/`LocalDeclaration`/`While`/`For` ya estaban mapeados.
- **LLVM** (`ProjectOopLlvmLowerer.cpp`): **cero cambios**. `lowerWhile`/`lowerFor` ya existían y ya eran correctos.

Verificado con `--check-project-oop-mir`/`--verify-project-oop-llvm` real después de cada capa, no asumido.

## 5. Bug real encontrado #1: `continue` en modo arreglo (infinite loop)

El primer diseño del candidato de modo arreglo usaba un `while` con el incremento como última sentencia del cuerpo:

```csharp
int __foreach_index = 0;
while (__foreach_index < __foreach_source.Length)
{
    T <name> = __foreach_source[__foreach_index];
    { body }                          // si body ejecuta `continue`...
    __foreach_index = __foreach_index + 1;  // ...esta línea NUNCA corre
}
```

`continue` dentro de un `WhileStatement` salta directo a la re-evaluación de la condición — correctamente, es la semántica de `while`. Pero eso significa que cualquier `foreach` con un `continue` en el cuerpo se queda en el MISMO índice para siempre: loop infinito. Encontrado con un test real (`foreach` sobre `int[6]` saltando pares con `continue`), reproducido con `nsc --emit-project-oop-exe` + ejecución bajo Wine (el binario colgó, `timeout` lo mató con exit 124).

**Corrección**: el candidato de modo arreglo se reconstruyó como un `ForStatement` real (init/condición/incremento/cuerpo en sus cuatro slots dedicados), reusando el MISMO nodo que un `for` escrito a mano ya usa. `ProjectOopLlvmLowerer::lowerFor` ya empuja `{endBlock, incBlock}` al `loopStack_` — el `continue` de un `ForStatement` YA salta al bloque de incremento, no a la condición. Cero código de control de flujo nuevo; el bug desapareció por construcción al reusar el nodo correcto en vez de una sentencia final tacked-on. Verificado: el mismo test ahora corre limpio bajo Wine, exit code correcto (9 = 1+3+5, la suma de impares).

## 6. Bug real encontrado #2: `owned` de tipo valor corrompe stack en llamadas anidadas

Este NO es un bug de `foreach` — es un bug preexistente del compilador, ya identificado por escrito en `WINE_EXECUTION_FINDINGS_STAGE52.md` durante el cierre de Stage 52, pero **nunca corregido en el código fuente** (el documento lo deja explícito: "se recomienda tratarlo como blocker de alta prioridad... trabajo fuera del alcance de este cierre"). Se encontró de nuevo, en vivo, al probar `foreach` sobre `List<int>` real:

```csharp
List<int> numbers = new List<int>();
numbers.Add(10);
numbers.Add(20);
int count = numbers.Count();   // devolvía 1, no 2
```

**Causa raíz** (`ProjectOopLlvmLowerer.cpp`, `emitOwnedParameterDestroy`, etiquetada "Stage 50.8" — anterior a Stage 52): el cleanup-pad SEH que destruye un parámetro `owned` si el método lanza mientras el parámetro sigue vivo cargaba/escribía SIEMPRE 8 bytes (tratando la storage como puntero), sin comprobar antes si el `typeKey` del parámetro es realmente de tipo referencia. `List<T>.Add(owned T value)` con `T = int` es exactamente el patrón que dispara esto: `owned int value` vive en un `alloca i32` de 4 bytes, pero `Add` hace una llamada anidada (`EnsureCapacity(count + 1)`) que activa el cleanup-pad — cuya carga/escritura de 8 bytes lee/escribe 4 bytes de más allá del final del `alloca`, pisando memoria de stack vecina (en la práctica: el propio `count` del objeto, truncándolo a 1 para siempre).

**Corrección**: guardia añadida al inicio de `emitOwnedParameterDestroy` — si `node->typeKey` no es `builtin:string` ni una referencia de clase/interfaz (`isClassReference`, ya existente en el mismo archivo), no hay nada que destruir para un parámetro `owned` de tipo valor (es un no-op semántico, Stage 52 fix 1/2/3) y la función retorna sin tocar la storage. Mismo patrón (`elementIsReferenceLike`) que `MoveValue` ya usa correctamente desde el propio cierre de Stage 52 — simplemente nunca se había aplicado a esta ruta específica.

Verificado: `List<int>.Add()` repetido ahora incrementa `Count()` correctamente; el ejemplo de esta entrega (`examples/project_stage52x_foreach`) corre limpio bajo Wine con el resultado exacto esperado; **las 207/207 pruebas de CTest existentes siguen pasando** (esta corrección no cambia comportamiento para ningún parámetro `owned` de tipo referencia/string, sólo agrega el camino "no hacer nada" para tipo valor que antes no existía).

**Alcance -- ACTUALIZADO, ya resuelto** (ver `52_x_stage52_cierre_bug_owned_move.md` para el detalle completo): en la entrega original de este documento, `examples/project_stage52/Stage52Collections.exe` seguía crasheando bajo Wine incluso con la corrección de `emitOwnedParameterDestroy` de más arriba, y este documento especulaba -- de forma incorrecta -- que la causa estaba "en algún punto de `List<T>.Sort()`/`Exists()`/el manejo de `Payload`". Una bisección posterior, mucho más exhaustiva (constant-index vs loop-index, con/sin llamada anidada, con/sin `GetEnumerator()`, con/sin generics, clase concreta vs genérica), descartó esa hipótesis por completo y aisló la causa real a un punto muy distinto y mucho más simple: `List<T>.Add(owned T value)` guardaba el parámetro `owned` en el arreglo de respaldo con una asignación PLANA (`items[count] = value;`), sin `move(value)`. Eso deja el storage propio del parámetro sin anular, así que el cleanup automático de fin-de-método de `Add()` destruye el mismísimo objeto que un segundo puntero (el que acaba de quedar guardado en `items[count]`) todavía referencia -- un doble-free real, cuyo síntoma sólo aparece más tarde (en este caso, dentro de `RunList()`, varios pasos después, en Wine, en una dirección de memoria sin relación aparente con `Add()`). El mismo patrón, encontrado con el mismo método, apareció también en `Queue<T>.Enqueue`, `Stack<T>.Push`, `Dictionary<TKey,TValue>` (`InsertNew`/`AddOrUpdate`/`SetValue`), `SortedList<TKey,TValue>` (`InsertAt`/`AddOrUpdate`/`SetValue`), `PriorityQueue<TElement,TPriority>.Enqueue` y `LinkedListNode<T>` (`SetValue`/`SetNext`) -- los siete se corrigieron a la vez, con `move(value)` en el sitio de guardado, y quedan cubiertos por regresión real de ejecución (no sólo estructural) en `tests/unit/Stage52_4OwnedCleanupRuntimeTests.cpp`. `Stage52Collections.exe` corre limpio bajo Wine de forma determinista (verificado en múltiples corridas) desde esta corrección.

## 7. Librería: `List<T>.GetEnumerator()`

NativeSharp no soporta clases anidadas (`parseClassMember` nunca recursa a `parseClassDeclaration`), así que — igual que `LinkedListNode<T>` ya es una clase genérica de nivel superior en vez de un tipo anidado de `LinkedList<T>` — el enumerador de `List<T>` es `ListEnumerator<T>`, una clase genérica de nivel superior en el mismo archivo/namespace (`lib/NativeSharp.Core.Collections.Generic/ListEnumerator.ns`), con una referencia NO owned de vuelta a la lista (mismo patrón que `LinkedListNode<T>.previous`) y un cursor `int`:

```csharp
public class ListEnumerator<T>
{
    private List<T> source;
    private int index;

    public ListEnumerator(List<T> sourceList) { source = sourceList; index = -1; }
    public bool MoveNext() { index = index + 1; return index < source.Count(); }
    public T Current() { return source[index]; }
}
```

`List<T>.GetEnumerator()` está declarado `owned ListEnumerator<T>` (no decorativo: `raii var __foreach_enumerator = ...` exige que su inicializador sea una llamada que devuelva ownership). Cada elemento se lee a través del propio indexer de `List<T>` — el enumerador no duplica storage ni bookkeeping.

**Alcance NO cubierto todavía**: `Dictionary<TKey,TValue>`, `HashSet<T>`, `Queue<T>`, `Stack<T>`, `LinkedList<T>`, `SortedList`/`SortedDictionary`/`SortedSet`, `PriorityQueue` no tienen `GetEnumerator()` propio en esta entrega — `foreach` sobre esas colecciones hoy da un diagnóstico claro ("no compatible member named 'GetEnumerator'"), no un crash, pero no compila. Queda como trabajo pendiente explícito (mismo patrón `XxxEnumerator<T>`, mecánico de replicar una vez validado el patrón sobre `List<T>`).

## 8. Casos probados — evidencia real (Wine, no simulado)

Todos los binarios se compilaron con `nsc --emit-project-oop-exe ... -O0` real y se ejecutaron con `/usr/lib/wine/wine64` real (Wine 9.0), no CI simulado. El ejemplo consolidado final es `examples/project_stage52x_foreach` (guardado permanentemente como test de regresión, sección 9).

| Caso | Resultado esperado | Resultado real (exit code) |
|---|---|---|
| Modo arreglo — suma simple | 150 (10+20+30+40+50) | 150 ✅ |
| Modo arreglo — `break` | 6 (1+2+3, corta en 4) | 6 ✅ |
| Modo arreglo — `continue` (antes del fix §5) | cuelga (loop infinito) | timeout ✅ (reprodujo el bug) |
| Modo arreglo — `continue` (después del fix §5) | 9 (1+3+5) | 9 ✅ |
| Modo arreglo — arreglo vacío | 0 (cuerpo nunca corre) | 0 ✅ |
| Modo arreglo — `foreach` anidado | 441 (21²) | 441 ✅ |
| Modo enumerador — clase propia sin interfaz, `break`/`continue` | 26 (16+10) | 26 ✅ |
| Modo enumerador — `List<int>` real (antes del fix §6) | 100 | 1 ❌ (reprodujo el bug) |
| Modo enumerador — `List<int>` real (después del fix §6) | 170 | 170 ✅ |
| Negativo — `foreach` sobre tipo sin `GetEnumerator()` | diagnóstico claro, sin crash | "no compatible member named 'GetEnumerator'" ✅ |
| Consolidado (`project_stage52x_foreach`, todo junto) | 603 | 603 (91 mod 256) ✅ |

## 9. Regresión completa

`ctest` completo, 207/207 (206 preexistentes + `nsc.stage52-3.foreach`, nuevo): **100% passed, 0 failed**. La corrección del §6 no rompió ningún test existente — sólo agrega un camino "no-op" para un caso (`owned` de tipo valor en cleanup SEH) que antes corrompía memoria silenciosamente y ningún test anterior ejercitaba con la profundidad necesaria para detectarlo (COFF/MIR/HIR verifican FORMA, nunca ejecutan el binario).

## 10. Archivos modificados/creados

**Compilador:**
- `include/nativesharp/Lexer/TokenKind.h`, `src/Lexer/TokenKind.cpp`, `src/Lexer/Lexer.cpp` — keywords `foreach`/`in`.
- `include/nativesharp/AST/AstNodeKind.h`, `src/AST/AstNodeKind.cpp` — nodo `ForeachStatement`.
- `include/nativesharp/Parser/Parser.h`, `src/Parser/Parser.cpp` — `parseForeachStatement` + helpers + despacho.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`, `src/Frontend/ProjectMethodBodyModel.cpp` — `MethodScopeKind::Foreach`, `analyzeForeach`, despacho en `analyzeStatement`, `methodScopeKindName`.
- `src/Frontend/ProjectOopHirModel.cpp` — `case ForeachStatement` en `lowerStatement`.
- `src/LLVM/ProjectOopLlvmLowerer.cpp` — fix de `emitOwnedParameterDestroy` (§6), NO relacionado a foreach en sí pero encontrado por su causa.

**Librería:**
- `lib/NativeSharp.Core.Collections.Generic/ListEnumerator.ns` — nuevo.
- `lib/NativeSharp.Core.Collections.Generic/List.ns` — `GetEnumerator()` agregado.

**Tests/ejemplos:**
- `examples/project_stage52x_foreach/` — nuevo, ejemplo consolidado (§8).
- `examples/project_foreach_negative_test/` — nuevo, caso negativo (diagnóstico sin `GetEnumerator`).
- `tests/unit/Stage52_3ForeachTests.cpp` + registro en `CMakeLists.txt` (`nsc.stage52-3.foreach`) — guarda de regresión permanente, mismo patrón que `Stage52_1GenericCollectionsAdditionsTests.cpp`.

## 11. Cómo se usa

```csharp
// Modo arreglo -- automático, sin nada que implementar.
int[] numbers = ...;
foreach (var n in numbers) { ... }

// Modo enumerador -- cualquier clase con GetEnumerator()/MoveNext()/Current(), sin interfaces.
foreach (var item in miColeccion) { ... }

// Ya funciona hoy para List<T>. Para las demás colecciones de
// NativeSharp.Core.Collections.Generic, ver la sección 7 (pendiente).
```
