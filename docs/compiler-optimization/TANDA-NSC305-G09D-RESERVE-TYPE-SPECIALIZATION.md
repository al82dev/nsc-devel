# Tanda NSC305-G09d — Extraer `reserveTypeSpecialization` (reserva de slot)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas deterministas: aislar la reserva del slot + ordinal).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Aislar la reserva del slot + ordinal de una especialización de tipo (commit del planner + push_back del record Building) en `reserveTypeSpecialization`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Reserva y ordinal de especialización. |
| Revisión de referencia | `development` en `3e886d6a`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `reserveTypeSpecialization`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `reserveTypeSpecialization` reserva slot + ordinal (paso "reserve" de la onda).
- `ensureTypeSpecialization` queda como: reservar → ejecutar (clonar + processNode) → merge.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
