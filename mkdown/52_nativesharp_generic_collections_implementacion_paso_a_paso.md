# 52 — Implementación paso a paso: Generic Collections

Bitácora reproducible de cómo se construyó Stage 52 sobre el árbol Stage 51 cerrado (`nsc 0.1.0-dev51`, 203/204). Cada paso incluye el comando real usado contra `nsc`.

## Paso 0 — Baseline

```bash
cd build && ctest --output-on-failure -j"$(nproc)"
# 203/204 -- unica falla conocida: nsc.stage50_10.ownership-examples-documentation
```

## Paso 1 — `NativeSharp.Core.Exception` real

`throw`/`catch` ya funcionaban desde Stage 45/49, pero cada ejemplo tenía que declararse su propia clase `Exception` local porque el compilador reconoce ese nombre calificado, `NativeSharp.Core.Exception`, pero nunca se había empaquetado. Se agregó `lib/NativeSharp.Core/Exception.ns` con `Exception`, `InvalidOperationException`, `ArgumentOutOfRangeException`, `KeyNotFoundException`, `ArgumentException` (esta última añadida más tarde, paso 5, para `Dictionary<TKey,TValue>.Add`). Ningún cambio de compilador — es pura librería.

## Paso 2 — Probar reglas de ownership no vistas antes del compilador real

Antes de escribir `Queue<T>`/`Stack<T>`/`LinkedList<T>`, se armaron proyectos desechables contra `nsc --check-project-method-bodies`/`--verify-project-oop-llvm` para las formas nuevas que la superficie iba a necesitar:

```bash
# owned T return uniforme para T valor y T referencia
nsc --check-project-method-bodies /tmp/ns52_probe_owned_return/Probe.nsproj

# reasignacion de raii local restringida a move() literal -- workaround con
# arreglo de un elemento (patron usado en LinkedListNode.ns)
nsc --check-project-method-bodies /tmp/ns52_probe_linked/Probe.nsproj
```

Confirmado: `owned T Take()` funciona igual para T valor y T referencia; la reasignación de un local `raii` sólo acepta un `move(...)` literal como RHS, nunca una llamada owned-returning directamente (`x = someObj.TakeNext();` se rechaza) — el rodeo es un arreglo owned de un elemento y reasignar `cursor[0] = ...;` (sin esa restricción para elementos de arreglo).

## Paso 3 — `Queue<T>`, `Stack<T>`, `LinkedListNode<T>`/`LinkedList<T>`

Escritos sobre el patrón ya establecido por `List<T>` (backing `owned T[]`, `EnsureCapacity` con `move()` elemento-a-elemento). `LinkedListNode<T>` guarda `Value`/`Next` en arreglos owned de un elemento (`T[1]`, `LinkedListNode<T>[1]`) en vez de campos bare, por el mismo motivo del Paso 2. Al escribir `LinkedList<int>.AddFirst` con el patrón `created[0].SetValue(move(value))` (T=int, value un parámetro `owned int`) apareció el primer bug real de este stage:

```text
error: no viable overload of 'SetValue'
```

Aislado con una secuencia de probes cada vez más chicos (`Slot<T>` → `Node<T>` con traversal → librería real completa al lado → finalmente `LinkedList<int>` sola) hasta el repro mínimo: un parámetro `owned T` de tipo VALOR, movido como argumento de llamada a otro parámetro `owned T`.

## Paso 4 — Fix 10(a): resolución de overload para `owned`/`borrowed` no-ownable

```cpp
// ProjectMethodBodyModel.cpp, loop de viabilidad de CallExpression -- ANTES
if (parameterOwnership == Owned && parameterOwnable) { ... }
else if (parameterOwnership == Borrowed && parameterOwnable) { ... }
else { /* catch-all: rechaza cualquier move() hacia parametro "no calificado" */ }

// DESPUES
if (!parameterOwnable) { /* bypass -- T valor, sin ceremonia de ownership */ }
else if (parameterOwnership == Owned) { ... }
else if (parameterOwnership == Borrowed) { ... }
else { ... }
```

Verificado con `--check-project-method-bodies`: el repro mínimo pasa a compilar. Pero `--verify-project-oop-llvm` sobre el mismo repro reveló un segundo bug.

## Paso 5 — Fix 10(b): `MoveValue` cargaba/almacenaba siempre como puntero

```text
error: LLVM verifier rejected call parameter type mismatch (loading an i32 alloca as ptr)
```

`MoveValue` (lowering de `move()` desde un local/parámetro nombrado) usaba `LLVMPointerTypeInContext` sin condición. Corregido usando `llvmType(node->typeKey, error)` (igual que ya hacía `MoveArrayElement`) y condicionando el null-store de la fuente a una bandera nueva, `elementIsReferenceLike`, calculada ahora para toda forma de fuente de `move()`:

```cpp
move.elementIsReferenceLike = isReferenceTypeKey(local.typeKey) || local.typeKey == "builtin:string";
```

`--verify-project-oop-llvm` y `--emit-project-oop-obj` sobre el repro mínimo, limpio. Test de regresión: `testMoveOfOwnedParameterAsCallArgumentForBothValueAndReferenceT` (`Stage52GenericCollectionsCompilerFixesTests.cpp`), instanciado con T=Payload (referencia) y T=int (valor), verificado hasta objeto COFF real.

## Paso 6 — `Dictionary<TKey,TValue>`

Diseño de tabla hash con encadenamiento separado (`buckets`/`entryHashCodes`/`entryNext`/`entryKeys`/`entryValues`, free-list para O(1) remove, `Grow()` que compacta). Primer intento de `TryGetValue` devolvía un struct `{bool Found; TValue Value;}` por valor — reveló el segundo hallazgo real de este stage:

```text
error: Stage 40 Windows x64 direct struct value ABI supports only 1, 2, 4 or 8-byte
aggregates; '...DictionaryTryGetResult__g1__Payload...' has size 16
```

Aislado con un repro sin generics en absoluto (`TwoLongs { long A; long B; }`, 16 bytes, ni una referencia involucrada) para confirmar que no era un problema de ownership ni de generics — el mismo error aparece con un struct completamente plano, incluso como variable local sin cruzar ningún límite de llamada:

```bash
nsc --verify-project-oop-llvm /tmp/ns52_probe_structarg/Probe.nsproj
# error identico con TwoLongs, cero generics
```

Y confirmado que, como elemento de ARREGLO accedido sólo por campo, sí funciona:

```csharp
TwoLongs[] arr = new TwoLongs[2];
arr[0].A = 1; arr[0].B = 2;   // OK
long a = arr[0].A;            // OK -- nunca materializa el struct como valor
```

Decisión: no tocar el ABI de structs (fuera de alcance de "generics", ver el capítulo conceptual sección 5). `TryGetValue` se rediseñó con el idioma de arreglo-de-salida:

```csharp
public bool TryGetValue(TKey key, TValue[] result)
{
    int index = FindEntry(key);
    if (index < 0) { return false; }
    result[0] = entryValues[index];
    return true;
}
```

`ToArray()` se reescribió para escribir por campo (`result[outIndex].Key = ...`), nunca materializando un `KeyValuePair<TKey,TValue>` local. `ForEach` se rediseñó con un delegate de dos parámetros nuevo, `KeyValueAction<TKey,TValue>`, en vez de `Action<KeyValuePair<TKey,TValue>>`.

Regresión de este hallazgo: `testOversizedStructWorksAsArrayElementButNotAsAStandaloneValue`, dos mitades — confirma que el patrón de arreglo compila y lowera limpio, y que el patrón de local standalone sigue siendo rechazado con el diagnóstico esperado exacto (`lowerer.error()` contiene `"direct struct value ABI"`), no un crash ni un resultado silenciosamente incorrecto.

Verificado end-to-end contra un proyecto de prueba real con `<ProjectReference>` a la librería, instanciado `Dictionary<int,int>` y `Dictionary<int,Payload>`:

```bash
nsc --check-project-syntax /tmp/ns52_libtest/Stage52Test.nsproj
nsc --check-project-method-bodies /tmp/ns52_libtest/Stage52Test.nsproj
nsc --check-project-oop-hir /tmp/ns52_libtest/Stage52Test.nsproj
nsc --check-project-oop-mir /tmp/ns52_libtest/Stage52Test.nsproj
nsc --verify-project-oop-llvm /tmp/ns52_libtest/Stage52Test.nsproj
nsc --emit-project-oop-obj /tmp/ns52_libtest/Stage52Test.nsproj -o out.o
# los seis: ok
```

## Paso 7 — `HashSet<T>`

Wrapper delgado sobre `Dictionary<T, bool>`. Único punto a verificar: un campo de clase NO owned apuntando a otro objeto (`private Dictionary<T, bool> items;`), liberado manualmente en el destructor (`delete items;`), sin pasar por el idioma de arreglo-de-un-elemento que usa `LinkedListNode.ns`. Probado aislado antes de escribir la clase real:

```csharp
public class Outer
{
    private Inner inner;
    public Outer() { inner = new Inner(); }
    ~Outer() { delete inner; }
}
```

`--check-project-method-bodies` + `--verify-project-oop-llvm`: ok. Confirma que el idioma de arreglo-de-uno de `LinkedListNode.ns` es necesario específicamente cuando hace falta REASIGNAR el campo con `move()` a través de una llamada (Fix 10 aplica ahí), no simplemente para poseer un único objeto.

## Paso 8 — `SortedList<TKey,TValue>`, `SortedDictionary<TKey,TValue>`, `SortedSet<T>`

`SortedList<TKey,TValue>`: arreglos paralelos `keys`/`values` ordenados, búsqueda binaria (`LowerBound`, usando sólo `operator <` — igual que `List<T>.BinarySearch`), inserción/borrado por desplazamiento (igual que `List<T>.Insert`/`RemoveAt`). `SortedDictionary<TKey,TValue>` y `SortedSet<T>` se construyeron como wrappers delgados sobre `SortedList<TKey,TValue>`/`SortedList<T, bool>` respectivamente — decisión de diseño documentada en el capítulo conceptual sección 6, no un descuido.

## Paso 9 — Test de regresión C++ y suite completa

Se agregó `testMoveOfOwnedParameterAsCallArgumentForBothValueAndReferenceT` y `testOversizedStructWorksAsArrayElementButNotAsAStandaloneValue` a `tests/unit/Stage52GenericCollectionsCompilerFixesTests.cpp`:

```bash
cmake --build build --target nsc_stage52_generic_collections_compiler_fixes_tests -j"$(nproc)"
./build/nsc_stage52_generic_collections_compiler_fixes_tests
# PASS: 88 checks
```

## Paso 10 — Versión y suite completa

```cpp
// include/nativesharp/Basic/Version.h
inline constexpr std::string_view kCompilerVersion = "0.1.0-dev52";
inline constexpr std::string_view kCompilerStage = "52";
```

```bash
cmake --build build -j"$(nproc)"
cd build && ctest --output-on-failure -j"$(nproc)"
# 203/204 -- misma unica falla preexistente, nsc.version ahora en verde con dev52/stage 52
```

## Paso 11 — `examples/project_stage52`

Proyecto ilustrativo con `<ProjectReference>` a `NativeSharp.Core` y `NativeSharp.Core.Collections.Generic`, recorriendo cada contenedor con T valor y T referencia. Punto de fricción real encontrado al escribirlo: un literal de string no puede inicializar un `raii string` directamente ni pasarse como argumento `owned` sin pasar por `String.Copy(...)` primero — regla ya existente desde Stage 51, documentada aquí porque es justo el patrón que un consumidor real de `Dictionary<int, string>` necesita.

```bash
nsc --check-project-syntax examples/project_stage52/Stage52Collections.nsproj
nsc --check-project-method-bodies examples/project_stage52/Stage52Collections.nsproj
nsc --check-project-oop-hir examples/project_stage52/Stage52Collections.nsproj
nsc --check-project-oop-mir examples/project_stage52/Stage52Collections.nsproj
nsc --verify-project-oop-llvm examples/project_stage52/Stage52Collections.nsproj
nsc --emit-project-oop-obj examples/project_stage52/Stage52Collections.nsproj -o out.o
# los seis: ok (349 lowered callables, objeto COFF real, machine=0x8664)
```

No se pudo ejecutar el objeto/`.exe` resultante en este entorno (target Windows PE, sin Wine) — misma limitación que Stage 51. **[Superado — ver Paso 12 en adelante.]**

## Paso 12 — Wine 9.0 instalado, primera ejecución real, bug de doble-free `owned`/`move()`

```bash
# instalar Wine 9.0 en el sandbox
wine build/bin_windows/Stage52Collections.exe
# CRASH — page fault, no el checksum esperado
```

Bisección de 12 pasos (cada uno recompilando y corriendo bajo Wine), descartando
`GetEnumerator`/generics/nested-calls/loop-vs-const-index uno por uno, hasta aislar: `items[count]
= value;` (asignación plana) en vez de `items[count] = move(value);` al guardar un parámetro
`owned T value` en el storage de `List<T>.Add`. Auditado el mismo patrón en toda la librería —
encontrado y corregido también en `Queue.ns`, `Stack.ns`, `Dictionary.ns`, `SortedList.ns`,
`PriorityQueue.ns`, `LinkedListNode.ns` (14 sitios). Detalle completo de la bisección y la causa
raíz en `52_x_stage52_cierre_bug_owned_move.md` (documento dedicado de esta ronda).

```bash
wine build/bin_windows/Stage52Collections.exe
echo $?
# 149 -- correcto, sin crash, determinista (3/3 corridas)
cd build && ctest --output-on-failure -j"$(nproc)"
# 208/208
```

## Paso 13 — `foreach` (ronda 52.x)

Desugaring estructural de `foreach (var x in coleccion) { ... }` a un bloque `for` equivalente
usando `GetEnumerator()`/`MoveNext()`/`Current`, inicialmente para `List<T>`/arreglos.

```bash
wine build/bin_windows/Stage52xForeach.exe
echo $?
# 91 -- correcto
```

## Paso 14 — `foreach`, revisión técnica (ronda 52.x.2)

Alberto subió `STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md` con cuatro gaps P0 y dos P1.
Corregidos en orden:

```text
P0.1 -- $__foreach_index/$__foreach_source/$__foreach_enumerator (prefijo $, colision imposible)
P0.2 -- fuente ownership-aware (doble parseo con collectionProbe) -- destapó el bug de cleanup
         region no cerrada en analyzeForeach (ver abajo)
P0.3 -- elemento de iteracion no-reasignable (reusa el mecanismo const existente)
P0.4 -- 'const' ya no se acepta en silencio -- error de sintaxis honesto
P1.1 -- Current como PROPIEDAD get-only, no metodo
P1.2 -- GetEnumerator() extendido a Queue/Stack/LinkedList/HashSet/SortedSet/PriorityQueue
```

El mecanismo de doble parseo de P0.2 destapó un segundo bug real: `analyzeForeach` nunca cerraba
la cleanup region de la fuente cuando ésta se vuelve `raii` (nunca antes podía serlo), causando
`"Stage 45.10 structured exit references an inactive cleanup region"` en cualquier
return/break/continue posterior en el mismo método. Corregido con teardown simétrico al que
`analyzeFor` ya tiene para su propio scope.

```bash
./build/nsc_stage52_5_foreach_semantic_gaps_tests   # 24/24
./build/nsc_stage52_6_foreach_collections_enumerators_tests  # 16/16
cd build && ctest --output-on-failure -j"$(nproc)"
# 210/210
```

## Paso 15 — Bug de RAII/try-catch (tutorial_19) — investigado y CERRADO como bug de Wine

Fuera del código de esta librería, pero descubierto usando sus tipos: reusar un objeto `raii`
después de un `catch` real crasheaba bajo Wine. Investigación dedicada de una sesión completa,
incluyendo un repro mínimo en C puro (sin NativeSharp) confirmando la misma anomalía con clang
real apuntando al triple oficial de Windows. **Confirmado contra Windows real el 2026-08-28: no
es un bug de `nsc`.** Ver `claude/13_bug_raii_trycatch_confirmado_bug_de_wine.md` en el proyecto.

## Paso 16 — Cierre formal definitivo

```bash
cd build && ctest --output-on-failure -j"$(nproc)"
# 210/210 PASS (100%)
./bin/nsc --version
# nsc 0.1.0-dev52 (NativeSharp 0.1, stage 52)
```

Empaquetado final: FULL/SOURCE-ONLY/manifests/hashes regenerados desde el árbol corregido, con
extracción limpia + rebuild + ctest completo desde cero — ver `HANDOVER_STAGE52_TO_STAGE53.md`.
