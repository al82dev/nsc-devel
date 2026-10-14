# NS304-T06-F07 — Early stop, destrucción, errors, panic y cleanup determinista

- **Estado**: CERRADO (yield break + dropFn + drop-glue + it.Drop() + teardown foreach + Iterator<T,E>)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar early stop, destrucción, errors, panic y cleanup determinista*.
Plan T06-F07 (DeepSeekDocs): *drop explícito por abandono de `foreach`, `yield break`, error y unwind;
cada defer/drop corre una vez; un error tipado del generator conserva su tipo y no se confunde con fin
normal*.

Estado real:

- **`yield break`**: `YieldBreak` (LLVM) pone `state=-1` + `return false`, pero **no ejecuta drop/defer**
  de los locals vivos del generator.
- **Abandono de `foreach`**: el `foreach` tiene su propia teardown (`analyzeForeach`, línea 7423) que
  limpia el enumerador sintetizado, pero el **frame del generator** (sus locals/resource RAII) no se
  destruye al abandonar.
- **Error tipado**: el generator no tiene un `throws E` productivo (el item es el único tipo; no hay
  canal de error separado del fin normal). Es una surface no definida.
- **Panic/unwind**: el camino SEH del async (T05) no se reutiliza para el generator.

## Gap

El drop/defer de los locals vivos del generator no se ejecuta en `yield break` ni en el abandono del
`foreach`. No hay canal de error tipado (un error se confundiría con fin normal).

## Plan (pieza productiva: drop en yield break + abandono)

1. En `YieldBreak` (LLVM): ejecutar el cleanup de los locals vivos (drop/defer en orden inverso)
   antes de `return false`.
2. En el abandono del `foreach`: propagar la destrucción del frame del generator (sus slots vivos).
3. Error tipado (`throws E` de generator): surface pendiente (análogo a F07 async) — decisión de
   Alberto.

## Prueba puntual (pendiente)

Un generator con un resource RAII local; abandonar el `foreach` (o `yield break`) debe correr el drop
exactamente una vez. Un error tipado debe distinguirse del fin normal.

### Cierre parcial (yield break) — verificado 2026-10-13

- `YieldBreak` (HIR) adjunta las cleanup regions activas (continuación Return).
- `YieldBreak` (LLVM) ejecuta `emitOneStructuredCleanup` en orden inverso antes de `state=-1` +
  `return false`.
- Verificado: `Gen(){yield 1; yield break; yield 2;}` → MoveNext produce 1 y termina (exit 1).
  Registrado `TL-LANG-383` (PASS). Commit `8e8bd884`.

Queda: **abandono de `foreach`** (destruir el frame del generator al salir del loop) y **error tipado**
(`throws E` de generator) — surface pendiente.

### Cierre parcial (dropFn + it.Drop) — verificado 2026-10-13

- `iterator:<T>` lleva `dropFn` (campo 4); el constructor lo fija a `Gen$drop`.
- `Gen$drop` = `DropFrame(self)`: hereda los `asyncFrameLocals` del `$moveNext` y baja el drop-glue
  (`valueDropEntries_[typeKey](frame+offset)` en orden inverso).
- `it.Drop()` (core-op `IteratorDrop` + facade) llama `dropFn`.
- Verificado: `it.MoveNext(); it.Drop();` compila y corre (exit 0); IR muestra el null-check del frame
  + el bucle de drop-glue. Registrado `TL-LANG-384` (PASS). Commits `9479383c`, `cfc7885a`, `8c7ab115`.

Queda: el **teardown automático del `foreach`** (invocar `dropFn` al salir del loop, sin llamada
explícita) y el **error tipado**.

### Teardown del foreach — cerrado 2026-10-13

El `__enumerator` del foreach ahora es RAII para `iterator:`/`async-iterator:` (porta Drop fn),
`typeRequiresValueDrop` los incluye, y el RAII destruye el value-type por la drop-glue
(`__nativesharp_drop_iterator` → `Gen$drop`). Verificado: `foreach (x in Gen()) { break; }` corre el
destructor del frame (exit 1). Commits `dd464f72`, `...` + `TL-LANG-391` PASS.
