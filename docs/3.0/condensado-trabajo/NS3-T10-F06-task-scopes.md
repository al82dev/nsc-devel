# NS3-T10-F06 — Concurrencia estructurada / Structured concurrency

## ES

Un task scope posee todos los hijos creados dentro de su lifetime. Un hijo que toma prestado almacenamiento del scope sólo es válido cuando su lifetime cabe en el scope y existe un join probado antes de salir. Salida normal, error temprano, cancelación y panic conservan la misma frontera: ningún hijo queda vivo al abandonar el bloque.

En salidas anormales, el plan cancela cooperativamente los hijos pendientes en orden inverso, los une y sólo entonces propaga error, cancelación o panic. `spawn`, `poll/join` y `cancel` pertenecen a un protocolo estático de executor; el lenguaje no fija thread pool. Un scope `noheap` exige que el executor declare soporte explícito.

Prueba dirigida: `nsc_ns3_t10_task_scope_tests` cubre child borrow, early error, cancellation, panic, scopes anidados, executor custom y `noheap`.

## EN

A task scope owns every child spawned within its lifetime. A child borrowing scope storage is valid only when its lifetime fits the scope and a join is proven before exit. Normal exit, early error, cancellation, and panic preserve the same boundary: no child remains alive after leaving the block.

On abnormal exits, the plan cooperatively cancels pending children in reverse order, joins them, and only then propagates the error, cancellation, or panic. `spawn`, `poll/join`, and `cancel` belong to a statically dispatched executor protocol; the language does not prescribe a thread pool. A `noheap` scope requires explicit executor support.

Focused test: `nsc_ns3_t10_task_scope_tests` covers child borrows, early errors, cancellation, panic, nested scopes, custom executors, and `noheap`.
