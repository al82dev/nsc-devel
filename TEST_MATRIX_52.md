# TEST MATRIX — NativeSharp Stage 52 (cierre)

## Identity

```text
nsc 0.1.0-dev52 (NativeSharp 0.1, stage 52)
```

## Automated matrices (este sandbox, GCC)

| Gate | Result |
|---|---:|
| `nsc.stage52.generic-collections-compiler-fixes` (10 fixes de compilador, entrega original) | 88/88 PASS |
| `nsc.stage52-1.generic-collections-additions` (métodos nuevos por contenedor, esta ronda) | 9/9 PASS |
| `nsc.stage52-2.var-keyword` (`var`, esta ronda) | 12/12 PASS |
| `nsc.stage50_10.ownership-examples-documentation` (fallaba en el paquete previo) | PASS |
| `nsc.stage52-3.foreach` (foreach 52.x, ronda posterior) | PASS |
| **`nsc.stage52-4.owned-cleanup-runtime`** (**gate de EJECUCIÓN REAL**, no solo estructural — ver abajo) | **24/24 PASS** |
| **`nsc.stage52-5.foreach-semantic-gaps`** (P0.1-P0.4 + P1.1, revisión técnica de `foreach`, **ejecución real**) | **24/24 PASS** |
| **`nsc.stage52-6.foreach-collections-enumerators`** (P1.2, `GetEnumerator()` extendido, **ejecución real**) | **16/16 PASS** |
| Full CTest, árbol completo (al momento de este documento) | 210/210 PASS (100%) — creció desde 208/208 con las dos filas de arriba |

Sin fallas conocidas al cierre de esta ronda.

## Ronda `STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md` (`nsc.stage52-5`/`nsc.stage52-6`)

Revisión técnica dedicada a `foreach`, enviada DESPUÉS de que tanto la feature original (52.x)
como el cierre del bug `owned` sin `move()` ya habían sido entregados. Encontró y este paquete
corrige:

- **P0.1** (higiene): temporales sintetizados (`__foreach_index`/`__foreach_source`/
  `__foreach_enumerator`) ahora prefijados `$` -- colisión con nombres de usuario categóricamente
  imposible (el lexer nunca produce `$` en un identificador de usuario), no sólo improbable.
- **P0.2** (fuente ownership-aware): `foreach (var x in Factory.CreateOwnedList())` ahora
  compila y ejecuta correctamente -- antes fallaba con "owned return value cannot initialize a
  non-owning local". Destapó además un SEGUNDO bug latente (`analyzeForeach` nunca cerraba la
  cleanup region de la fuente cuando ésta se vuelve `raii`), corregido en el mismo paso.
- **P0.3** (elemento readonly): la variable de iteración de `foreach` ahora es no-reasignable
  incondicionalmente, igual que en C# real.
- **P0.4** (`const` ya no se acepta): antes se parseaba y se descartaba en silencio; ahora es un
  error de sintaxis honesto.
- **P1.1** (`Current` como propiedad): el desugaring de `foreach` y toda la librería de
  enumeradores migraron de `Current()` (método) a `Current` (propiedad get-only), la forma
  estructural real de C#.
- **P1.2** (`GetEnumerator()` extendido): `Queue<T>`, `Stack<T>`, `LinkedList<T>`, `HashSet<T>`,
  `SortedSet<T>`, `PriorityQueue<TElement,TPriority>` ganan enumerador propio.
  `Dictionary<TKey,TValue>`/`SortedDictionary<TKey,TValue>`/`SortedList<TKey,TValue>`
  DELIBERADAMENTE no lo reciben -- ver `52_x_2_foreach_revision_tecnica_paso_a_paso.md`, Paso 12,
  para la explicación completa (landmine de ABI de structs >8 bytes con `KeyValuePair<TKey,
  TValue>`); usar `Keys()`/`Values()` en su lugar, ya compatibles con `foreach` hoy sin código
  nuevo.

Ver `52_x_2_foreach_revision_tecnica_paso_a_paso.md` para la bitácora paso a paso completa
(comandos y errores reales, no reconstruidos a posteriori).

## Gate de ejecución real (P1 #11 del checklist de cierre: "falta una categoría: runtime
execution gate")

Todas las filas de arriba, salvo la marcada, verifican hasta "compila + LLVM verifica + COFF es
válido" — NUNCA ejecutan el binario resultante. Eso es precisamente lo que dejó pasar dos bugs
reales de doble-free/corrupción de stack sin detectar durante un ciclo de cierre completo (ver
`52_x_stage52_cierre_bug_owned_move.md`). `nsc.stage52-4.owned-cleanup-runtime`
(`tests/unit/Stage52_4OwnedCleanupRuntimeTests.cpp`) es el primer gate de esta suite que además
EMITE UN .EXE REAL DE WINDOWS X64 Y LO EJECUTA (bajo Wine 9.0 cuando está disponible en el host
de CI/desarrollo; se degrada a solo-estructural, sin fallar, si Wine no está presente):

| Sub-caso | Verifica |
|---|---|
| `owned int` + llamada anidada (repro P0 #3 del checklist) | sema, IR sin destrucción dinámica sobre el entero, COFF, ejecución real → 16 |
| `owned` referencia guardado SIN `move()` (caso negativo intencional) | compila, pero DEBE crashear al ejecutarse (confirma que el peligro es real) |
| `owned` referencia guardado CON `move()` (el fix) | compila, IR nulea el slot del parámetro, ejecución real → 30 |

Prioridad de entorno para este gate, igual que el resto del checklist: Windows real primero,
Wine como segunda verificación útil cuando Windows real no está disponible (este sandbox).

## Cobertura enfocada — fixes de compilador para generics (`nsc.stage52.generic-collections-compiler-fixes`, 88 checks)

Los 10 fixes de la entrega original (sin cambios esta ronda): namespace de origen cross-project
en `cloneType`/`bindTypeSyntax`; `move()` como target de asignación a elemento de arreglo;
`default(T)` end-to-end (lexer→parser→AST→semántica→HIR→MIR→LLVM); `!parameterOwnable` evaluado
primero en viabilidad de overload; `elementIsReferenceLike` calculado para toda forma de fuente
de `move()`; lowering LLVM de `MoveValue` sin puntero hardcodeado; límite de struct >8 bytes
documentado (no arreglado) hasta objeto COFF real donde aplica. Ver
`52_nativesharp_generic_collections.md` sección 4 para el detalle fix-por-fix.

## Cobertura enfocada — adiciones de esta ronda de cierre (`nsc.stage52-1.generic-collections-additions`, 9 checks)

Compila `examples/project_stage52/Stage52Collections.nsproj` completo (con las 7 funciones
nuevas de `Main.ns`) hasta objeto COFF verificado — una regresión end-to-end sobre el ejemplo
real en vez de fixtures sintéticas por método, exercitando:

- `List<T>`: `EnsureCapacity`, `TrimExcess`, `AddRange`, `InsertRange`, `RemoveRange`,
  `GetRange`, `FindLastIndex`, `FindLast`, `FindAll` — con `T=int` y `T=Payload`;
- `Queue<T>`/`Stack<T>`: constructor con capacidad, `EnsureCapacity`, `TrimExcess`,
  `TryDequeue`/`TryPop`, `TryPeek`, `CopyTo`;
- `LinkedList<T>`: `AddAfter`, `AddBefore`, `Find`, `FindLast`, `Remove(LinkedListNode<T>)`;
- `Dictionary<TKey,TValue>`: `TryAdd`, `EnsureCapacity`, `TrimExcess` (vía `RehashTo`
  compartido);
- `HashSet<T>`/`SortedSet<T>`: superficie de teoría de conjuntos completa (`UnionWith`,
  `IntersectWith`, `ExceptWith`, `SymmetricExceptWith`, `IsSubsetOf`, `IsProperSubsetOf`,
  `IsSupersetOf`, `IsProperSupersetOf`, `Overlaps`, `SetEquals`, `RemoveWhere`), más
  `GetViewBetween`/`Reverse` en `SortedSet`;
- `SortedList<TKey,TValue>`/`SortedDictionary<TKey,TValue>`: constructor `Comparison<TKey>`,
  `IndexOfValue`, `EnsureCapacity`, `TrimExcess`;
- `PriorityQueue<TElement,TPriority>` (nuevo): `Enqueue`/`Dequeue`/`Peek`/`TryDequeue`/
  `TryPeek` sobre un binary min-heap, con y sin `Comparison<TPriority>` explícito;
- `Comparer<T>`/`EqualityComparer<T>` (nuevos): construcción con comparador natural y con
  delegado explícito, uso como fuente de `Comparison<T>`/`HashFunc<T>`.

Cada función corre con `T=int` (valor) y `T=Payload` (clase, referencia) para confirmar que el
manejo de ownership es correcto en ambos casos.

## Cobertura enfocada — `var` (`nsc.stage52-2.var-keyword`, 12 checks)

Positivo (hasta objeto COFF real):

- `var x = 5;` infiere `int` desde un literal;
- `var y = Add(x, 10);` infiere desde el retorno de una llamada;
- `for (var i = 0; ...)` como inicializador de loop;
- `raii var payload = new Payload();` infiere un tipo clase y habilita RAII;
- `const var frozen = 7;` combina `const` con inferencia.

Negativo (diagnóstico limpio, no crash):

- `var x;` sin inicializador → `"'var' requires an initializer"`;
- `var x = DoNothing();` (retorno `void`) → `"'var' cannot infer a type from a
  void-returning expression"`;
- `public var Value;` como tipo de campo → rechazado (`var` sólo es legal donde
  `parseVariableDeclaration` lo intercepta, no en cualquier punto de `parseType()`).

## Regresiones — ninguna esta ronda

A diferencia de Stage 51 (5 regresiones de suite completa causadas por cambios de compilador,
todas resueltas con causa raíz verificada), esta ronda de cierre no introdujo ninguna regresión
en la suite existente: los 206 tests corren limpios, incluyendo los 204 heredados del paquete
Stage 52 original (una vez restaurado el contenido histórico de
`examples/project_stage50/README.md`, ver `STAGE52_CHECKPOINT.md`) más los 2 nuevos de esta
ronda.
