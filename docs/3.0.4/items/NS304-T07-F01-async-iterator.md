# NS304-T07-F01 — AsyncIterator/AsyncGenerator

- **Estado**: CERRADO (surface `async-iterator:<T>|<E>` + `AsyncIterator<T,E>` productiva)
- **Rama**: development

## Cierre

- `AsyncIterator<T,E>` resuelve a `async-iterator:<T>|<E>` (surface A). El consumer usa
  `await it.MoveNextAsync()` → `bool` y `it.Current` → `T`.
- La máquina única await+yield (F02) y el demand future (`MoveNextAsync` → `future:<bool>|<E>`)
  quedan cerrados en F02. Evidencia: `TL-LANG-385` (MoveNext síncrono), `TL-LANG-386`
  (`await it.MoveNextAsync()`), `TL-LANG-387` (máquina única `await Fetch()` + `yield`).

## Auditoría (línea base)

Contrato (tracking): *Definir AsyncIterator/AsyncGenerator y demanda explícita del consumidor*.
Plan T07-F01 (DeepSeekDocs): *definir tipos asociados de item, error y future de siguiente demanda;
métodos poll/resume/drop; ownership del frame y estado terminal; evitar API basada en sentinels o
strings*.

Estado real (tras T05 + T06 productivos):

- **T05** aporta el future opaco (`future:<T>|<E>` + `pollFn`) y el `await`.
- **T06** aporta el iterator opaco (`iterator:<T>` + `moveNextFn`) y el `MoveNext/Current`.
- **Surface**: `AsyncIterator<T,E>` → `async-iterator:<T>|<E>` con `MoveNextAsync`/`Current`.

## Decisión de surface (aprobada)

- **(A) `AsyncIterator<T>`/`AsyncIterator<T, E>`** (adoptada): el async generator devuelve
  `AsyncIterator<T, E>`; `await it.MoveNextAsync()` → `bool`, `it.Current` → `T`, `it.Drop()`.
