# Entrega 250 — Pool interno de workers / Internal compiler worker pool

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 250.
- Resultado / Result: existe capacidad concurrente reusable, limitada por la política del host y
  con lifecycle y propagación de errores definidos.
- Baseline: reservado para el cierre de la Tanda 1.

## Problema / Problem

La política `-j` del ítem 249 no debía desembocar en threads creados ad hoc por cada fase. Eso
produciría oversubscription, lifetimes diferentes y reglas incompatibles para errores. Era necesario
un único componente de ejecución antes de decidir qué nodos del grafo pueden correr juntos.

Parallel scheduling also needs failures to be reproducible. Rethrowing whichever worker happens to
finish first makes diagnostics depend on timing, even when task submission was deterministic.

## Implementación / Implementation

`CompilationWorkerPool` creates a fixed set of joinable threads. A zero-sized constructor request
uses the active `CompilationJobPolicy`; an explicit request is still capped by that policy and its
reserved-core guarantee.

Cada tarea recibe:

- ordinal monotónico de envío;
- índice estable del worker que la ejecuta;
- acceso thread-local a la política resuelta.

The queue is FIFO, guarded by one mutex and a work condition variable. A separate idle condition
waits until both the queue and active-task count are empty. Shutdown marks the pool as stopping,
wakes every worker, drains already accepted work and joins every thread. No worker is detached.

### Fallos deterministas / Deterministic failures

Workers catch task exceptions and store them by submission ordinal. `wait()` completes all queued
work, rethrows the lowest-ordinal failure and clears the captured failure set. The pool can then be
used again. This separates task completion timing from which diagnostic becomes authoritative.

Empty callbacks are rejected synchronously. Thread creation failure stops and joins the subset
already created before propagating the exception.

## Frontera con el siguiente commit / Boundary with the next commit

Este item no paraleliza frontend, grafo ni LLVM. El pool aporta capacidad y reglas de lifecycle. El
ítem 251 decidirá mediante un scheduler determinista qué trabajo es independiente. Mantener esa
separación permite probar threads sin mezclar todavía dependencias semánticas.

## Archivos y funciones / Files and functions

- `include/nativesharp/Driver/CompilationWorkerPool.h`
  - define pool, tareas, ordinales e índice sentinel.
- `src/Driver/CompilationWorkerPool.cpp`
  - implementa creación, cola, espera, shutdown, propagación de política y fallos ordenados.
- `tests/unit/CompilationWorkerPoolTests.cpp`
  - fuerza cuatro workers simultáneos mediante latches;
  - valida índices, política, orden de fallos, reutilización y rechazo de tareas vacías.
- `CMakeLists.txt`
  - incorpora el pool al driver y registra su prueba enfocada.
- Contrato, conformidad y roadmap 1.0.1
  - formalizan invariantes y avance.

## Decisiones de diseño / Design decisions

1. **Pool fijo.** Evita creación repetida y oversubscription por fase.
2. **Threads joinable.** Ningún trabajo sobrevive a los objetos que utiliza.
3. **Errores por ordinal.** El scheduling físico no cambia qué fallo se informa primero.
4. **Drain en destrucción.** Una tarea aceptada no desaparece silenciosamente.
5. **Política copiada.** Los workers no conservan punteros a un scope del driver que pudiera morir.
6. **Scheduler separado.** El pool no inventa dependencias ni orden semántico.

## Criterios de aceptación / Acceptance criteria

- El tamaño se resuelve con la política activa y nunca es cero.
- Cada worker tiene índice estable y recibe la política efectiva.
- Todos los threads se unen al destruir el pool.
- `wait()` espera cola vacía y cero tareas activas.
- El fallo reportado es el de menor ordinal de envío.
- El pool sigue utilizable después de reportar un fallo.
- Una tarea vacía nunca entra a la cola.
- La prueba enfocada y una reconstrucción limpia de `nsc` pasan.
- No se ejecuta baseline dentro de esta entrega.

## Commit de referencia / Reference commit

**Título / Title**

`Agrega el pool interno de compilación / Add the internal compilation worker pool`

**Descripción / Description**

`Introduce a fixed policy-aware compiler worker pool with stable worker indices, FIFO submission,
joined shutdown, deterministic exception propagation and focused concurrency tests, leaving graph
dependency scheduling to the next delivery.`
