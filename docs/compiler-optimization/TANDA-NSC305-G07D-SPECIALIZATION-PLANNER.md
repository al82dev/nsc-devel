# Tanda NSC305-G07d — `SpecializationPlanner::request` (demanda deduplicada)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G07 (cierre: planner de deduplicación de demandas de especialización).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Encapsular la deduplicación y el ciclo de vida del slot (`request`/`commit`/`rollback`) en un `SpecializationPlanner`, y hacer que `ensure*Specialization` delegue en él. |
| Naturaleza | Refactor no funcional (misma deduplicación; la pide el planner). |
| Comportamiento idéntico | Deduplicación por clave, commit y rollback. |
| Revisión de referencia | `development` en `a9d5fa36`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `SpecializationPlanner` (`request`/`commit`/`rollback`), `specializationPlanner_`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `SpecializationPlanner` posee `byKey` y expone `request` (hit ordinal o nullopt), `commit` y `rollback`.
- `ensure*Specialization` y la siembra usan el planner en lugar de `specializationByKey_` directo.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.

## 4. Resultado de G07

- Extracción clave/sustituciones (G07a), `SpecializationContext` (G07b), `SpecializationState` (G07c) y
  `SpecializationPlanner` (G07d). G07 queda cerrado.
