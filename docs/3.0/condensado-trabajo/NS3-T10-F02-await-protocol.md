# NS3-T10-F02 — `await` y protocolo Future / Await and Future protocol

## Español

`FutureProtocol` fija un contrato estático con tipos explícitos para valor, error y cancelación,
operaciones `Poll`/`Resume`, y consumidor único. No existe dispatch virtual oculto. El resultado de
poll distingue `Pending`, `ReadyValue` y `ReadyError`; después de cancelar o consumir no puede
reanudarse ni observarse un payload obsoleto.

`awaitAllowed` centraliza el contexto: `await` requiere una función async suspendible y se rechaza
durante comptime determinista o dentro de una región cleanup marcada no-suspendible.

La API booleana histórica de `FutureState::poll` permanece como wrapper de compatibilidad sobre
`Pending`/`ReadyValue`, por lo que el endurecimiento no rompe consumidores anteriores.

La prueba puntual `nsc_ns3_t10_await_protocol_tests` realiza nueve comprobaciones sobre tipos,
dispatch, contextos, valor, error, cancelación y terminalidad. No se ejecutó baseline ni CTest.

## English

`FutureProtocol` freezes a static contract with explicit value, error and cancellation types,
`Poll`/`Resume` operations, and a single consumer. There is no hidden virtual dispatch. Poll output
distinguishes `Pending`, `ReadyValue` and `ReadyError`; cancelled or consumed futures cannot resume
or expose a stale payload.

`awaitAllowed` centralizes source context: `await` requires a suspendable async function and is
rejected during deterministic comptime or inside a cleanup region declared non-suspendable.

The historical boolean `FutureState::poll` API remains as a compatibility wrapper over
`Pending`/`ReadyValue`, so hardening does not break existing consumers.

The focused `nsc_ns3_t10_await_protocol_tests` executable performs nine checks covering types,
dispatch, contexts, value, error, cancellation and terminal behavior. No baseline or CTest suite
was run.
