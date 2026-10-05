# NS3-T10-F05 — `Send`/`Sync` estructurales / Structural `Send`/`Sync`

## ES

`Send` y `Sync` se derivan estructuralmente para tipos compuestos. Valores y referencias heredan las garantías pertinentes del tipo referido; los atómicos proporcionan ambas; los `dyn trait` deben publicarlas explícitamente. Los raw pointers nunca reciben estas garantías automáticamente y una implementación negativa siempre prevalece.

`unsafe impl Send` y `unsafe impl Sync` son overrides independientes y sólo se aceptan con una prueba de seguridad marcada como auditada. Las closures asíncronas se evalúan como compuestos de sus capturas. La decisión se exporta por NSMETA/DLL junto con la versión mayor del contrato de concurrencia y el consumidor falla cerrado ante una versión incompatible.

Prueba dirigida: `nsc_ns3_t10_send_sync_tests` cubre composites, atomics, referencias, raw pointers, dyn traits, negative impl, unsafe auditado, closures async y metadata DLL.

## EN

`Send` and `Sync` are derived structurally for composite types. Values and references inherit the relevant guarantees from their referred type; atomics provide both; `dyn trait` values must publish them explicitly. Raw pointers never receive these guarantees automatically, and a negative implementation always wins.

`unsafe impl Send` and `unsafe impl Sync` are independent overrides accepted only with a reviewed safety proof. Async closures are evaluated as composites of their captures. The decision is exported through NSMETA/DLL metadata with the concurrency contract major version, and consumers fail closed on an incompatible version.

Focused test: `nsc_ns3_t10_send_sync_tests` covers composites, atomics, references, raw pointers, dyn traits, negative impl, audited unsafe overrides, async closures, and DLL metadata.
