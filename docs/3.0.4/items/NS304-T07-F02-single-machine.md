# NS304-T07-F02 — Máquina única para await + yield

- **Estado**: PARCIAL (MoveNextAsync ready-future + tramo síncrono cerrados; await dentro del cuerpo generator pendiente)
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

### Cierre parcial (marcación + guard) — verificado 2026-10-13

- `AsyncIterator<T,E>` devuelto → efectos `Generator` + `Async` simultáneos.
- `yield` es válido en un async-generator (el item es `T`).
- El HIR rechaza el cuerpo de doble efecto con `NS304-T07-F02` explícito (sin apilar los dos splits).
- Verificado: `AsyncIterator<int,object> Gen(){ yield 1; }` → error claro. Commit `cf188c1`.

Queda: **el split único** (`MoveNextAsync` → future de la siguiente demanda) y el MIR de estados/aristas.

### Tramo síncrono cerrado — verificado 2026-10-13

- `splitGeneratorFunction` acepta `async-iterator:<T>|<E>` (item = T); el async-generator baja por el
  split generator (MoveNext/Current/GetEnumerator/Drop síncronos).
- facade + `llvmType` aceptan el receiver/layout `async-iterator:`.
- Verificado: `AsyncIterator<int,int> Gen(){ yield 1; yield 2; }` → MoveNext/Current sum=3 (exit 3).
  Registrado `TL-LANG-385` (PASS). Commits `3af99a2d`, `05963358`.

Queda: **`MoveNextAsync`** (el future de la demanda) + el `await` en la misma máquina.

### MoveNextAsync ready-future cerrado — verificado 2026-10-13

- `MoveNextAsync()` (core-op + facade) devuelve `future:<bool>`; el LLVM ejecuta el MoveNext síncrono
  eager, `state=-1` + `result=hasNext`.
- El blocking await comprueba el state ANTES de hacer poll (el future listo salta el poll).
- Verificado: `await it.MoveNextAsync()` impulsa el generator (sum=3). Registrado `TL-LANG-386` (PASS).
  Commit `6639b4db`.

Queda: **`await` dentro del cuerpo generator** (la máquina única await+yield con suspend/resume).

### ABI del future-de-demanda (recomendado)

- **(A) `future<bool>` + poll interno**: `await it.MoveNextAsync()` → `bool`; el poll del future ejecuta
  el cuerpo async-generator hasta el siguiente `yield`/`await`. Reutiliza `future:<bool>|<E>` de T05.
- (B) `future<item>` directo (descartado por ahora).

Split planeado: constructor `Gen` (async-iterator + `moveNextFn = Gen$moveNext`) → `Gen$moveNext`
(`future<bool>`, fija `pollFn = Gen$moveNext$poll`) → `Gen$moveNext$poll` (cuerpo con yield/await, un
frame).
