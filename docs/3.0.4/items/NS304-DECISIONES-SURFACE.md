# NS304 — Decisiones de surface (tomadas, ronda 171)

Las 12 superficies se resuelven con las opciones recomendadas. Cada una se implementa en su tanda.

| # | surface | decisión |
|---|---|---|
| 1 | arena/allocator del frame async (T05-F08) | `[FrameStorage(Arena)]` / parámetro ctor — **A: atributo `[FrameStorage(...)]`** |
| 2 | item `ref`/`ref mut` generator (T06-F05/T07-F04) | **A: `Iterator<ref T>`/`Iterator<ref mut T>`** |
| 3 | error tipado generator (T06-F07) | **`Iterator<T,E>`** (2 args, como AsyncIterator) |
| 4 | AsyncIterator surface (T07-F01) | **`AsyncIterator<T,E>` + `await it.MoveNextAsync()`** |
| 5 | cancelación (T07-F05) | **`CancellationToken` + `it.Cancel()`** |
| 6 | task scope (T07-F06) | **`TaskScope` + `scope.Spawn(...)`/`scope.Join()`** |
| 7 | Send/Sync + executor (T07-F07) | **marcadores `Send`/`Sync` + `IExecutor`** |
| 8 | operador extension (T08-F04) | **witness estático de trait** |
| 9 | allocator (T13-F01) | **protocolo `IAllocator` (`Allocate/Resize/Deallocate`)** |
| 10 | ref-like generic (T11-F05) | **`where T : ref-like`** |
| 11 | builder collection (T09-F05) | **`[CollectionBuilder]` + `CreateBuilder`** |
| 12 | interpolation handler (T10-F03) | **`AppendLiteral`/`AppendFormatted` handler** |

## Orden de implementación restante

1. T06-F05/T07-F04 item ref/ref mut (#2) → desbloquea T06-F06 pinning.
2. T06-F07 `Iterator<T,E>` (#3) → error tipado generator.
3. T05-F08 `[FrameStorage]` (#1) → arena async.
4. T07-F03–F08 (#4/#5/#6/#7) → async streams completo.
5. T08-F04–F07 (#8) → operadores/conformance extension.
6. T13 (#9) → allocators (desbloquea T06-F06).
7. T11 (#10) + T15 + T16 → spans/decimal/ABI.

Nota: el teardown de `foreach` (T06-F07) y el ref getter (T08-F02) dependen de features amplios
(RAII de value-types, ref-returning property), no de estas superficies.
