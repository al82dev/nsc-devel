# Entrega 256 — MIR de funciones en paralelo / Parallel function MIR construction

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 256.
- Resultado / Result: cada callable HIR se transforma en una tarea MIR independiente y sus
  resultados se publican en el orden canónico original.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

`ProjectOopMirModel::build` now accepts the effective compilation job count propagated by
`ProjectCompilation`. When concurrency is available, it allocates one result and one diagnostic
slot per HIR function, then submits independent lowering tasks to the shared bounded compiler pool.

Cada tarea usa un `ProjectOopMirModel` privado configurado exclusivamente con punteros de sólo
lectura a los modelos semánticos ya terminados: nombres, miembros, layout, despacho virtual,
interfaces, destrucción y HIR. El recorrido recursivo de `lowerNode` no comparte estado mutable ni
requiere locks.

Workers never append directly to `functions_` and never update `functionByMember_`. After the pool
barrier, the coordinator inspects result slots in exact HIR order, selects the first logical error,
moves successful functions into the final vector and rebuilds the member lookup table. Completion
timing therefore cannot change identities, ordering or diagnostics.

Imports are preserved as bodyless MIR records. Field initializers, static fields and constants,
layouts, dispatch tables and destruction plans remain coordinator passes: they are not independent
function transformations and retaining one publication owner keeps their existing semantics exact.

The serial path invokes the same callable helper directly. There is no alternate MIR dialect or
reduced parallel lowering implementation; one and many workers execute the same transformation.

## Archivos / Files

- `include/nativesharp/Frontend/ProjectOopMirModel.h`: worker-count-aware MIR build contract.
- `src/Frontend/ProjectOopMirModel.cpp`: private lowering contexts, preassigned result/error slots,
  bounded task submission and deterministic coordinator publication.
- `src/Frontend/ProjectCompilation.cpp`: propagation of the effective compiler job count into MIR.
- `tests/unit/ProjectOopLoweringTests.cpp`: complete serial/four-worker HIR and MIR dump equivalence.
- Compiler contract, conformance map and 2.0 roadmap: normative behavior and traceability.

## Decisiones / Decisions

1. Una función por tarea: function bodies are the independent and dominant MIR workload.
2. Lowerer privado por tarea: deep expression traversal remains lock-free.
3. Slots preasignados por índice HIR: task completion order is never observable.
4. Publicación sólo en el coordinador: `functions_` and `functionByMember_` stay coherent.
5. Primer error lógico: diagnostics remain deterministic under scheduling changes.
6. Postprocesos no funcionales seriales: layouts, constants and destruction preserve their
   established dependency order.

## Aceptación / Acceptance

- `ProjectCompilation` forwards its effective `-j` value into MIR construction.
- Multiple HIR callables are lowered through the shared bounded pool.
- No worker mutates the destination MIR vectors or member index.
- Imported C/native callables remain valid bodyless records.
- Unchanged input produces exactly the same complete MIR dump with one and four workers.
- La prueba enfocada OOP lowering pasa 76 checks.
- Reconstrucción limpia de `nsc` pasa; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Construye MIR de funciones en paralelo / Build function MIR in parallel`

**Descripción / Description**

`Lower HIR callables through isolated per-task MIR contexts, place results and diagnostics in
canonical HIR-indexed slots, publish functions and member mappings only on the coordinator, preserve
bodyless imports and non-function postpasses, and prove exact serial/parallel MIR equivalence.`
