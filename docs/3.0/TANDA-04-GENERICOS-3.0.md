# Tanda 04 — Genéricos 3.0 / Generics 3.0

> **Estado 2026-09-29:** `IMPLEMENTING`. La tanda operativa 10 conectó F01/F04/F07 al modelo
> productivo para constraints existentes, inferencia por tipo esperado exacto y presupuestos de
> monomorfización. F02/F03/F05/F06 y la superficie completa de templates continúan en la tanda 11.

## Resultado / Outcome

ES: El compilador productivo ya combina constraints declarativos con inferencia desde argumentos y
desde un tipo esperado exacto en inicializadores, retornos y asignaciones. La materialización usa
límites independientes de profundidad y cantidad, publica estadísticas deterministas y falla con
cadena causal. EN: The productive compiler now combines declarative constraints with inference from
arguments and exact expected types in initializers, returns, and assignments. Materialization has
independent depth/count limits, deterministic statistics, and causal failure chains.

## Cambios y archivos / Changes and files

- `ProjectGenericModel` consume el presupuesto de `GenericSemantics` en cada especialización nueva.
- El dump de genéricos informa materializaciones, cache hits, profundidad máxima y ambos límites.
- `c60_ns3_generic_solver` prueba el camino NativeSharp→MIR→LLVM→PE x64 O0 y devuelve 42.
- Los helpers de const generics, packs, aliases opacos y especialización continúan siendo contrato
  aislado hasta su conexión productiva en la tanda 11.

## Mejora / Improvement

ES: Una llamada como `Item x = Identity(null)` o `return Make();` puede cerrarse sin argumentos de
tipo explícitos, pero el solver no prueba conversiones arbitrarias. Una expansión como
`Expand<T[]>` termina en un diagnóstico estable al alcanzar profundidad 64. Repeticiones exactas
reutilizan la cache y aparecen en el informe.

EN: Calls such as `Item x = Identity(null)` or `return Make();` can close without explicit type
arguments, while the solver refuses arbitrary conversion search. An `Expand<T[]>` expansion stops
with a stable diagnostic at depth 64. Exact repeats reuse the cache and appear in the report.

## Límite / Boundary

ES/EN: La inferencia contextual de esta entrega es exacta y directa; lambdas, tuplas contextuales,
method groups y propagación contextual anidada permanecen abiertas. Const generics, packs, opaque
aliases y partial specialization siguen en la tanda 11. NSMETA ya transporta constraints/templates
2.x, pero su superficie 3.0 completa todavía no está certificada.

## Validación / Validation

Compilación Debug/O0 de `nsc`; `c60` emitido y ejecutado con exit 42; dos dumps genéricos idénticos;
cuatro rechazos dirigidos para falta de evidencia, conflicto, constraint y profundidad. Sin baseline.
