# Tanda 09 — Iteración, ranges y consultas / Iteration, ranges, and queries

> **Reclasificación 2026-09-29:** `IMPLEMENTING`. El compilador ya contenía foreach productivo para
> arrays y el patrón estructural; la tanda operativa 21 añadió `foreach` sobre `begin..end` entero
> sin materialización. Comprehensions, adapters, generators y `Length/Slice` siguen abiertos. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: Se definió el protocolo foreach 3.0, ranges perezosos, builders de comprehensions, préstamos de
views, frames de generators síncronos y condiciones legales de fusión. EN: Foreach 3.0, lazy ranges,
comprehension builders, lending views, synchronous generator frames, and legal fusion are defined.

## Archivos / Files

- Nuevos `IterationSemantics.h/.cpp`: protocolo, representación y garantías de evaluación.
- Nueva `NativeSharp3T09IterationTests.cpp`: ocho verificaciones puntuales.
- `CMakeLists.txt`: integra el modelo y target T09.

## Mejora / Improvement

Los ranges nunca materializan una colección, el sink decide storage, un lending iterator conserva
el lifetime del source, el frame guarda sólo locals vivos a través de `yield`, y la fusión se permite
únicamente cuando conserva categoría y efectos. Ranges stay lazy, storage is builder policy, lending
preserves source lifetime, frames store only live locals, and fusion cannot reorder effects.

## Límite / Boundary

Parser y state-machine lowering end-to-end se conectan en T15 sobre este contrato ya probado.
Parser and end-to-end state-machine lowering are integrated in T15 over this tested contract.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t09_iteration_tests`; sin baseline ni CTest general.

## Integración productiva 2026-09-29

`foreach (int value in begin..end)` se parsea como `RangeExpression` y se desazucara a temporales
higiénicos más un `for` ordinario. Begin/end se evalúan una vez, el final es exclusivo, `continue`
usa el bloque de incremento existente y no hay colección ni heap implícito. Evidencia:
[TANDA-OPERATIVA-21-T09-E021-INTEGER-RANGE.md](TANDA-OPERATIVA-21-T09-E021-INTEGER-RANGE.md).

El protocolo lending completo, `Length/Slice`, step/descending, algorithms/range adapters,
comprehensions, generators y gates de rendimiento continúan pendientes.
