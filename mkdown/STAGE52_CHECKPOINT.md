# NativeSharp Stage 52 Checkpoint

Status: **CLOSED / VERIFIED** (re-confirmado tras la ronda de cierre real de ejecución — ver
`52_x_stage52_cierre_bug_owned_move.md` para el detalle completo de los dos bugs reales
encontrados y corregidos vía Wine, y la sección "Ejecución en Windows" más abajo, actualizada).
Re-confirmado nuevamente tras la ronda de revisión técnica dedicada a `foreach`
(`STAGE52X_FOREACH_REVISION_TECNICA_Y_FALTANTES.md`) -- cuatro gaps semánticos P0, dos gaps de
compatibilidad P1, un tercer bug real destapado (cleanup region de `foreach` nunca cerrada) --
todos corregidos y verificados bajo ejecución real. Ver
`52_x_2_foreach_revision_tecnica_paso_a_paso.md` para la bitácora completa. CTest: **210/210
PASS (100%)**.

## Scope

Stage 52 — Generic Collections, ronda final de cierre (post-crítica de Alberto sobre el
paquete previamente entregado, `NativeSharp_Stage52_Package.zip`; ver
`STAGE52_FALTANTES_Y_CRITERIOS_DE_CIERRE.md` y `claude/10_stage52_correcciones_cierre.md`).

Implementado en la ronda de entrega original (sin cambios en esta ronda de cierre):

- 10 fixes reales de compilador para soportar generics no triviales (namespace de origen
  cross-project en `cloneType`, `move()` como target de asignación a elemento de arreglo,
  `default(T)` como feature completa de lenguaje, resolución de overload para parámetros
  `owned`/`borrowed`, lowering LLVM de `MoveValue` sin puntero hardcodeado, etc. — ver
  `52_nativesharp_generic_collections.md` sección 4 para el historial completo);
- `NativeSharp.Core.Collections.Generic` completo: `List<T>`, `Queue<T>`, `Stack<T>`,
  `LinkedList<T>`/`LinkedListNode<T>`, `Dictionary<TKey,TValue>`, `HashSet<T>`,
  `SortedList<TKey,TValue>`, `SortedDictionary<TKey,TValue>`, `SortedSet<T>`,
  `KeyValuePair<TKey,TValue>`, delegates de soporte (`Delegates.ns`);
- `NativeSharp.Core.Exception` real, empaquetado por primera vez.

Cerrado en esta ronda, en respuesta directa a los gaps P0/P1 identificados por la crítica:

- **P0 — árbol FULL genuinamente acumulativo (204/204, no 203/204)**: restaurado
  `examples/project_stage50/README.md` desde el último FULL certificado
  (`NativeSharp_Stage51_FULL.zip`), contenido histórico real verificado en dos copias
  idénticas en disco, no fabricado. Sin esto faltaba contenido histórico real y
  `nsc.stage50_10.ownership-examples-documentation` fallaba.
- **P0 — tres artefactos de empaquetado faltantes**: `NativeSharp_Stage52_FULL.zip`,
  `NativeSharp_Stage52_SOURCE_ONLY.zip`, `SHA256SUMS.txt` (ver `PACKAGE_VERIFICATION_STAGE52.md`).
- **P0 — ejecución real en Windows**: contradicción corregida (esta línea decía "sin
  Windows/Wine disponible", lo cual ya no era cierto en el momento en que se escribió —
  ver la sección "Ejecución en Windows" de este mismo documento, que sí documenta Wine 9.0
  instalado y usado). Wine 9.0 SÍ se instaló y usó en este sandbox; encontró y permitió corregir
  dos bugs reales de compilador/librería (`emitOwnedParameterDestroy` para tipos de valor, y
  `owned` guardado sin `move()` en 8 métodos de la librería de colecciones). Ver
  `52_x_stage52_cierre_bug_owned_move.md` para el detalle completo y la re-verificación final.
- **P1 — comparer configurable + inconsistencia doc/código en `SortedList`**: el comentario de
  `Delegates.ns` ya afirmaba que `SortedList<TKey,TValue>` aceptaba un `Comparison<TKey>` en el
  constructor; ahora existe de verdad, junto con el mismo soporte en `SortedDictionary`,
  `SortedSet` y el nuevo `PriorityQueue`.
- **P1 — métodos de alto valor faltantes en cada contenedor** (`EnsureCapacity` público,
  `TrimExcess`, `AddRange`/`InsertRange`/`RemoveRange`/`GetRange`, `TryPop`/`TryDequeue`/
  `TryPeek`, operaciones de conjunto completas en `HashSet`/`SortedSet`, etc. — ver
  `STAGE52_CHANGED_FILES.txt` para el listado completo por archivo).
- **Sección 16 (nuevos tipos genéricos)**: `PriorityQueue<TElement,TPriority>` (binary min-heap)
  y `Comparer<T>`/`EqualityComparer<T>`.
- **Bloque `var` (pedido por Alberto por separado del cierre, no parte de la crítica)**: keyword
  reservada para inferencia de tipo local, ver `PART5_CONFORMANCE_STAGE52.md`.

## Qué se dejó explícitamente fuera de alcance (Opción A, aprobada por Alberto)

- `IEnumerable<T>`/interfaces/`foreach` — la crítica misma ofrecía esto como Opción A
  aceptable; queda para una etapa de lenguaje futura, no es parte de "Generic Collections".
- `ref`/`out` reales, generic constraints (`where T : ...`), struct ABI > 8 bytes — limitaciones
  de lenguaje preexistentes y generales, sin relación específica con collections. La ABI de
  struct se recomienda revisar antes/durante Stage 53 si esa etapa necesita pasar/devolver
  structs grandes por valor (ver `HANDOVER_STAGE52_TO_STAGE53.md`).

## Superficie final de `NativeSharp.Core.Collections.Generic`

```text
12 clases genéricas: List<T>, Queue<T>, Stack<T>, LinkedList<T>, LinkedListNode<T>,
Dictionary<TKey,TValue>, HashSet<T>, SortedList<TKey,TValue>, SortedDictionary<TKey,TValue>,
SortedSet<T>, PriorityQueue<TElement,TPriority>, KeyValuePair<TKey,TValue>,
más Comparer<T>/EqualityComparer<T> (2 clases utilitarias) y 5 delegates de soporte.
```

## Technical diff de esta ronda de cierre sobre el paquete Stage 52 previamente entregado

Ver `STAGE52_CHANGED_FILES.txt` sección "RONDA DE CORRECCION DE CIERRE" para el listado
completo archivo por archivo (compilador: soporte de `var`; librería: métodos nuevos por
contenedor + comparer configurable; nuevos: `PriorityQueue.ns`, `Comparer.ns`; ejemplo y tests
nuevos). Resumen por categoría:

- Compilador: 3 archivos (`TokenKind.h/.cpp`, `Lexer.cpp`) + `Parser.cpp` +
  `ProjectMethodBodyModel.cpp` + `ProjectNameLookup.cpp` — soporte de `var`.
- Documentación/historia: `examples/project_stage50/README.md` (restaurado).
- Librería: 9 archivos de contenedores existentes modificados, 2 archivos nuevos
  (`PriorityQueue.ns`, `Comparer.ns`).
- Ejemplo: `examples/project_stage52/Main.ns` (7 funciones nuevas, T valor y T referencia).
- Tests: 2 archivos nuevos (`Stage52_1GenericCollectionsAdditionsTests.cpp`,
  `Stage52_2VarKeywordTests.cpp`).
- Build: `CMakeLists.txt` (registro de los 2 targets nuevos).

Ningún archivo fue eliminado.

## Verification

Compiler identity:

`nsc 0.1.0-dev52 (NativeSharp 0.1, stage 52)`

Primary tree (este sandbox, GCC, build ya existente):

- `nsc.stage52.generic-collections-compiler-fixes`: **88/88 PASS**
- `nsc.stage52-1.generic-collections-additions`: **9/9 PASS** (nuevo esta ronda)
- `nsc.stage52-2.var-keyword`: **12/12 PASS** (nuevo esta ronda)
- `nsc.stage50_10.ownership-examples-documentation`: **PASS** (fallaba en el paquete previo)
- Full CTest en el momento de este documento: **206/206 PASS (100%)**. Desde entonces creció a
  **208/208 PASS** (foreach 52.x + owned-cleanup runtime tests con ejecución real bajo Wine) —
  ver `52_x_stage52_cierre_bug_owned_move.md`.

Sin fallas. A diferencia de Stage 51 (que cerró con 202/203, una falla documentada), esta
ronda cierra sin ninguna falla conocida en la suite.

Fresh-extraction post-freeze gate: ver `PACKAGE_VERIFICATION_STAGE52.md`.

## Ejecución en Windows -- hallazgo crítico, no un simple "no disponible"

A diferencia de Stage 50/51 (donde Wine/Windows no estaban disponibles en absoluto), esta ronda
sí logró instalar y correr Wine 9.0 en el sandbox, y eso permitió ejecutar binarios reales por
primera vez en la historia del proyecto -- no sólo verificarlos estructuralmente. El resultado
NO es una simple limitación de entorno: **se encontró y aisló con precisión un bug real de
compilador, reproducible, en `emitOwnedParameterDestroy` (`src/LLVM/ProjectOopLlvmLowerer.cpp`,
código de Stage 50, anterior a Stage 52)** que corrompe stack cuando un método con un parámetro
`owned` de tipo valor también llama a otro método. Ver `WINE_EXECUTION_FINDINGS_STAGE52.md` para
el análisis técnico completo (causa raíz identificada en el código fuente, no sólo el síntoma) y
`WINE_CHECKLIST_STAGE52.md` + `windows_execution_checklist/` para el paquete de verificación en
Windows real que necesita correr Alberto antes de que este hallazgo se pueda confirmar o
descartar como artefacto específico de la emulación de Wine.

**Esto no invalida el resto del cierre de Stage 52** (la superficie de la librería compila,
lowerea y emite objeto/PE32+ estructuralmente válido, 206/206 CTest en ese momento), y desde
entonces el estado avanzó dos rondas más:

- **Ronda de cierre de ejecución (posterior a este documento)**: `emitOwnedParameterDestroy` se
  corrigió para tipos de valor (guardia de tipo-referencia, mismo patrón que `MoveValue` ya
  usaba). Verificado con los 3 repros del checklist de cierre bajo Wine real: 42/5/1, sin crash.
- **Segunda ronda (el hallazgo real detrás de "`Stage52Collections.exe` sigue crasheando")**:
  bisección exhaustiva (documentada paso a paso en `52_x_stage52_cierre_bug_owned_move.md`)
  encontró que la causa NO estaba en `Sort()`/`Exists()` como se especuló inicialmente, sino en
  que `List<T>.Add()` (y otros 7 métodos across `Queue.ns`/`Stack.ns`/`Dictionary.ns`/
  `SortedList.ns`/`PriorityQueue.ns`/`LinkedListNode.ns`) guardaban un parámetro `owned` con una
  asignación plana en vez de `move()`, causando un doble-free real cuando el elemento se leía
  más tarde. Corregido en los 14 sitios encontrados, con regresión de EJECUCIÓN REAL (no solo
  estructural) agregada en `tests/unit/Stage52_4OwnedCleanupRuntimeTests.cpp`.

Con ambas correcciones, `Stage52Collections.exe` corre limpio bajo Wine 9.0 de forma
determinista (verificado en 3 corridas independientes), y "ejecución real en Windows" (P0 3.2 de
la crítica) puede considerarse genuinamente cerrado en Wine — pendiente todavía de una
confirmación en Windows real (hardware físico o VM), que sigue siendo la prioridad #1 declarada
por el propio checklist de cierre ("1. Windows real Win64. 2. Wine como segunda ejecución útil").
