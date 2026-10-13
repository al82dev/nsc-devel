# NS304-T05-F09 — ABI/NSMETA de future opaco, poll/resume/drop y consumo intermodular

- **Estado**: PARCIAL (Opción A cerrada en su núcleo: pollFn + constructor + await sobre variable; Future<T> nombrable + resume/drop + intermodular pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *emitir ABI/NSMETA de future opaco, poll/resume/drop y consumo intermodular*.

Estado real tras F03 (cerrado):

- El future es **uniforme** `future:<T> = { i32 state@0; void* frame@8; T result@16 }`. El poll es
  **por-función** (`<mangled>$poll`), y el `AwaitFuture` lo resuelve por `calleeName` (un call
  directo a función async).
- **Gap 1 — sin future opaco nombrable**: no existe `Future<T>` como tipo resoluble; el type-key
  `future:<T>` es interno. El usuario no puede nombrar un future (no puede declarar
  `Future<int> f;`).
- **Gap 2 — await sólo sobre call directo**: `await` exige `AsyncFunctionCall(...)`. `await` sobre
  una **variable** future no puede resolver el poll (el tipo uniforme no lleva el poll; el poll es
  por-función). Por eso no hay primitiva pendiente esperable.
- **Gap 3 — sin poll/resume/drop runtime**: no hay descriptor de future con puntero a poll, ni
  `resume`/`drop` emisibles; el consumo intermodular de un future opaco no existe.

## Opciones de diseño del ABI (decisión material)

- **(A) Poll virtual (puntero a función en el future)**: añadir `pollFn` al struct future
  (`{state, frame, result, pollFn}`); el constructor lo fija y el `await` llama el puntero. Permite
  `await` sobre cualquier future (incl. primitiva pendiente) sin tipos por-función. Cambia el
  layout (24→32 bytes) y el `AwaitFuture` (usa `pollFn`, no `calleeName`).
- **(B) Future por-función (`opaqueFutureIdentity`)**: el type-key `future:<T>#hash` lleva el poll
  por función; `await` sobre variable resuelve por tipo. Revierte el future uniforme (refactor del
  member model + Sema) y tiene el problema de orden del hash (frame → análisis de cuerpo).
- **(C) Builtin `Future<T>.PendingOnce()`**: exponer `Future<T>` (map a `future:<T>`) + reconocer
  el builtin en el HIR y generar un poll "pending once" (suspende la 1ª, completa la 2ª). Mínimo
  para demostrar `pending→ready`, pero no generaliza a `await` sobre variable arbitraria.

Recomendación: **(A) poll virtual** — es la forma canónica del "future opaco" (el future lleva su
poll/resume/drop), evita los tipos por-función y desbloquea tanto `await` sobre variable como la
primitiva pendiente. Es una decisión de ABI material (F09); se registra para Alberto.

### Cierre parcial (Opción A, fundación) — verificado 2026-10-13

- **Layout**: el future ahora lleva `pollFn` (campo 3, offset 24): `{state, frame, result, pollFn, error?}`;
  el error se mueve al índice 4. `future:builtin:int` = 32 bytes.
- **Constructor**: fija `pollFn = F$poll` vía el nuevo nodo `PollAddress` (HIR/MIR), que baja a la
  dirección de la función. IR verificado: `store ptr F$poll, ptr %future.field.addr`.
- **Partición**: `ProjectOopCodePartition::collectNode` declara el poll en la unidad que baja el
  `PollAddress` (referencia cruzada de unidad).
- Verificado: `TL-LANG-328/369-376` pass. Commit `7ddc45fb`.

Queda (Opción A, consumidor): el `await` debe **llamar `pollFn`** (hoy usa `calleeName`), `Future<T>`
nombrable (map a `future:<T>`) y `await` sobre **variable** — el hueco demostrado que consume el
pollFn.

### Cierre parcial (consumidor) — verificado 2026-10-13

- El HIR de `await` acepta un operando **variable** con tipo future (no sólo call directo);
  `calleeName` vacío marca el camino `pollFn`.
- El blocking await y el suspend/resume await cargan el poll desde `pollFn` (campo 3) y lo llaman
  con la firma reconstruida `void(ptr)`.
- Verificado: `var f = Read(); await f` resuelve por `pollFn` (exit 5/6). Registrado `TL-LANG-377`
  (PASS). Commit `f2f38892`.

Queda: `Future<T>` **nombrable** (map en name lookup a `future:<T>`), el runtime **resume/drop**
explícito y el **consumo intermodular** (future opaco cruzando módulos). Esos son la "cola" del ABI.

## Prueba puntual

`async int F(){ int v = await PendingOnce(); return v+1; }` + `Main(){ return await F(); }` debe
suspender y reanudar (`pending→ready`) y devolver 1, con el future llevando su poll (ABI).

## Nota

F09 desbloquea F04 (liveness) — la suspensión real es la única forma de ejercer el guardado de
locals vivos. Orden efectivo: F09 → F04 (liveness) → F05-F08.
