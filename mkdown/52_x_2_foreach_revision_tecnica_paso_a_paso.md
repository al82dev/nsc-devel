# 52.x.2 — Implementación paso a paso: revisión técnica de `foreach`

Bitácora reproducible de la ronda que responde a `STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md`
-- una revisión técnica dedicada a `foreach` que Alberto envió DESPUÉS de que tanto la feature
`foreach` original (52.x) como el cierre del bug `owned` sin `move()` (52.x, ver
`52_x_stage52_cierre_bug_owned_move.md`) ya habían sido entregados. Encontró cuatro gaps
semánticos P0 y dos gaps de compatibilidad P1 en el diseño original de
`Parser::parseForeachStatement`/`ProjectMethodBodyModel::analyzeForeach`, ninguno de los cuales
los tests existentes (`Stage52_3ForeachTests.cpp`, `Stage52_4OwnedCleanupRuntimeTests.cpp`)
ejercitaba. Igual que el resto de esta serie de documentos: cada paso es el comando/error real
que salió del compilador, no una reconstrucción a posteriori.

## Paso 0 — Baseline

```bash
cd build && ctest --output-on-failure
# 208/208 -- 100% passed (ronda de cierre `owned` sin move() ya entregada)
```

## Paso 1 — P0.1: higiene de temporales sintetizados

**El problema**, tal como lo señaló la revisión: `Parser::parseForeachStatement` sintetizaba
identificadores bare (`__foreach_index`, `__foreach_source`, `__foreach_enumerator`) que, aunque
extremadamente improbables de colisionar con código real, no eran IMPOSIBLES de colisionar --
nada en el lexer/parser impedía a un usuario declarar un local literalmente llamado
`__foreach_index` dentro del cuerpo de un `foreach`.

**La solución**: revisar `Lexer.cpp`'s `isIdentifierStart`/`isIdentifierPart` confirma que un
identificador de usuario en NativeSharp solo puede ser `[A-Za-z_][A-Za-z0-9_]*` -- `$` nunca puede
aparecer en NINGÚN identificador que el lexer produzca a partir de código fuente real. Prefijar
cada nombre sintetizado con `$` (`$__foreach_source`, `$__foreach_index`, `$__foreach_enumerator`)
hace la colisión categóricamente imposible, no simplemente improbable -- higiene real, no una
convención de nombres "suficientemente rara". `$` es también un carácter de identificador LLVM IR
válido sin comillas, así que no hace falta ningún cambio complementario en HIR/MIR/LLVM lowering.

```cpp
// Parser.cpp, parseForeachStatement -- todas las ocurrencias renombradas
ast::AstNode* sourceDecl = makeVariableDeclaration(varMarker(), "$__foreach_source", collection, false);
// ... "$__foreach_index" en el candidato array-mode, "$__foreach_enumerator" en el
// candidato enumerator-mode
```

Verificación: `testForeachHygieneUserNameShadowsInternalPrefix`
(`Stage52_5ForeachSemanticGapsTests.cpp`) declara un local de usuario llamado literalmente
`__foreach_index` (sin `$`) dentro de un `foreach`, y confirma bajo ejecución real bajo Wine que
el loop sigue calculando el resultado correcto -- no solo que compila, sino que el loop interno no
se confunde con el nombre del usuario.

## Paso 2 — P0.2: fuente de `foreach` ownership-aware

**El problema**, verificado primero con un proyecto desechable antes de tocar el compilador (ver
sección "Errors and fixes" del historial de esta sesión):

```bash
nsc --check-project-method-bodies /tmp/ns51_probe_foreach_owned/Probe.nsproj
# error: owned return value cannot initialize a non-owning local; use a raii owner destination
```

`foreach (var x in Factory.CreateList())`, donde `CreateList()` devuelve `owned List<int>`, no
compilaba nunca: el parser siempre construía `sourceDecl` (`var $__foreach_source = <collection>;`)
como un `var` plano, no-`raii`, porque en tiempo de parseo no hay información de tipos disponible
para saber si la expresión de colección terminará siendo owned-returning.

**Callejón sin salida #1 -- "$__foreach_source siempre raii"**: probado, y falla en el caso común
(array-mode):

```bash
nsc --check-project-method-bodies /tmp/ns51_probe_raii_array/Probe.nsproj
# error: raii local requires a NativeSharp class/interface reference type or native string
```

Los arreglos nunca pueden ser `raii` -- hacer `$__foreach_source` incondicionalmente `raii`
rompería el caso array-mode, que es el caso común/ya funcionando.

**La solución adoptada**: parsear la expresión de colección DOS VECES desde el mismo rango de
tokens -- una vez para `collection` (el nodo real, el que termina en `sourceDecl`) y una segunda
vez, tras resetear `index_` al mismo punto, en un subárbol AST estructuralmente idéntico pero
completamente separado (`collectionProbe`), adjuntado al nodo `ForeachStatement` como hijo extra
vía `addChild`. `ProjectMethodBodyModel::analyzeForeach` corre un `analyzeExpression` desechable
sobre esa copia (con un `DiagnosticEngine` de scratch, descartado) para conocer la forma de
ownership de la colección ANTES de decidir si `sourceDecl` necesita el modificador `raii`:

```cpp
// Parser.cpp
const std::size_t collectionStart = index_;
ast::AstNode* collection = parseExpression();
const std::size_t collectionEnd = index_;
index_ = collectionStart;
ast::AstNode* collectionProbe = parseExpression();
index_ = collectionEnd;
```

```cpp
// ProjectMethodBodyModel.cpp, analyzeForeach -- ANTES de analizar $__foreach_source
const ExpressionResult probe = analyzeExpression(
    collectionProbe, methodMemberOrdinal, foreachScope, scratchDiagnostics);
if (probe.valid && probe.ownership == sema::OwnershipKind::Owned &&
    probe.ownershipTransfer && probeTypeOwnable)
{
    statement->first()->addModifier(TokenKind::KwRaii);
}
```

**Por qué una copia separada y no analizar `collection` dos veces**: `analyzeExpression` tiene
efectos de lado reales más allá de su valor de retorno (empuja a `nameBindings_` y otros vectores
de estado del modelo semántico). Probar el MISMO nodo dos veces registraría esos bindings dos
veces; probar una copia genuinamente separada y nunca referenciada por el camino de lowering real
no tiene ningún efecto sobre el análisis real.

Verificación inicial (sema, sin ejecutar): con el fix de arriba, el repro mínimo pasa
`--check-project-method-bodies`. Pero al escribir el test de regresión con ejecución real bajo
Wine apareció un SEGUNDO bug, latente desde el diseño original de `analyzeForeach`.

## Paso 3 — el bug latente que el Paso 2 destapó: cleanup region nunca cerrada

```bash
nsc --emit-project-oop-exe /tmp/manual_p02/ForeachOwnedSource.nsproj -O0 -o /tmp/manual_p02/out.exe
# nsc: error: Stage 45.10 structured exit references an inactive cleanup region
```

Aislado comparando `analyzeForeach` con `analyzeFor` (la función hermana para `for`, que SÍ
resuelve este caso correctamente para su propio local de inicialización): `analyzeFor` captura
`breakCleanupDepth = activeCleanupRegions_.size()` ANTES de analizar su declaración de init, y al
final de la función -- después de analizar todo el cuerpo del loop -- hace pop explícito de
cualquier cleanup region abierta desde ese punto, registrándola como "cleanup normal de fin de
scope" vía `recordNormalCleanupContinuation`. `analyzeForeach` nunca tuvo el equivalente: como
`$__foreach_source` nunca había sido `raii` antes de este mismo Paso 2, nunca creaba una cleanup
region, y este gap estaba completamente dormido -- ningún test existente podía dispararlo.

Con el fix del Paso 2, `$__foreach_source` SÍ puede ahora ser `raii`, así que su cleanup region
(creada por `analyzeVariable` igual que cualquier otro local `raii`) quedaba activa para siempre
en `activeCleanupRegions_` -- corrompiendo la contabilidad de cleanup para el SIGUIENTE
`return`/`break`/`continue` analizado en cualquier parte posterior del método, que calculaba un
cleanup path incluyendo un ordinal que LLVM lowering nunca había entrado/salido realmente
(el scope de `foreach` nunca estaba conectado a eso).

**El fix**, simétrico al de `analyzeFor`:

```cpp
// analyzeForeach -- capturado ANTES de declarar $__foreach_source
const std::size_t foreachCleanupDepth = activeCleanupRegions_.size();
// ... análisis normal ...
// al final, después de analizar el candidato elegido:
for (std::size_t index = activeCleanupRegions_.size(); index > foreachCleanupDepth; --index)
{
    recordNormalCleanupContinuation(
        activeCleanupRegions_[index - 1], statement, methodMemberOrdinal, foreachScope);
}
activeCleanupRegions_.resize(foreachCleanupDepth);
```

Verificación real:

```bash
nsc --emit-project-oop-exe /tmp/manual_p02/ForeachOwnedSource.nsproj -O0 -o /tmp/manual_p02/out.exe
# project OOP PE executable emission: ok
WINEDEBUG=-all wine64 /tmp/manual_p02/out.exe; echo $?
# 60  (10+20+30, exactamente lo esperado)
```

Este es exactamente el tipo de bug que este proyecto ya documentó que sólo aparece bajo ejecución
real -- ver `52_x_stage52_cierre_bug_owned_move.md`. `testForeachOverOwnedReturningSource`
(`Stage52_5ForeachSemanticGapsTests.cpp`) es el guardia de regresión, ejecutado bajo Wine.

## Paso 4 — P0.3: variable de iteración readonly

**La decisión**: el elemento de `foreach` pasa a ser incondicionalmente no-reasignable,
exactamente como el iteration variable de C# real. Se reutiliza el mecanismo YA EXISTENTE que
un local `const` ordinario usa (`AstNode::setFlag(isConst)` en tiempo de parseo,
`local.isConst = variable->flag();` en `analyzeVariable`, que luego bloquea `result.assignable`)
-- ningún mecanismo nuevo, sólo un sitio de llamada distinto alimentando la bandera existente:

```cpp
// makeVariableDeclaration ganó un 5to parámetro, isConst = false por defecto
ast::AstNode* makeVariableDeclaration(
    ast::TypeSyntax* type, std::string name, ast::AstNode* initializer, bool isRaii, bool isConst = false);

// ambos candidatos (array-mode, enumerator-mode) pasan isConst=true para el elemento
ast::AstNode* elementDecl = makeVariableDeclaration(
    elementType, elementName, /* initializer */, elementIsRaii, /*isConst=*/true);
```

Verificación (negativa, sólo diagnóstico -- no requiere ejecución real, es un rechazo en tiempo de
compilación):

```
foreach (var n in numbers) { n = 99; ... }
```

```text
error: left side of assignment is not assignable
```

`testForeachElementIsReadonly` (`Stage52_5ForeachSemanticGapsTests.cpp`).

## Paso 5 — P0.4: la palabra clave `const` ya no se acepta

**El problema original**: `foreach (const var x in ...)` parseaba con éxito y luego DESCARTABA
`const` en silencio (`const bool elementIsConst = consumeIf(...); (void) elementIsConst;`) -- una
promesa que el compilador nunca cumplía.

**La decisión**: dado que el Paso 4 ya hace el elemento readonly incondicionalmente, aceptar
`const` ahí sería redundante en el mejor caso y engañoso en el peor (implicaría que los elementos
sin `const` SÍ son mutables, lo cual ya no es cierto). Se quitó el `consumeIf(TokenKind::KwConst)`
por completo de esa posición -- ahora es un error de sintaxis honesto en vez de un no-op
silenciosamente ignorado. Real C# tampoco acepta `const` en la variable de iteración de un
`foreach`, así que esto además alinea el comportamiento con el lenguaje que NativeSharp imita.

```text
foreach (const var n in numbers) { ... }
```

Ya no compila -- error de sintaxis (`const` no es consumido por nada en esa posición, y ni
`parseType()` ni el chequeo de `var` lo aceptan). `testForeachConstKeywordIsNowARejectedSyntaxError`.

## Paso 6 — P1.1: `Current` como propiedad, no método

**El problema**: el `IEnumerator<T>.Current` de C# real es una PROPIEDAD get-only; el desugaring
original de NativeSharp llamaba `Current()` con paréntesis -- una desviación innecesaria dado que
este compilador soporta propiedades desde Stage 38.

**El fix**, en tres lugares que deben cambiar juntos (cambiar sólo el desugaring rompería toda
librería de enumeradores existente; cambiar sólo la librería no tendría efecto sin el cambio de
desugaring):

```cpp
// Parser.cpp, patternCandidate -- ANTES
ast::AstNode* elementDecl = makeVariableDeclaration(
    elementType, elementName,
    makeZeroArgCallExpression(makeMemberExpression(..., "Current")),
    elementIsRaii);

// DESPUES -- acceso de miembro plano, sin llamada
ast::AstNode* elementDecl = makeVariableDeclaration(
    elementType, elementName,
    makeMemberExpression(makeIdentifierExpression(range, "$__foreach_enumerator"), "Current"),
    elementIsRaii, /*isConst=*/true);
```

```csharp
// ListEnumerator.ns -- ANTES: public T Current() { return source[index]; }
// DESPUES:
public T Current
{
    get { return source[index]; }
}
```

Mismo cambio en `examples/project_stage52x_foreach/Main.ns`'s `CountUpToEnumerator`.

Verificación: `Stage52_3ForeachTests.cpp` (recompilación completa de
`examples/project_stage52x_foreach`, que ya usa `List<T>`/`ListEnumerator<T>`) sigue en verde sin
cambios propios -- confirma que el cambio de desugaring y el cambio de librería quedaron
sincronizados. `testForeachOverCustomEnumeratorWithPropertyCurrent`
(`Stage52_5ForeachSemanticGapsTests.cpp`) agrega un caso enfocado, ejecutado bajo Wine, con un
enumerador mínimo hecho a mano.

## Paso 7 — regresión completa tras P0.1-P0.4 + P1.1

```bash
cmake --build build -j"$(nproc)"
cd build && ctest --output-on-failure
# 209/209 -- 100% passed (208 previos + nsc_stage52_5_foreach_semantic_gaps_tests)
./nsc_stage52_5_foreach_semantic_gaps_tests
# PASS: 24 checks
```

También re-verificados bajo Wine real (no sólo estructural) los ejecutables ya existentes, para
confirmar que ninguno de los cambios de esta ronda -- especialmente el rename `$`-prefix y el
cambio `Current`-como-propiedad -- alteró resultados ya documentados:

```text
Stage52xForeach.exe          -> 91,  sin crash (idéntico al valor documentado en la ronda anterior)
Stage52Collections.exe       -> 149, sin crash (idéntico al valor documentado en la ronda anterior)
```

## Paso 8 — P1.2: `Queue<T>`, `Stack<T>`

Mismo patrón que `ListEnumerator.ns` (clase top-level -- este compilador no soporta tipos
anidados -- que pide prestada, no posee, la colección fuente, y lee cada elemento a través de un
accesor público). Ninguna de las dos ya tenía forma de acceder por posición lógica, así que cada
una gana un indexer get-only mínimo:

```csharp
// Queue.ns
public T this[int index]
{
    get { return items[PhysicalIndex(index)]; }
}

// Stack.ns -- orden tope-a-fondo, igual que ToArray()/CopyTo() ya usan
public T this[int index]
{
    get { return items[count - 1 - index]; }
}
```

`QueueEnumerator<T>`/`StackEnumerator<T>` (archivos nuevos) son casi idénticos a
`ListEnumerator<T>`: `source`/`index`, `MoveNext()` incrementa y compara contra `Count()`,
`Current` (propiedad, P1.1) lee `source[index]`.

## Paso 9 — P1.2: `LinkedList<T>`

Distinto de los anteriores: `LinkedList<T>` no tiene (ni debería tener) acceso O(1) por posición
-- agregar un indexer ahí derrotaría el propósito de una lista enlazada. `LinkedListEnumerator<T>`
en cambio camina la cadena de nodos viva directamente vía `First()`/`LinkedListNode<T>.Next()`,
los mismos accesores públicos que cualquier otro caller ya usa para recorrer la lista a mano:

```csharp
public bool MoveNext()
{
    if (started) { current = current.Next(); }
    else { current = source.First(); started = true; }
    return current != null;
}
```

`started` existe porque `null` es también el valor legítimo de "cadena agotada" -- sin él, el
primer `MoveNext()` no podría distinguir "todavía no empecé" de "ya terminé".

## Paso 10 — P1.2: `HashSet<T>`, `SortedSet<T>`

Ambos son wrappers delgados sobre `Dictionary<T, bool>`/`SortedList<T, bool>` (ver sus propios
comentarios de clase) -- el layout interno de buckets/entries/free-list de `Dictionary` no tiene
una "posición lógica i" estable para indexar como sí la tienen los contenedores respaldados por
arreglo. En vez de exponer ese layout interno, el enumerador usa el `ToArray()` que ambas clases
ya tienen (el mismo patrón que `ForEach()` ya usaba internamente) como una SNAPSHOT tomada una
vez, al construir el enumerador -- distinto en filosofía de `ListEnumerator`'s referencia viva,
pero la única opción correcta y barata disponible aquí:

```csharp
public HashSetEnumerator(HashSet<T> sourceSet)
{
    snapshot = sourceSet.ToArray();
    index = -1;
}
```

Ownership del snapshot: igual que el local de `ForEach()` (`const T[] snapshot = items.Keys();`
... `delete[] snapshot;`), el arreglo devuelto por `ToArray()` debe liberarse explícitamente --
por eso `HashSetEnumerator<T>`/`SortedSetEnumerator<T>` ganan destructor propio
(`delete[] snapshot;`), aunque sus ELEMENTOS sean copias no-owned (siguen perteneciendo al
`Dictionary`/`SortedList` de respaldo).

## Paso 11 — P1.2: `PriorityQueue<TElement,TPriority>`

Real C#'s `PriorityQueue<TElement,TPriority>` implementa `IEnumerable<TElement>` -- NUNCA
`IEnumerable<(TElement,TPriority)>` -- en orden interno del heap, no ordenado por prioridad. Se
replica exactamente esa forma: un indexer get-only sobre el arreglo crudo del heap
(`elements[index]`, orden heap-interno, no prioridad) y `Current` yield solo `TElement`:

```csharp
public TElement this[int index]
{
    get { return elements[index]; }
}
```

Esta asimetría deliberada con `Dictionary`/`SortedList` (que NO reciben `GetEnumerator()`, ver
Paso 12) es segura precisamente porque `TElement` es un único parámetro de tipo -- la misma
historia de ABI que cualquier otro contenedor de un solo parámetro en este paquete, nunca un
struct combinando dos parámetros genéricos independientes cuyo tamaño combinado no se puede
predecir en tiempo de autoría de la librería.

## Paso 12 — P1.2: la decisión de NO implementar `Dictionary`/`SortedDictionary`/`SortedList`

**La revisión explícitamente pide no improvisar** ("no debería improvisarse un ABI de tuple o
struct grande sin revisar las restricciones ya documentadas", sección 12). Revisando esas
restricciones (comentario de clase de `Dictionary.ns`, ya existente antes de esta ronda): un
`KeyValuePair<TKey,TValue>` NUNCA debe materializarse en un local standalone
(`KeyValuePair<TKey,TValue> pair = result[i];`) cuando `TKey`/`TValue` combinados pueden exceder 8
bytes -- justo la restricción de ABI de structs de Stage 40 (1/2/4/8 bytes exactos, nada más).

El desugaring de `foreach` en modo array hace EXACTAMENTE eso: `<ElementType> <name> =
$__foreach_source[índice];` -- una declaración de variable con el elemento del arreglo como
inicializador, materializándolo en un local standalone. Confirmar esto contra el compilador real
antes de decidir:

```bash
nsc --check-project-method-bodies /tmp/p12test/P12Test.nsproj
# (fixture: Dictionary<int,int> con foreach directo)
# error: receiver/type '...Dictionary...' has no compatible member named 'GetEnumerator'
```

**La decisión**: NO agregar `GetEnumerator()` a `Dictionary<TKey,TValue>`,
`SortedDictionary<TKey,TValue>`, ni `SortedList<TKey,TValue>` -- las tres ya implementan (real
C# también) la superficie conceptual de "diccionario", así que las tres comparten el mismo
problema. Ofrecer un `foreach (var pair in dict)` que compila y funciona SOLO para las
combinaciones de `TKey`/`TValue` cuyo tamaño combinado da 1, 2, 4 u 8 bytes, y falla con un error
de mapeo de tipos LLVM para cualquier otra combinación, es peor que no ofrecerlo -- exactamente el
resultado "ABI de tuple/struct improvisada" que la revisión advierte evitar.

La alternativa, ya existente, ya seguro en ABI, y ya compatible con `foreach` HOY sin ningún
código nuevo: `Keys()`/`Values()` devuelven arreglos de UN solo parámetro de tipo cada uno
(`TKey[]`/`TValue[]`), la misma forma que cualquier otro contenedor de este paquete ya soporta sin
problema, y al ser arreglos planos ya son compatibles con el candidato array-mode de `foreach`:

```csharp
foreach (var key in dict.Keys()) { ... }
foreach (var value in dict.Values()) { ... }
```

Verificado bajo ejecución real:

```bash
nsc --emit-project-oop-exe /tmp/p12test/P12Test.nsproj -O0 -o /tmp/p12test/out2.exe
WINEDEBUG=-all wine64 /tmp/p12test/out2.exe; echo $?
# 101  (1 + 100, la única clave y su único valor)
```

Cada uno de los tres archivos (`Dictionary.ns`, `SortedDictionary.ns`, `SortedList.ns`) documenta
esta decisión en su propio comentario de clase, junto al comentario preexistente sobre el límite
de ABI de structs que la motiva.

## Paso 13 — regresión completa tras P1.2

```bash
cmake --build build -j"$(nproc)"
cd build && ctest --output-on-failure
# 210/210 -- 100% passed (209 previos + nsc_stage52_6_foreach_collections_enumerators_tests)
./nsc_stage52_6_foreach_collections_enumerators_tests
# PASS: 16 checks
```

Un solo fixture combinado bajo ejecución real (no solo estructural) cubre los seis contenedores
recién enumerables en una corrida de Wine, verificando ORDEN real (no solo "compila" o "no
crashea"): FIFO para `Queue<T>`, tope-a-fondo para `Stack<T>`, orden de inserción para
`LinkedList<T>`, orden ordenado para `SortedSet<T>` (insertado 3,1,2 -- debe iterar 1,2,3), y
CONTEO de elementos para `HashSet<T>`/`PriorityQueue<>` (cuyo orden interno es, por diseño,
no especificado -- igual que en C# real):

```text
6 (queue) + 302010 (stack, orden verificado dígito a dígito) + 600 (linked list) +
1000 (hash set) + 123 (sorted set, orden verificado) + 3 (priority queue, conteo) = 303742
-> wine exit code 303742 % 256 = 126
```

Sin regresiones: los 209 tests previos siguen en verde, incluyendo
`nsc_stage52_3_foreach_tests` (recompilación completa de `examples/project_stage52x_foreach`, que
usa `List<T>`/`ListEnumerator<T>` y por tanto ejercita indirectamente que el diseño P1.1/P1.2 no
rompió nada del contenedor original).
