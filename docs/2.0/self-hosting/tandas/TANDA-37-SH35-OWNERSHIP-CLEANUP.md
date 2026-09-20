# NativeSharp 2.0 self-hosting — tanda 37: ownership y cleanup

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH35` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `a9cf77580` |
| Commit de cierre | este commit (`selfhost(tanda-37): enforce ownership exit cleanup`) |

## Implementación

`TypeTable.Intern` procesa `owned`/`borrowed` antes de punteros y arrays; así `owned int*` conserva
el qualifier en el tipo exterior. Los hechos semánticos de variables ahora publican tipo, storage
y categoría. El parser conserva `raii`, y el backend trata `move` como transferencia de valor sin
crear una segunda asignación implícita.

MIR distingue `ownership.move`, `free`/`free.array` y añade `cleanup.if-live` para cada local owned,
en orden inverso de construcción, antes de las salidas. `OwnershipCleanupVerifier` rechaza una
función propietaria cuya salida carezca del marcador.

SH35 ejecuta una transferencia heap, escritura/lectura indexada y liberación; MIR prueba ambos
owners y el cleanup de salida. O0–O3 × LLD/NSLink devuelve 42.

- [x] identidad `owned`/`borrowed` en tipos compuestos.
- [x] transferencia `move` observable en MIR y backend.
- [x] cleanup de owners antes de retorno.
- [x] ejecución O0–O3 con ambos linkers.
- [ ] baseline acumulado final.
