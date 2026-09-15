# 373 — Dependencias y cancelación de jobs / Job dependencies and cancellation

## Español

`IJob` proporciona una superficie no genérica para construir grafos entre `Job<T>` de tipos de
resultado distintos. `AddDependency` acepta esas referencias antes de programar el job; el worker
espera cada dependencia y sólo invoca el productor cuando todas completaron. Las dependencias se
prestan: sus propietarios deben vivir hasta que el job dependiente termine. Para evitar un bloqueo
en pools de un solo worker, deben programarse antes que sus dependientes.

La máquina de estados atómica distingue listo, en cola, ejecutándose, completado, cancelación
solicitada y cancelado. `Cancel` tiene éxito sólo antes de comenzar `Invoke`: un job listo se marca y
señala inmediatamente, mientras uno en cola se descarta cuando el worker lo alcanza. Una dependencia
cancelada cancela también al dependiente sin ejecutar su función. Cancelar no intenta interrumpir
asincrónicamente código que ya corre.

La prueba cubre una dependencia `Job<int>` hacia `Job<long>`, cancelación determinista de trabajo en
cola y propagación desde una dependencia cancelada. Verifica que las funciones canceladas jamás se
invocan y repite todos los casos en O0 y O2.

## English

`IJob` supplies a non-generic surface for building graphs between `Job<T>` instances with different
result types. `AddDependency` accepts those references before scheduling; the worker waits for every
dependency and invokes the producer only after all complete. Dependencies are borrowed, so their
owners must outlive the dependent job. To avoid blocking a single-worker pool, dependencies must be
scheduled before dependents.

The atomic state machine distinguishes ready, queued, running, completed, cancellation requested,
and cancelled. `Cancel` succeeds only before `Invoke` begins: a ready job is marked and signalled
immediately, while a queued job is discarded when a worker reaches it. A cancelled dependency also
cancels its dependent without invoking its function. Cancellation does not attempt to interrupt code
that is already running.

The test covers an `int` job feeding a `long` job, deterministic cancellation of queued work, and
propagation from a cancelled dependency. It verifies that cancelled functions are never invoked and
repeats every case at O0 and O2.
