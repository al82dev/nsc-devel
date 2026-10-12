# NS304-T05-F02 — Integrar Future lazy, Poll, Waker, resultado, error y cancelación

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development

## Auditoría (línea base)

- El MODELO semántico ya existe en `Language/CoroutineSemantics.{h,cpp}`: `FutureStart` (Lazy/Eager),
  `FutureProtocol` (+ `validateFutureProtocol`), `PollOutcome` (Pending/ReadyValue/ReadyError),
  `FutureState` (poll/consume/cancel), y `AsyncGeneratorPollOutcome` para generadores.
- **Falta `Waker`**: no está en `CoroutineSemantics.h` (no hay tipo ni protocolo de wake). El resto
  del modelo (resultado/error/cancelación) sí está.
- NO está integrado en Parser/Sema/HIR/LLVM (T05-F03 hace la transformación a state machine).

## Gap F02

1. Añadir `Waker` (callback de readiness al executor) al modelo y al protocolo.
2. Exponer `Future<T, E, C>` (carrier opaco) + `Waker` como tipos resolubles.
3. Conectar `Waker` y cancelación al `FutureState`.
