# NS304-T05-F05 — Verificar borrows, moves, pinning, self-references y escapes entre suspensiones

- **Estado**: PARCIAL (rechazo conservativo de borrow/pin across-suspension cerrado; re-localización y move = refinamiento)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *verificar borrows, moves, pinning, self-references y escapes entre suspensiones*.

El modelo de borrow/move/pin/escape YA EXISTE y es síncrono (punto de uso / return), pero **no
conoce la suspensión**:

- **Ownership**: `sema::OwnershipKind` = `None | Owned | Borrowed` (`Sema/OwnershipKind.h`).
- **Borrows**: `MethodBorrowKind` (8 variantes owner/borrowed/local/parameter/return +
  `FreshOwnedTemporaryToBorrowedParameter`) en `ProjectMethodBodyModel.h:494`. Bindings con
  `sourceLocalOrdinal`/`sourceParameterOrdinal`.
- **Escape**: `ProjectReferenceEscapeEvaluator.cpp` + checks en `ProjectMethodBodyModel.cpp`
  (`borrows from local '…'`, `cannot escape through return`, `stack storage and cannot escape the
  current method`). Es un análisis de RETURN síncrono.
- **Move/pin**: `local.isMoveOnly`, `local.isPinned` (T06-F06 `pin raii`: "no puede ser movido ni
  reemplazado tras inicialización"). Checks `cannot move pinned owner '…'`,
  `cannot move-spread pinned local`, `cannot move-capture pinned owner`.

**Hallazgo**: el `await` NO participa en ninguno de estos checks. `ProjectMethodBodyModel.cpp`
(case `AwaitExpression`, ~9703) solo resuelve `future:<T> → T`; `ProjectOopHirModel.cpp` (case
`AwaitExpression`, 8184) solo baja el call y construye `AwaitFuture`. No hay arista de borrow/move/
pin/escape que trate la suspensión como frontera de vida. El frame (F04) sólo guarda futures hijo,
no locals vivos; por tanto ningún borrow/pin/self-reference sobrevive a la suspensión.

## Gap (no demostrado aún — acoplado a F04/F09)

Ejemplo canónico: `async int F(){ int y = 10; borrowed int b = borrow y; int x = await G(); return b + x; }`
— `b` es una vista prestada de `y` que cruza el `await`. Hoy: (a) `y` no se guarda en el frame
(F04 liveness), y (b) la vista `b` no se revalida contra el slot del frame (self-reference). Un
move/pin a través del `await` tampoco se verifica.

**No demostrable en runtime todavía**: con F03 los futures completan sincrónicamente (no hay
primitiva pendiente, F09) y el frame no guarda locals (F04). El defecto sólo se observa con una
suspensión real. Es un gap de CORRECCIÓN latente, no un fallo observable hoy.

## Plan

1. Tras F04 (locals vivos en el frame): añadir la **frontera de suspensión** al análisis
   borrow/move/pin/escape — el `await` es un punto donde los locals vivos se mueven al frame y
   sus vistas prestadas deben re-apuntar al slot del frame (self-reference válida) o rechazarse.
2. Verificar cada valor vivo a través del `await`: borrow (origen aún vivo y re-localizado),
   move (permitido si no pinned), pin (identidad conservada en el frame), escape (no escapa del
   future).
3. Rechazo causal para vistas/moves/pins inválidos entre suspensiones (sin fallback silencioso).

## Prueba puntual

`async int F(){ int y = 10; int x = await G(); return x + y; }` (valor vivo) y una variante con
`borrow y`/`pin` a través del `await` deben compilar sólo si el borrow/pin/self-reference es
válido, y rechazar el escape/move inválido. Requiere la suspensión real de F09 para ejercerse.

## Nota

F05 depende de F04 (liveness: los locals vivos existen en el frame) y de F09 (suspensión real).
El transform de verificación no es ejercitable hasta F09; se implementará cuando el frame guarde
locals vivos y la primitiva pendiente exista. Orden efectivo: F09 → F04 → F05.

### Cierre parcial (rechazo conservativo) — verificado 2026-10-13

`analyzeCallable` ahora rechaza, tras el cuerpo, cualquier local `Borrowed` o `isPinned` en un
callable async con punto de suspensión (`await` o `yield;`). El `SuspendStatement` se registra
también como punto de suspensión. Verificado: `borrowed Box view` a través de `yield` se rechaza
(`async function cannot hold a borrowed or pinned local across a suspension`); `int value` a través
de `yield` sigue compilando (exit 10). Registrado `TL-LANG-374` (PASS). Commit `d84ff262`.

**Nota de precisión**: es conservativo (rechaza cualquier borrow/pin en un callable async con
suspensión, sin def-use liveness). La re-localización de vistas prestadas y la verificación de
move/self-reference quedan como refinamiento.
