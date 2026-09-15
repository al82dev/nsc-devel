# 375 — Work stealing / Work stealing

## Español

Cuando su propia cola está vacía, un worker de `ThreadPool` busca trabajo en las demás. El dueño
consume desde la cabeza para conservar FIFO local; el ladrón toma desde la cola opuesta, reduciendo
la contención conceptual entre ambos extremos. Toda selección y extracción sigue protegida por el
monitor del pool, y el callback se ejecuta después de soltarlo.

El constructor normal habilita work stealing. Un overload con `enableWorkStealing=false` conserva
afinidad estricta para usuarios de `QueueOnWorker`; `WorkStealingEnabled` expone la política y
`StealCount` permite observar cuántos trabajos se balancearon. Durante `Shutdown`, los workers
continúan robando y drenando todas las colas antes de terminar.

La prueba bloquea el worker 0 y coloca cuatro trabajos detrás de él. El worker 1 debe completar los
cuatro antes de liberar al primero, y el contador de robos debe reflejarlo. El caso se repite en O0 y
O2 y se vuelven a ejecutar los ítems 371–374.

## English

When its own queue is empty, a `ThreadPool` worker searches the other queues for work. The owner
consumes from the head to retain local FIFO order; a thief takes from the opposite tail, conceptually
reducing contention between both ends. Queue selection and removal remain protected by the pool
monitor, and callbacks execute after releasing it.

The ordinary constructor enables work stealing. An overload with `enableWorkStealing=false`
preserves strict affinity for `QueueOnWorker` users; `WorkStealingEnabled` exposes the policy and
`StealCount` reports how many jobs were balanced. During `Shutdown`, workers keep stealing and drain
all queues before exiting.

The test blocks worker 0 and places four jobs behind it. Worker 1 must complete all four before the
first worker is released, with the steal count reflecting that work. The case runs at O0 and O2, and
items 371–374 are rerun as well.
