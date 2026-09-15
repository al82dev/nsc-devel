# 374 — Colas por worker / Per-worker queues

## Español

`ThreadPool` mantiene ahora una FIFO acotada por worker. El almacenamiento se aplana en un solo
arreglo de delegates, acompañado por índices de cabeza y conteos independientes; un `Monitor`
coordina las transiciones cortas, pero cada worker consume exclusivamente su propia región. La
ejecución del callback continúa fuera del lock.

`Queue` distribuye trabajos round-robin y salta colas llenas. `QueueOnWorker` permite encolado
dirigido para afinidad y hace observable la separación de colas. El argumento `queueCapacity` del
constructor representa la capacidad de cada cola, y `WorkerQueueCount` informa cuántas existen.
Todavía no se consume trabajo ajeno: esa política pertenece explícitamente al ítem 375.

La prueba bloquea el worker 0, deja trabajo detrás de él y envía otro callback al worker 1. Comprueba
que la segunda cola progresa mientras la primera conserva el orden FIFO, en O0 y O2. También vuelve
a ejecutar las pruebas de los ítems 371–373 contra la nueva organización.

## English

`ThreadPool` now maintains one bounded FIFO per worker. Storage is flattened into a single delegate
array with independent head indexes and counts; one `Monitor` coordinates short state transitions,
while each worker consumes only its own region. Callback execution remains outside the lock.

`Queue` distributes work round-robin and skips full queues. `QueueOnWorker` enables directed
submission for affinity and makes queue separation observable. The constructor's `queueCapacity`
argument is the capacity of each queue, and `WorkerQueueCount` reports their number. Workers do not
yet consume another worker's work; that policy belongs explicitly to item 375.

The test blocks worker 0, leaves work behind it, and submits another callback to worker 1. It proves
that the second queue progresses while the first preserves FIFO order at O0 and O2. Tests for items
371–373 are also rerun against the new organization.
