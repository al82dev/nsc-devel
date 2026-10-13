# NS304-T06-F05 — Items por value, move, ref y ref mut con lifetime asociado

- **Estado**: AUDITADO (value ya productivo; move/ref/ref mut = surface pendiente de Alberto)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Soportar items por value, move, ref y ref mut con lifetime asociado*.
Plan T06-F05 (DeepSeekDocs): *definir tipo y lifetime de item por modo; `move` transfiere una vez;
`ref`/`ref mut` quedan ligados al frame y a la demanda actual; rechazar conservación del borrow
después de avanzar o destruir el iterator*.

Estado real:

- **Value** (default): productivo end-to-end (F03/F08/F04). El item se copia al slot `item@16` del
  iterator y `Current` lo devuelve por valor.
- **move/ref/ref mut**: sin surface. El modelo `IterationSemantics` ya define `IterationMode`
  (`Value/Reference/MutableReference`) y `GeneratorYieldKind` (`Value/SharedReference/MutableReference`),
  y `GeneratorSuspension.kind`/`lifetimeTiedToFrame`, pero ningún productor los fija.
- **foreach** (consumidor): ya tiene binding mode (`foreach (T v in s)` / `foreach (ref T v in s)` /
  `foreach (ref mut T v in s)`, `Parser.cpp:12917`), pero no se conecta con un item `ref`/`ref mut`
  del generator (el `Current` devuelve siempre por valor).

## Gap

No hay modo de item `ref`/`ref mut` (ni `move`); el `Current` es siempre por valor. Un item prestado
del frame (autoreferencial) no puede expresarse, y no hay rechazo del borrow que sobrevive a la
siguiente demanda.

## Decisión de surface (material, para Alberto)

Cómo declara el generator el modo de item:

- **(A) `Iterator<ref T>`/`Iterator<ref mut T>`** (recomendada): el modo viaja en el argumento del
  iterator, coherente con `Iterator<T>`; `Current` devuelve `ref T`/`ref mut T` ligado al frame.
- **(B) `yield return ref v`** (por yield): el modo es por punto de yield, no por tipo.

Recomendación: **(A)** — el modo es una propiedad del iterator (un generator es homogéneo), no de
cada yield.

## Plan (tras surface)

1. Resolver `Iterator<ref T>`/`Iterator<ref mut T>` → `iterator:ref:<T>`/`iterator:refmut:<T>`.
2. `Current` devuelve la referencia al slot del frame (no una copia); `MoveNext` no adelanta el slot
   mientras un borrow esté vivo.
3. Rechazar (borrow-check) la conservación del item prestado después de `MoveNext`/destrucción.

## Prueba puntual (pendiente)

Un generator autoreferencial que presta un local del frame como item, consumido por `foreach (ref T ...)`;
y un rechazo cuando el item prestado intenta sobrevivir a la siguiente demanda.
