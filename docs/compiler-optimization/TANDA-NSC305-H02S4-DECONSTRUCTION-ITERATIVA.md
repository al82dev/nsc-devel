# NSC 3.0.5 — H02S4: recorrido iterativo de deconstruction

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar las dos recursiones duplicadas que expandían patrones tuple de deconstruction. |
| Algoritmo | Scheduler DFS compartido con frames `{syntax, typeKey, byteOffset}` y push inverso. |
| Símbolos | `visitHirTuplePatternLeaves`, declaración `appendBindingLeaf` y asignación `appendTargetLeaf`. |
| Incluido | Tipos por hoja, offsets acumulados, orden izquierda→derecha, descartes y primer fallo. |
| Excluido | Cambiar bindings, layouts, conversiones, diagnósticos, sintaxis, MIR o LLVM. |
| Terminación | Rojo/verde a 32,768 niveles, orden/fallo ramificados, fixture HIR real y regresiones HIR. |

## Rojo reproducido

Las lambdas `appendBinding` y `appendTarget` tenían el mismo DFS recursivo. Se extrajo primero esa
operación común sin cambiar el algoritmo y se conectaron ambas rutas productivas mediante callbacks
hoja distintos. El foco `hir-deconstruction-walk-depth`, con 32,768 `TupleExpression` anidados,
terminó entonces con `-1073741571` (`0xC00000FD`).

El fixture sintético sólo modela el scheduler: la expansión devuelve el hijo inmediato, conserva el
tipo y suma un byte al offset. No intervienen parser, sema, layout real ni enlace, por lo que el rojo
queda atribuido al recorrido de deconstruction.

## Implementación

- `HirTuplePatternFrame` conserva nodo, type key fuente y offset absoluto de la hoja.
- `visitHirTuplePatternLeaves` usa una pila local y sólo expande el frame retirado.
- Los hijos inmediatos se insertan en orden inverso para visitarse de izquierda a derecha.
- El primer callback que devuelve `false` detiene el scheduler; no se preexpanden hermanos.
- La declaración mantiene `_` sin emisión, `LocalDeclaration` antes de
  `TupleDeconstructElement` y el mismo lookup de `localSymbol`.
- La asignación mantiene `_` sin trabajo, `lowerExpression(address=true)` antes de clasificar la
  conversión y el mismo orden de inserción de elementos.
- La consulta de tipos tuple, consulta de layout, validación de cardinalidad, suma de offsets y
  textos de error permanecen en sus rutas originales.
- Las lambdas autorrecursivas `appendBinding` y `appendTarget` desaparecieron; sólo permanecen los
  callbacks hoja no recursivos.

No se modificaron sintaxis, semántica, diagnósticos, formato HIR, API, ABI ni NSMETA.

## Evidencia focal

- Profundidad 32,768: rojo previo `0xC00000FD`; después `PASS 3/3`, exit `0`.
- El mismo foco verifica orden ramificado `a,b,c,d` y corte exacto tras `a,b,c` cuando `c` falla.
- Fixture real en `A:\temp\nsc305-h02d4-deconstruction`, con declaración anidada, asignación
  anidada y descarte: `--check-project-oop-hir -O0` respondió
  `project OOP HIR check: ok (2 callables)`, exit `0`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc --parallel 16`: exit `0`.
- `nsc.exe static host-runtime contract`: `passed`.

La prueba completa `nsc_ns3_t02_deconstruction_tests` quedó `4/5`: el único fallo fue el caso que
intentó enlazar un ejecutable contra
`C:\Program Files (x86)\NativeSharp\lib\nativesharp\runtime\NativeSharp.Runtime.Win32.lib`, archivo
no disponible. No se instaló, escribió ni redirigió nada en C y ese intento no se declara aprobado.

H02 permanece abierto por aggregate flattening y la auditoría final. No se ejecutó baseline,
deploy, instalación, batería completa ni consumidor externo.
