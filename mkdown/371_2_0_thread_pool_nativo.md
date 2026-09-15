# 371 — Thread pool nativo / Native thread pool

## Español

`NativeSharp.Core.Threading.ThreadPool` mantiene un conjunto fijo de threads nativos y una cola
FIFO acotada. `Queue` despierta un worker, `WaitIdle` espera hasta que terminen tanto los trabajos
encolados como los que ya están ejecutándose, y `Shutdown` drena la cola antes de unir todos los
threads. El destructor llama `Shutdown`, de modo que el cierre es determinista e idempotente.

Los workers ejecutan el trabajo fuera del `Monitor`; productores y consumidores sólo mantienen el
lock al modificar índices y contadores. Un delegate `ThreadStart` se presta al pool hasta su
ejecución, igual que en `Thread(ThreadStart)`: el llamador debe conservarlo vivo hasta `WaitIdle` o
`Shutdown`. La cola es acotada y `Queue` devuelve `false` si está llena o el cierre ya comenzó.

La prueba ejecuta 64 trabajos sobre cuatro threads, comprueba el contador atómico y repite el caso
completo en O0 y O2.

## English

`NativeSharp.Core.Threading.ThreadPool` owns a fixed set of native threads and a bounded FIFO queue.
`Queue` wakes one worker, `WaitIdle` waits for queued and currently executing work, and `Shutdown`
drains the queue before joining every thread. The destructor calls `Shutdown`, making teardown
deterministic and idempotent.

Workers execute callbacks outside the `Monitor`; producers and consumers hold the lock only while
changing queue indexes and counters. A `ThreadStart` delegate is borrowed until execution, matching
`Thread(ThreadStart)`: callers must keep it alive through `WaitIdle` or `Shutdown`. The bounded queue
returns `false` when full or after shutdown begins.

The item test executes 64 work items on four threads, verifies the atomic completion count, and
repeats the full scenario at O0 and O2.
