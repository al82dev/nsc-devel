# NS304-T05-F02 — Integrar Future lazy, Poll, Waker, resultado, error y cancelación

- **Estado**: CERRADO sólo como modelo de protocolo / residual (surface/runtime/lowering T05-F03)
- **Rama**: development

## Resultado

- El modelo de datos del protocolo contiene `FutureStart` (Lazy/Eager), `FutureProtocol`
  (`valueType`, `errorType`, `cancellationType`, `wakerType` + flags), `PollOutcome`,
  `FutureState` (poll/consume/cancel) y `AsyncGeneratorPollOutcome` (`94db6b58`).
- `wakerType` es metadata estructurada del modelo. No demuestra que `Waker` exista como tipo
  público resoluble ni que polling, wake/cancel o la máquina de estados tengan lowering/runtime.
- La superficie `Future<T,E,C>`/`Waker` y el lowering productivo pertenecen a T05-F03.
