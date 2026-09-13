# Entrega 255 — HIR de funciones en paralelo / Parallel function HIR construction

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 255.
- Resultado / Result: callables regulares elegibles se bajan a OOP HIR en tareas independientes y
  se publican en el mismo orden e identidad ordinal que el lowering serial.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

The master HIR model submits one bounded task per eligible method, constructor, destructor or import.
Each task creates a private `ProjectOopHirModel`, initializes it from the immutable completed semantic
models and runs the existing lowering implementation with a single-member filter. This reuses the
full language lowering rather than introducing a reduced parallel code path.

El modelo privado conserva todo el estado mutable que antes impedía concurrencia: pila de cleanups,
handlers EH activos, contexto de lambda, rango del error y contadores de temporales/regiones. Ningún
worker escribe en `functions_`, `functionByMember_` ni en el error del modelo maestro.

Results occupy preassigned member slots. After the pool barrier, the coordinator selects failures by
member ordinal and appends successful functions in that same order. Synthetic owned-temporary locals
and cleanup regions start from the serial floor inside each worker; a tree scan counts their local
range and applies the cumulative deterministic offset. HIR exception regions receive the equivalent
cumulative rebase, including every exceptional target.

Lambda functions remain sequential after regular functions because closure-cell ownership resolves
against the completed creator-function table. Field initializer HIR and synthetic implicit
constructors also remain coordinator work. This boundary preserves their dependencies while moving
the independent, usually dominant written-callable bodies to the worker pool.

## Archivos / Files

- `include/nativesharp/Frontend/ProjectOopHirModel.h`: worker-count-aware build and filtered internal
  lowering contract.
- `src/Frontend/ProjectOopHirModel.cpp`: isolated per-callable tasks, deterministic error selection,
  ordinal scanning/rebasing and stable publication.
- `src/Frontend/ProjectCompilation.cpp`: effective job-count propagation into OOP HIR.
- `tests/unit/ProjectOopLoweringTests.cpp`: full serial/four-worker HIR dump equivalence.
- Compiler contract, conformance map and 2.0 roadmap: normative state and traceability.

## Decisiones / Decisions

1. Reusar el lowerer completo por tarea: no existe una semántica HIR paralela alternativa.
2. Estado privado por worker: no locks en el recorrido profundo de expresiones y statements.
3. Result slot por member: finishing order never becomes function order.
4. Rebase post-lowering: synthetic ordinals remain compact and match serial allocation order.
5. Postprocesos dependientes coordinados: closures and implicit construction see a complete table.

## Aceptación / Acceptance

- Métodos, constructores, destructores e imports elegibles se procesan mediante el pool acotado.
- Fallos se reportan por el primer `memberOrdinal` lógico y conservan su rango fuente.
- Temporales owned y regiones EH no colisionan entre tareas.
- Uno y cuatro workers generan exactamente el mismo dump HIR completo del fixture.
- La prueba enfocada OOP lowering pasa 75 checks.
- Reconstrucción limpia de `nsc` pasa; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Construye HIR de funciones en paralelo / Build function HIR in parallel`

**Descripción / Description**

`Lower eligible callables through isolated per-task HIR models, consume results by member ordinal,
rebase synthetic locals, cleanup regions and EH regions into deterministic global ranges, preserve
the existing lowering semantics and error locations, and prove exact serial/parallel HIR equivalence.`
