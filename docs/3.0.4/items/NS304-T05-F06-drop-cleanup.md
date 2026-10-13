# NS304-T05-F06 — Ejecutar drop/defer/finally una vez en completion, error o cancelación

- **Estado**: ABIERTO (auditado; drop en completion perdido; cancelación sin path)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *ejecutar drop/defer/finally una vez en completion, error o cancelación*.

Infraestructura existente (independiente de async):

- `finalizeCleanupContract` (`ProjectOopMirModel.cpp:30`) recorre el cuerpo MIR y recoge:
  - `cleanupRegions`: `LocalOwner` (LocalDeclaration), `TemporaryOwner` (OwnedTemporaryBorrow),
    `OwnedParameter` (OwnedParameterCleanup), `Using` (UsingRegion), `Finally` (FinallyRegion).
  - `nonlocalExits`: `Return`/`Throw`/`Panic`/`CleanupContinuation` con su `cleanupRegionOrdinals`.
- El lowering LLVM emite el drop **en el exit no-local** (`ProjectOopLlvmLowerer.cpp:6692-6754`):
  `complexExit = !node->cleanupRegionOrdinals.empty()`, y recorre `node->cleanupRegionOrdinals`
  para disparar cada `emitOneStructuredCleanup`. Es decir, el drop de un owner al salir del scope
  viaja en los `cleanupRegionOrdinals` del nodo de salida (`Return`/`Throw`).

Estado real del poll async (tras F03):

- El poll hereda el cuerpo async reescrito: los **nodos** de cleanup (`LocalDeclaration`,
  `FullExpressionCleanup`, `FinallyRegion`) se conservan, y `finalizeCleanupContract` sí corre
  sobre el poll (`ProjectOopMirModel.cpp:1501`).
- **PERO** `rewriteAsyncPollReturns` (`ProjectOopHirModel.cpp:3590-3638`) reescribe
  `return v` → `{ self->result = v; self->state = -1; return; }`. El `return;` final es un nodo
  nuevo (`ProjectOopHirModel.cpp:3637`) que **NO copia** los `cleanupRegionOrdinals` del `return v`
  original. Por tanto el drop de owners vivos al completar **no se emite**.

## Gap demostrable

`async int F(){ var box = new Box(); return 42; }` con `Box` de destructor contador: al completar,
el destructor de `box` debería correr **una vez**. Hoy el `return 42` se reescribe a un `return;`
sin `cleanupRegionOrdinals`, así que el drop no se emite y el destructor **no corre** (contador 0
en vez de 1). Es demostrable **sin F09** (el caso completa sincrónicamente; no requiere primitiva
pendiente).

Lo mismo aplica a `finally`/`using` cuyo disparo depende del exit de `Return`.

## Gap no demostrable (acoplado a F09)

- **Cancelación**: no existe path de cancelación/drop del future (el ABI `poll/resume/drop` es
  F09). El drop en cancelación no puede observarse hasta F09.
- **Suspensión**: el drop **no** debe correr al suspender (los owners vivos cruzan el `await`); su
  drop correcto al completar depende además del liveness de F04 (el valor debe sobrevivir a la
  suspensión para que su destructor corra sobre el valor correcto).

## Plan

1. En `rewriteAsyncPollReturns`, conservar los `cleanupRegionOrdinals` del `Return` original en el
   `return;` sintetizado (o envolver el store de estado/resultado en un cleanup path equivalente).
2. Garantizar exactly-once: el drop se emite sólo en el poll terminal (estado `-1`), no en los
   `return;` de suspensión (los de `AwaitFuture` ya `ret` sin cleanup).
3. Con F09: emitir el drop en cancelación y en el drop del future (single path compartido).

## Prueba puntual

`async int F(){ var box = new Box(); return 42; }` con destructor contador debe ejecutar el
destructor exactamente una vez al completar (sin F09). Con F09, también una vez en cancelación y
una vez en error, y cero en suspensión intermedia.

## Nota

El gap de completion es independiente de F09 y demostrable hoy. El gap de cancelación/error depende
de F09 (ABI drop) y de F07 (errores). El orden efectivo: F09 → F04 (liveness) → F06 (drop), porque
el drop al completar debe correr sobre el valor correcto restaurado por F04.
