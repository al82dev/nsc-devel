# NS304-T07-F06 — Task scope, spawn/join y child failure

- **Estado**: IMPLEMENTADO (surface #6/#7 de `NS304-DECISIONES-SURFACE.md`)
- **Rama**: development

## Surface final

- `IExecutor { int Spawn(Future<void>); int PollJoin(int); void Cancel(int); }` (`PollJoin`:
  `-1` pendiente / `0` ok / `1` fallo, idempotente; `Cancel` no reemplaza a `Join`).
- `TaskScope<TExecutor> where TExecutor : IExecutor` + `TaskScope<TExecutor>.Create(executor)`.
- `scope.Spawn(Future<void>)` (sin lambda async); `void Join()` + `bool Failed()`; `ChildCount()`.
- `Join` **idempotente** (flag `active` por handle), **child failure** (`PollJoin == 1` → `failed`
  y cancel+join de los restantes en orden inverso), **destructor** `~TaskScope()` (cancel+join
  inverso, cada operación una vez), estado Open/Joining/Closed.

## Captura `ref` del async function

- El `$poll` lleva `generatorParameterTypeKeys` + `isGeneratorConstructor` (HIR); MIR registra
  `asyncFrameParameters`; LLVM `GeneratorFrameInit` copia los parámetros al frame.
- `ParameterValue`/`ParameterAddress` distinguen `self` (ordinal 0, `future:<T>*`) de un `ref`
  capturado (ordinal 0, `T*`) por ordinal + type key, de modo que la escritura llega al storage
  original a través del `yield`. `TL-LANG-459` (exit 42).

## Provenance del borrow (scoped child borrow)

- `scope.Spawn(Child(ref value))` registra el origen `CurrentFrameLocal` del `ref` en
  `scopedChildBorrows_`; `scope.Join()` lo descarga; salir del bloque que posee `value` con el
  borrow vivo rechaza con `scoped child borrow outlives its source; join or cancel the TaskScope
  before leaving the source lifetime`. `TL-LANG-460` (rechazo) / `TL-LANG-461` (Join descarga, 42).

## Pruebas

- `TL-LANG-457` spawn/join básico (exit 2); `TL-LANG-458` child failure (exit 7); `TL-LANG-459`
  captura `ref` (exit 42); `TL-LANG-460` borrow escape (rechazo); `TL-LANG-461` borrow join (42);
  `TL-LANG-462` cleanup inverso del destructor (exit 7); `TL-LANG-463` Join idempotente (exit 7).

## Commits

- `a0121a53` — captura `ref` del async function.
- `9cc14132` — Join idempotente + destructor inverso.
- `87dd975d` — provenance: borrow que sobrevive al source se rechaza.
