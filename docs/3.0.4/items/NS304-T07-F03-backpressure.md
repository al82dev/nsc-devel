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

- **Propagación de error runtime**: un `await` que falla dentro del async-generator no viaja al future
  de demanda (acoplado a `throws E` del poll, F05/F07). El tipo `future:<bool>|<E>` ya es productivo.
- **Comportamiento después del terminal**: CERRADO — el terminal es sticky (`TL-LANG-400`: una
  demanda tras el `result=false` devuelve `false` otra vez, exit 1).

### Error tipado del future — verificado 2026-10-13

- `MoveNextAsync` ahora devuelve `future:<bool>|<E>` (el error E del async-iterator). Commit `5ddc9132`.
  La propagación runtime (un `await` que falla) depende de F05 (cancelación/error) + `throws` del poll.

## Nota

El núcleo de F03 (backpressure + orden + terminal sticky) ya lo entrega el diseño del poll de F02 +
`TL-LANG-400`. El único residual es la propagación runtime del error (acoplado a `throws E` de F01/F02/F05).
