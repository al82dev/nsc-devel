# Entrega 251 — Scheduler determinista / Deterministic graph scheduler

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 251.
- Resultado / Result: el pool puede ejecutar un DAG por olas topológicas estables sin permitir que
  el orden físico de los threads altere el orden lógico de envío.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

`CompilationGraphScheduler` stores tasks with a non-empty stable key, monotonic ordinal, dependency
ordinals and callback. A task may depend only on earlier tasks. This construction rule rejects self
edges, forward references and duplicate dependencies before workers run, so every accepted graph is
acyclic by construction.

En `run()` se construyen conteos de dependencias y listas inversas. Cada iteración obtiene todos los
nodos listos, los ordena por clave estable y después por ordinal, los envía al
`CompilationWorkerPool` y espera a que termine la ola completa. Sólo entonces libera dependientes.
El paralelismo ocurre dentro de una ola; las fronteras semánticas permanecen explícitas.

Worker completion order is intentionally not treated as semantic order. `submissionOrder()` exposes
the deterministic logical order for tests and future diagnostics. Exceptions captured by the pool
become a scheduler error and prevent subsequent waves from starting.

## Archivos / Files

- `include/nativesharp/Driver/CompilationGraphScheduler.h`: public internal scheduler contract.
- `src/Driver/CompilationGraphScheduler.cpp`: validation, stable ready ordering and wave execution.
- `tests/unit/CompilationGraphSchedulerTests.cpp`: parallel roots, prerequisite barrier and invalid
  dependency evidence.
- `CMakeLists.txt`: driver source and focused test target.
- Compiler contract, conformance map and 2.0 roadmap: normative state and traceability.

## Decisiones / Decisions

1. Olas completas antes que scheduling oportunista: prioriza reproducibilidad inicial.
2. Clave estable más ordinal: empates nunca dependen de direcciones o enumeración del filesystem.
3. Dependencias sólo hacia atrás: el DAG se valida incrementalmente y no admite ciclos tardíos.
4. El scheduler no conoce proyectos todavía; el ítem 252 adaptará `ProjectReferences` a esta API.
5. Un fallo impide nuevas olas, pero el pool termina de forma segura el trabajo ya aceptado.

## Aceptación / Acceptance

- Raíces independientes ejecutan concurrentemente.
- Dependientes esperan a todas sus dependencias.
- Orden de envío estable por clave y ordinal.
- Aristas inválidas se rechazan antes de ejecutar.
- Fallos no terminan threads ni liberan trabajo dependiente.
- Prueba enfocada y reconstrucción limpia de `nsc` pasan; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Agenda dependencias de forma determinista / Schedule dependencies deterministically`

**Descripción / Description**

`Add a stable topological-wave scheduler over the internal worker pool, validate dependency ordinals
before execution, order ready tasks by key and ordinal, stop dependent waves on failure, and cover
parallel roots and prerequisite barriers with a focused test.`
