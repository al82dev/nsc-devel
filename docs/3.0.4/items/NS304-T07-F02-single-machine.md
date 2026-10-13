# NS304-T07-F02 — Máquina única para await + yield

- **Estado**: AUDITADO (modelo async-generator unit-tested; sin surface ni lowering)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar máquina única para await + yield sin frames duplicados*.
Plan T07-F02 (DeepSeekDocs): *fusionar puntos de suspensión en un discriminante y frame; no anidar un
future dentro de un generator con dos frames propietarios; el MIR debe mostrar estados y aristas de
await, yield, error, cancelación y fin*.

Estado real (tras T05 + T06 productivos):

- **T05** aporta el poll async (estados await, `SuspendPoint`, `AwaitFuture`) y el future opaco.
- **T06** aporta el MoveNext generator (estados yield, `YieldValue`, `YieldBreak`) y el iterator opaco.
- **Modelo**: `CoroutineSemantics` ya tiene `AsyncGeneratorPlan`, `buildAsyncGeneratorPlan`,
  `AsyncGeneratorSuspension{Await,Yield}`, `AsyncGeneratorState{Pending,ItemReady,Completed,Error,Cancelled}`
  y `AsyncGeneratorPollOutcome` — unit-tested pero **sin surface ni lowering**.
- **Surface**: `AsyncIterator<T,E>` resuelve (F01), pero no hay cuerpo async-generator productivo
  (`await` + `yield` en una función).

## Gap

Un callable que combina `await` + `yield` no se baja a UNA máquina: no hay split que fusione los
puntos de suspensión await+yield en un único discriminante/frame. El modelo está listo como contrato
sin consumidor.

## Plan

1. Marcar el callable async-generator (`Effect::Generator` + `Effect::Async` simultáneos) y bajar su
   cuerpo con `AwaitFuture` (await) + `YieldValue` (yield) a un **único** frame/discriminante.
2. `splitAsyncGeneratorFunction`: constructor + `MoveNextAsync` (devuelve un future de la siguiente
   demanda) reutilizando el split async (T05) + el split generator (T06) sin dos frames.
3. El MIR verifica estados/aristas de await, yield, error, cancelación y fin.

## Nota

T07-F02 es la combinación de T05-F03 + T06-F03; magnitud comparable a ambas. El modelo
`buildAsyncGeneratorPlan` ya valida estados contiguos + backpressure; el trabajo es el lowering
productivo.
