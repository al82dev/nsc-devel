# NS304-T07-F03 — Backpressure y orden

- **Estado**: PARCIAL (backpressure/orden ✅; error tipado del future ✅; post-terminal = refinamiento)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar backpressure y orden observable*.
Plan T07-F03 (DeepSeekDocs): *una demanda produce a lo sumo un item o pending/terminal; prohibir
producción anticipada no contratada; congelar orden observable, propagación de error y comportamiento
después del terminal*.

Estado real (tras F02):

- **Una demanda → un item**: el `AsyncYieldValue` del poll produce UN item y **retorna** (suspende).
  El siguiente `MoveNextAsync` crea un future nuevo y el poll retoma en el estado de resume. No hay
  producción anticipada (el cuerpo sólo avanza en el poll).
- **Orden observable**: los yields numerados 1..N se producen en orden de fuente (TL-LANG-386: 1, 2).
- **Terminal**: el `return false` final del cuerpo → `rewriteAsyncPollReturns` → `result=false` +
  `state=-1` (fin normal).

## Gap

- **Propagación de error**: `MoveNextAsync` devuelve `future:<bool>` **sin tipo de error**; el error
  del async-generator (`AsyncIterator<T,E>` con `await` que falla) no viaja al future de demanda.
- **Comportamiento después del terminal**: pedir otra demanda después del `result=false` no está
  definido/verificado.

### Error tipado del future — verificado 2026-10-13

- `MoveNextAsync` ahora devuelve `future:<bool>|<E>` (el error E del async-iterator). Commit `5ddc9132`.
  La propagación runtime (un `await` que falla) depende de F05 (cancelación/error) + `throws` del poll.

## Nota

El núcleo de F03 (backpressure + orden) ya lo entrega el diseño del poll de F02. El error tipado del
future de demanda y el post-terminal son el refinamiento pendiente (acoplado al `throws E` de F01/F02).
