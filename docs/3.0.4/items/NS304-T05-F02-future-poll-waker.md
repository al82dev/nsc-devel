# NS304-T05-F02 — Integrar Future lazy, Poll, Waker, resultado, error y cancelación

- **Estado**: CERRADO (modelo) / residual (surface/lowering T05-F03)
- **Rama**: development

## Resultado

- El MODELO semántico está completo: `FutureStart` (Lazy/Eager), `FutureProtocol` (valueType,
  errorType, cancellationType, **wakerType** + flags), `PollOutcome`, `FutureState`
  (poll/consume/cancel), `AsyncGeneratorPollOutcome`. `Waker` añadido (`94db6b58`).
- El `Future<T,E,C>`/`Waker` como tipos resolubles y el lowering productivo son T05-F03.
