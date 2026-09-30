# Tanda 04 — Genéricos 3.0 / Generics 3.0

> **Estado 2026-09-29:** `IMPLEMENTING`. Las tandas operativas 10–11 conectaron F01/F04/F07 y una
> vertical productiva F02/F03: const bool/enteros builtin, layout dependiente, type packs finales,
> `packcount`, forwarding y rehidratación `.nsmeta`. Opaque aliases, especialización controlada,
> expresiones const generales, value packs/folds/indexado y la inferencia contextual profunda
> continúan abiertos.

## Resultado / Outcome

ES: El compilador productivo combina constraints e inferencia exacta con const generics y type
packs AOT. Valores const afectan layout/identidad; packs vacíos o múltiples se cuentan y reenvían;
todo cruza `.nsmeta` sin diccionario runtime. La materialización usa límites independientes de
profundidad, cantidad y expansión. EN: The productive compiler combines constraints and exact
inference with AOT const generics and type packs. Const values affect layout/identity; empty or
multiple packs can be counted and forwarded; all of it crosses `.nsmeta` without runtime
dictionaries. Materialization has independent depth, count, and expansion limits.

## Cambios y archivos / Changes and files

- `ProjectGenericModel` consume el presupuesto de `GenericSemantics` en cada especialización nueva.
- El dump de genéricos informa materializaciones, cache hits, profundidad máxima y ambos límites.
- `c60_ns3_generic_solver` prueba el camino NativeSharp→MIR→LLVM→PE x64 O0 y devuelve 42.
- `c61_ns3_const_generics_packs` prueba valores const, layouts, packs y forwarding hasta PE O0.
- El source template público de const/packs se rehidrata desde `.nsmeta` en un consumidor separado.
- Los helpers de aliases opacos y especialización continúan siendo contrato aislado.

## Mejora / Improvement

ES: Una llamada como `Item x = Identity(null)` o `return Make();` puede cerrarse sin argumentos de
tipo explícitos, pero el solver no prueba conversiones arbitrarias. Una expansión como
`Expand<T[]>` termina en un diagnóstico estable al alcanzar profundidad 64. Repeticiones exactas
reutilizan la cache y aparecen en el informe.

EN: Calls such as `Item x = Identity(null)` or `return Make();` can close without explicit type
arguments, while the solver refuses arbitrary conversion search. An `Expand<T[]>` expansion stops
with a stable diagnostic at depth 64. Exact repeats reuse the cache and appear in the report.

## Límite / Boundary

ES/EN: La inferencia contextual sigue siendo exacta/directa. Const generics se limitan por ahora a
literales bool/enteros builtin; packs, a un type pack final con count/forwarding. Enums/structural,
const expressions, value packs/index/folds, lambdas/tuplas/method groups, opaque aliases y
especialización controlada continúan abiertas. NSMETA transporta la vertical productiva, no toda la
superficie futura 3.0.

## Validación / Validation

Compilación Debug/O0 de `nsc`; `c60` y `c61` emitidos/ejecutados con exit 42; dumps deterministas;
layouts 16/32; productor `.lib/.nsmeta` → consumidor exit 42; rechazos dirigidos. Sin baseline.
