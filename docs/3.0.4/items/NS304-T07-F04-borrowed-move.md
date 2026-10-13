# NS304-T07-F04 — Items borrowed y move-only

- **Estado**: AUDITADO (item por value productivo; borrowed/move-only = surface de Alberto)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar lifetime de item prestado y transferencia move-only*.
Plan T07-F04 (DeepSeekDocs): *items prestados sólo viven hasta la siguiente demanda o destrucción;
items move-only se transfieren una vez; rechazar buffering/copia oculta, escape del borrow y avance
con préstamo mutable vivo*.

Estado real (tras F02):

- **Value** (default): productivo. El `AsyncYieldValue` copia el item a `item@16` del async-iterator
  y `Current` lo devuelve por valor.
- **Borrowed (`ref`/`ref mut`)**: sin surface (igual que T06-F05).
- **Move-only**: sin seguimiento de transferencia única.

## Gap

No hay modo `ref`/`ref mut` del item (un item prestado del frame del poll no puede expresarse), ni
rechazo del borrow que sobrevive a la siguiente demanda/`Drop`, ni transferencia move-only única.

## Decisión de surface (material, para Alberto)

Igual que T06-F05, aplicado a `AsyncIterator`:

- **(A) `AsyncIterator<ref T, E>`/`AsyncIterator<ref mut T, E>`** (recomendada): el modo en el tipo.
- **(B) `yield return ref v`**: modo por yield.

Recomendación: **(A)**.

## Plan (tras surface)

1. `Current` devuelve la referencia al slot del frame (no copia) mientras el borrow viva.
2. Rechazar (borrow-check) la conservación del item prestado tras `MoveNextAsync`/`Drop`.
3. Move-only: transferir el item una vez (rechazar copia/segundo uso).

## Nota

F04 depende de la surface de F05/T07-F01 (`ref`/`ref mut`); con value el invariante ya se cumple
(la copia a `item@16` no presta del frame).
