# Tanda 10 — Coroutines y concurrencia estructurada / Coroutines and structured concurrency

> **Reclasificación 2026-09-29:** `IMPLEMENTING`. La tanda operativa 22 convirtió las reglas en el
> contrato generado `NSCONCURRENCY 1.0`. Faltan syntax/lowering de async, frames, ownership,
> Send/Sync productivo, metadata y logical debug stacks. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: Se definieron futures lazy, storage elegido por caller, borrows a través de await, máquina de
poll, cancelación determinista, async generators con backpressure, Send/Sync, task scopes y órdenes
atómicos. EN: Lazy futures, caller-selected storage, await borrows, polling, deterministic
cancellation, backpressured async generators, Send/Sync, task scopes, and atomic orderings are set.

## Archivos / Files

- Nuevos `CoroutineSemantics.h/.cpp`: contrato async y de memoria T10.
- Nueva `NativeSharp3T10CoroutineTests.cpp`: diez verificaciones específicas.
- `CMakeLists.txt`: integra modelo y target aislado.

## Mejora / Improvement

No hay scheduler ni heap global impuesto; un future no puede reanudarse dos veces; cancelar ejecuta
cleanup de estado activo; los borrows de tareas no escapan sin join; raw pointers no adquieren
Send/Sync; y `volatile` queda separado de sincronización. No scheduler or global heap is imposed;
future state, cancellation, scoped borrows, structural thread safety, and memory order are explicit.

## Límite / Boundary

T10 congela protocolos y estados. La generación completa de state machines y MIR se conecta en T15.
T10 freezes protocols/states; full state-machine and MIR lowering is integrated in T15.

## Validación / Validation

`nsc` O0 y `nsc_ns3_t10_coroutine_tests`; sin baseline ni CTest general.

## Integración productiva 2026-09-29

`nsc` imprime, escribe, verifica y genera después de cada build un manifiesto de concurrencia
versionado. El contrato fija scheduler explícito, future lazy, storage caller-selected, cancelación
cooperativa, cleanup inverso, Send/Sync fail-closed, memory orders y la separación de `volatile`.
Evidencia: [TANDA-OPERATIVA-22-T10-CONCURRENCY-CONTRACT.md](TANDA-OPERATIVA-22-T10-CONCURRENCY-CONTRACT.md).

Esto no agrega `async/await`: parser, effects, frames, borrow-across-await, MIR/LLVM, metadata,
tooling y E026 permanecen abiertos.
