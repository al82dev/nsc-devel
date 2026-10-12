# NS304-T05-F02 — Integrar Future lazy, Poll, Waker, resultado, error y cancelación

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development

## Auditoría (línea base)

- El MODELO semántico ya existe en `Language/CoroutineSemantics.{h,cpp}`: `FutureStart` (Lazy/Eager),
  `FutureProtocol` (+ `validateFutureProtocol`), `PollOutcome` (Pending/ReadyValue/ReadyError),
  `FutureState` (poll/consume/cancel), `Waker`, y `AsyncGeneratorPollOutcome` para generadores.
- NO está integrado en Parser/Sema/HIR/LLVM: no hay `Future`/`Poll`/`Waker` en `.ns` ni bajan a
  código (T05-F03 hace la transformación a state machine).

## Gap F02

1. Exponer `Future<T, E, C>` (carrier opaco) como tipo nominal del stdlib o builtin, con su
   protocolo (`Poll`/`Resume`/`Drop`, single-consumer) validado por `validateFutureProtocol`.
2. Conectar `Waker` (despertar del executor) y la cancelación al `FutureState`.
3. Resultado/error explícitos en el `FutureProtocol` (value/error/cancellation types).

## Plan

1. Definir `Future<T>` + `Waker` en `lib/NativeSharp.Core` (o como builtin) y registrarlos en el
   name lookup para que `async` los resuelva.
2. Prueba `TL-LANG-322`.

## Prueba puntual

`TL-LANG-322`: declarar/consumir `Future<int>` y `Waker` (sin lowering productivo aún).
