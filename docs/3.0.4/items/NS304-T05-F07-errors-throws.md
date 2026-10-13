# NS304-T05-F07 — Integrar throws E, Result, ?, panic y unwind nativo con async

- **Estado**: PARCIAL (superficie future lleva E; slot de error + ReadyError + ?/Result pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking, línea 1005): *integrar `throws E`, `Result`, `?`, panic y unwind nativo con async*.

Estado real tras F03 (cerrado):

- **`async throws E` SÍ resuelve en el member model**: `ProjectMemberModel.cpp:1867-1870` fija
  `declaredErrorTypeKey = E` (canonical del `throwsTypeSyntax`) y `1994-1995` añade
  `Effect::Throws`. El HIR copia `function.errorTypeKey` al callable y al poll
  (`ProjectOopHirModel.cpp:1656`, `2051`, `3681`).
- **PERO la superficie `future:<T>` pierde el error**: `futureTypeKey(inner) == "future:<inner>"`
  (`CoroutineSemantics.h:27-31`) sólo envuelve el tipo de VALOR; no hay `E` ni `C` en el type-key.
  `async throws E T F()` queda `future:<T>` (la `E` se descarta del tipo resoluble).
- **El struct future no tiene slot de error**: layout uniforme `{ i32 state@0; void* frame@8;
  T result@16 }` (F03). No hay campo de error ni discriminante `ReadyError`.
- **El `AwaitFuture` no tiene camino de error**: sólo chequea `child.state == -1` (ready) y carga
  `child.result` (offset 16). No hay rama `ReadyError`.
- **El modelo F02 ya lo contempla pero NO está cableado**: `FutureProtocol.errorType` /
  `cancellationType` (`CoroutineSemantics.h:79-91`), `PollOutcome::ReadyError` /
  `FutureCompletion::Error` (`:75-77`) y `FutureState::poll(PollOutcome)` (`CoroutineSemantics.cpp:180-205`)
  existen SÓLO en el modelo unit-tested; ningún archivo de `src/Frontend`/`src/LLVM` los invoca.
- **panic/unwind**: panic es abort-profile (`PanicProfile::Abort`) y baja a nodo divergente; el
  unwind nativo es SEH. No hay integración de esos dos con el future (no hay "future panicked").

## Gap demostrable

- **Estático (observable sin runtime)**: `async throws E F()` produce `future:<T>`, no
  `future:<T,E>`; el error está ausente del type-key, del struct y del `AwaitFuture` (sin
  `ReadyError`). El `Effect::Throws` se pierde en el futuro: el poll hereda `errorTypeKey` pero
  no hay superficie que lo transporte al consumidor.
- **Runtime (no demostrado aún)**: un `throw` dentro del cuerpo async no puede poblar el futuro
  (no hay slot de error ni `ReadyError`); hoy el `throw` del poll **unwindearía nativo (SEH)** en
  vez de propagarse como error del future. Observarlo exige un cuerpo async que lance + un
  consumidor que lea el error; no hay lowering `ReadyError` que lo haga posible.

**Acoplamiento**: el error del future sólo se OBSERVA en runtime cuando el `await` suspende y el
productor falla (F09 primitiva pendiente + F03 suspend). Sin F09 el camino `ReadyError` no se
ejecuta; pero el gap de SUPERFICIE (E perdida del type-key) ya es demostrable estáticamente.

## Plan

1. Superficie: ampliar el type-key a `future:<T,E,C>` (o `future:<T>` con `errorTypeKey` asociado
   en el plan) y el struct con un slot de error + discriminante `ReadyError`.
2. `AwaitFuture`/poll: añadir el camino `ReadyError` — si el hijo completa con error, propagar el
   error al futuro padre (estado de error + slot de error) sin unwinder nativo.
3. `?`/`Result`: integrar `await` sobre `future:<Result<T,E>>` y el operador `?` a través del await;
   `panic`/unwind nativo como camino de terminación explícito (no como propagación de `throws E`).

## Prueba puntual

`async throws E F(){ throw ...; }` + `int Main(){ try { return await F(); } catch (E) { return 7; } }`
debe propagar el error como `ReadyError` del future (no unwind nativo), y `?` debe funcionar a
través del `await`. (La ejecución real depende de F09 para el suspend.)

## Nota

F07 depende de F09 (suspensión real) para la ejecución runtime del camino de error; pero el gap de
superficie (`future:<T>` sin `E`) es estático y ya demostrable. El `errorType`/`cancellationType`
de F02 está listo como modelo y es el contrato a cablear.

### Cierre parcial (superficie con E) — verificado 2026-10-13

- `futureTypeKey(inner, error)` → `future:<inner>|<error>`; `futureErrorTypeKey` desenvuelve `E`;
  `futureInnerTypeKey` ignora el sufijo `|<error>`.
- Member model envuelve `declaredTypeKey` con `declaredErrorTypeKey` para `async throws E`.
- Verificado: `async int F() throws Exception` produce
  `future:builtin:int|type:NativeSharpCore::NativeSharp.Core.Exception` (HIR dump) y ejecuta exit 42.
  Registrado `TL-LANG-375` (PASS). Commit `1c07d46a`.

Queda: slot de error en el struct + camino `ReadyError` en `AwaitFuture` (un `throw` en el cuerpo
aún unwindea nativo SEH) y `?`/`Result` a través del await. Esos son el runtime, acoplados a F09.
