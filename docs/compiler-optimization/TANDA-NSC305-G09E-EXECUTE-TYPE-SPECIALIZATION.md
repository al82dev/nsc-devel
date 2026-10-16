# Tanda NSC305-G09e — Extraer `executeTypeSpecialization` (ejecución)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas deterministas: aislar la ejecución de la reserva/merge).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Aislar la ejecución de una especialización de tipo reservada (sustituciones + clonar + commit + processNode + restauración) en `executeTypeSpecialization`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Ejecución y restauración de contexto. |
| Revisión de referencia | `development` en `58c1c716`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `executeTypeSpecialization`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `executeTypeSpecialization` ejecuta (clonar + commit + processNode + restaurar) y devuelve `false`
  ante error; el caller hace rollback + `leave()` del presupuesto.
- `ensureTypeSpecialization` queda: reservar → ejecutar → merge, con el rollback explícito.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
