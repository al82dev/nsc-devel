# NS304-T07-F05 — Cancelación y cleanup bilateral

- **Estado**: AUDITADO (drop explícito cerrado; token de cancelación + cleanup bilateral pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar cancelación y cleanup bilateral*.
Plan T07-F05 (DeepSeekDocs): *propagar token/solicitud hacia awaits, productor e hijos; el consumidor
que abandona destruye el productor; error del productor finaliza consumidor; drops/defer/finally
corren una vez aun con carreras de cancelación/completion*.

Estado real (tras F02/F03):

- **Abandono del consumidor**: el async-iterator tiene `Drop()` (dropFn → drop-glue de F06) que
  destruye el frame del productor.
- **Error del productor → consumidor**: pendiente (el `future:<bool>` del demand no lleva error, F03).
- **Token de cancelación**: sin surface; no hay propagación de una solicitud de cancelación hacia los
  `await` internos del poll.

## Gap

No hay token de cancelación que el productor observe en sus puntos `await`/`yield`; la cancelación
no llega al poll en curso (sólo el `Drop()` del consumidor, que es un abandono brusco).

## Decisión de surface (material, para Alberto)

- **(A) `CancellationToken` explícito** (recomendada): un token que el poll comprueba en cada
  `await`/`yield` y que un `it.Cancel()` señala.
- **(B) cancelación por `Drop()`** únicamente (sin token observable): el abandono es la única señal.

Recomendación: **(A)**.

## Plan (tras surface)

1. Resolver `CancellationToken`/`it.Cancel()` + comprobar el token en los puntos de suspensión del poll.
2. Cleanup bilateral: `Drop`/defer/finally del productor corren una vez ante cancelación o completion.
3. Error del productor → future de demanda con error (cierra F03).

## Nota

F05 depende de F03 (error) y de la surface de cancelación. El `Drop()` explícito ya cubre el abandono
brusco; falta la cancelación cooperativa y el cleanup de carreras.
