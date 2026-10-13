# NS304-T07-F01 — AsyncIterator/AsyncGenerator

- **Estado**: AUDITADO (infraestructura de plan async-generator unit-tested, sin surface ni lowering)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Definir AsyncIterator/AsyncGenerator y demanda explícita del consumidor*.
Plan T07-F01 (DeepSeekDocs): *definir tipos asociados de item, error y future de siguiente demanda;
métodos poll/resume/drop; ownership del frame y estado terminal; evitar API basada en sentinels o
strings*.

Estado real (tras T05 + T06 productivos):

- **T05** aporta el future opaco (`future:<T>|<E>` + `pollFn`) y el `await`.
- **T06** aporta el iterator opaco (`iterator:<T>` + `moveNextFn`) y el `MoveNext/Current`.
- **Modelo async-generator**: `CoroutineSemantics` ya tiene `buildAsyncGeneratorPlan` (item/error/
  cancellation types, estados await+yield contiguos, backpressure explícito, `generatorResumeState`,
  `generatorCancellationDropOrder`). Está **unit-tested pero sin surface productiva** (grep de la
  ronda 42: `async generator` en `CoroutineSemantics.cpp`).
- **Surface**: no hay `AsyncIterator<T>`/`AsyncGenerator<T>` resoluble; no hay `await MoveNextAsync()`
  ni `Current`. La combinación await+yield en UNA máquina no tiene productor.

## Gap

No existe la surface que combina el future (await) con el iterator (yield) en una sola máquina. El
modelo async-generator está listo como contrato pero sin consumidor/lowering.

## Decisión de surface (material, para Alberto)

- **(A) `AsyncIterator<T>`/`AsyncIterator<T, E>`** (recomendada): el async generator devuelve
  `AsyncIterator<T, E>`; `await it.MoveNextAsync()` → `bool`, `it.Current` → `T`, `it.Drop()`. Coherente
  con `Iterator<T>` (T06) + `future:<T>|<E>` (T05).
- **(B) `AsyncGenerator<T>` + poll/resume explícitos**: surface más baja, similar al poll async.

Recomendación: **(A)**.

## Plan (tras surface)

1. Resolver `AsyncIterator<T, E>` → `async-iterator:<T>|<E>`.
2. Bajar el cuerpo async-generator a UNA máquina (estados await+yield), reutilizando el split async
   (T05) + el split generator (T06).
3. `MoveNextAsync()` devuelve un future de la siguiente demanda; `Current` el item; `Drop` el cleanup.

## Nota

T07 es la combinación de T05+T06 productivos. F01 es la surface + el tipo; la máquina única es F02.
