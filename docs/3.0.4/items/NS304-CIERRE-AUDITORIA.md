# NS304 — Resumen de cierre de auditoría (campaña 3.0.4)

- **Rama**: development · **Fecha**: ronda 164
- **Alcance recorrido**: NS304-T05-F03 → NS304-T17-F08 (secuencial, un expediente por ítem)

## Gate numérico (T17)

```text
items_T03_T17_inventariados == 122
items_T03_T17_cerrados   == 122   ← NO alcanzado (auditados, no cerrados)
items_totales_3_0_4_cerrados == 147  ← NO alcanzado
fallos_sin_clasificar == 0
diferidos_del_alcance == 0       ← NO (surfaces pendientes)
divergencias_host_selfhost == 0
casos_sin_evidencia == 0
```

## Superficies de Alberto pendientes (bloquean implementación)

| # | surface | tanda | opciones (recomendada primero) |
|---|---|---|---|
| 1 | arena/allocator del frame async | T05-F08 | `[FrameStorage(Arena)]` vs parámetro ctor |
| 2 | item `ref`/`ref mut` del generator | T06-F05 / T07-F04 | `Iterator<ref T>` (recomendada) vs `yield return ref` |
| 3 | error tipado del generator | T06-F07 | `Iterator<T,E>` / `throws E` |
| 4 | AsyncIterator surface | T07-F01 | `AsyncIterator<T,E>` + `MoveNextAsync` (recomendada) |
| 5 | cancelación | T07-F05 | `CancellationToken` (recomendada) vs sólo `Drop` |
| 6 | task scope | T07-F06 | `TaskScope` + `Spawn/Join` (recomendada) |
| 7 | Send/Sync + executor | T07-F07 | marcadores `Send`/`Sync` (recomendada) |
| 8 | operador extension | T08-F04 | witness estático de trait (recomendada) |
| 9 | allocator | T13-F01 | protocolo `IAllocator` (recomendada) |
| 10 | `ref-like` generic | T11-F05 | `where T : ref-like` (recomendada) |
| 11 | builder collection | T09-F05 | `[CollectionBuilder]` (recomendada) |
| 12 | interpolation handler | T10-F03 | `AppendLiteral/AppendFormatted` (recomendada) |

## Núcleos productivos ya cerrados (evidencia O0)

- **T05** async/await: future + poll + await (TL-LANG-328/329/370/377…).
- **T06** generadores: yield + MoveNext/Current/Drop + foreach (TL-LANG-381–388).
- **T07** async streams: AsyncIterator + máquina única await+yield (TL-LANG-385–387).
- **T08-F01** extension (TL-LANG-323/388) · **T09-F01–F04/F07/F08** construcción · **T10-F02/F04/F07** strings.

## Residuales de implementación (sin surface externa)

- ~~T08-F03 `T.Miembro()`~~ → **CERRADO** (ronda 166, `StaticMethodGroup` sin receiver).
- ~~`GetEnumerator` del iterator devolvía `void`~~ → **FIX** (regresión ronda 88, restaurado ronda 168).
- T06-F07 teardown automático de `foreach` → depende de **RAII de value-types** (feature amplio).
- T08-F02 ref getter → depende de **ref-returning property** (feature amplio).
- NSMETA/PDB (T05/T06/T07/T08/T16) → cola de metadata compartida.
- T07-F03 error tipado del future de demanda (acoplado a #3/#4).

## Criterio de reanudación

Con las surfaces decididas, reanudar por tanda en orden: cerrar cada F-item con prueba focal O0 +
registro en las tres superficies canónicas + commit aislado. T17 no empieza hasta T03–T16 cerradas.
