# NS3-T10-F03 — Cancelación y destrucción de frames / Cancellation and frame destruction

## Español

Cancelar una operación async equivale a destruir su Future suspendido. El compilador genera un
plan explícito desde el estado actual del frame:

1. ejecuta `defer`/`finally` activos del scope más profundo al más superficial;
2. destruye únicamente locals con drop vivos en ese estado, en orden inverso;
3. termina el Future como `Cancelled` y prohíbe resume/consume posteriores.

La causa distingue solicitud explícita, drop del Future y salida del task scope, pero las tres
rutas comparten la misma semántica de cleanup. El contrato es cooperativo y afirma de forma
auditable `usesThreadAbort=false`.

El planner valida estado suspendido, identidad completa de slots y profundidad real de cada defer.
Así no ejecuta cleanup de un scope todavía no alcanzado ni destruye un local que empieza a vivir
en un `await` posterior.

La prueba puntual `nsc_ns3_t10_cancellation_tests` contiene ocho comprobaciones. No se ejecutó
baseline ni CTest general.

## English

Cancelling an async operation is equivalent to destroying its suspended Future. The compiler
builds an explicit plan from the current frame state:

1. run active `defer`/`finally` actions from deepest to shallowest scope;
2. destroy only drop-bearing locals live in that state, in reverse order;
3. terminate the Future as `Cancelled` and reject later resume/consume operations.

The reason distinguishes explicit request, Future drop and parent task-scope exit, while all three
paths share one cleanup semantics. The contract is cooperative and audibly states
`usesThreadAbort=false`.

The planner validates suspension state, complete slot identity and actual defer depth, preventing
cleanup from an unreached scope or destruction of a local whose lifetime starts at a later await.

The focused `nsc_ns3_t10_cancellation_tests` executable contains eight checks. No baseline or
general CTest suite was run.
