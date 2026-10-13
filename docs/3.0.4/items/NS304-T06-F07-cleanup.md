# NS304-T06-F07 — Early stop, destrucción, errors, panic y cleanup determinista

- **Estado**: AUDITADO (drop/defer en yield break/abandono pendiente; error tipado = surface)
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
