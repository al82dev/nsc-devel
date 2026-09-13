# Entrega 261 — Diagnósticos paralelos deterministas / Deterministic parallel diagnostics

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 261.
- Resultado / Result: los diagnósticos producidos por workers se reducen en orden de fuente
  canónico, con un solo presupuesto global y conteo completo de descartes.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

`DiagnosticEngine::mergeDeterministically` is the reduction boundary for worker-local diagnostic
engines. It gathers retained diagnostics without mutating the shards, orders them by complete stable
content, then appends that sequence through the destination engine so the global error budget is
applied exactly once.

La clave canónica comienza por validez y `FileId`, continúa con offset inicial y final del rango,
severidad, `DiagnosticId` y mensaje. Con esto, dos workers que terminen en orden opuesto producen la
misma secuencia observable. Si dos entradas son idénticas bajo toda la clave, intercambiarlas no
cambia ni el texto renderizado ni el estado del motor.

Worker engines remain bounded independently so one hostile file cannot allocate an unbounded local
cascade while other tasks run. Suppression performed there is accumulated after the retained
records pass through the destination. Consequently the final count includes both diagnostics lost
inside a shard and records rejected by the single global budget.

The parallel lexer, parser and per-file name binder now use this reducer. Their semantic result
slots remain indexed independently; only diagnostic publication moved from manual per-vector
append loops to the shared canonical authority.

## Archivos / Files

- `include/nativesharp/Diagnostics/DiagnosticEngine.h`: deterministic shard-reduction API.
- `src/Diagnostics/DiagnosticEngine.cpp`: canonical ordering and complete suppression accounting.
- `src/Frontend/ProjectCompilation.cpp`: parallel lexer/parser reduction.
- `src/Frontend/ProjectNameLookup.cpp`: parallel per-file binding reduction.
- `tests/unit/DiagnosticTests.cpp`: reversed shards, canonical order, global limit and local/global
  suppression totals.
- Compiler contract, conformance map and 2.0 roadmap: normative behavior and traceability.

## Decisiones / Decisions

1. Content order, not arrival order: worker scheduling cannot affect diagnostics.
2. Source position leads the key: users see errors in the order they can fix them.
3. Errors precede warnings and notes at an identical source range.
4. One destination budget: parallelism cannot increase the public error limit.
5. Local suppression survives: bounded shards never under-report omitted cascades.
6. Existing phase boundaries remain: diagnostics from later compiler phases do not move ahead of
   earlier phases.

## Aceptación / Acceptance

- Invertir dos shards conserva IDs, ubicaciones, mensajes y orden exacto.
- Un límite global de tres errores retiene exactamente tres aunque cada worker tenga su propio
  límite.
- El total de descartes incluye uno local y uno durante la reducción global.
- `nsc.diagnostics`, `nsc.project-compilation` y `nsc.project-name-lookup` pasan: 3/3 tests.
- La reconstrucción limpia de `nsc` pasa; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Reduce diagnósticos paralelos de forma determinista / Reduce parallel diagnostics deterministically`

**Descripción / Description**

`Canonicalize worker-local diagnostics by source and stable content, apply one global error budget,
preserve local and global suppression counts, and route parallel syntax and name-binding phases
through the shared deterministic reducer.`
