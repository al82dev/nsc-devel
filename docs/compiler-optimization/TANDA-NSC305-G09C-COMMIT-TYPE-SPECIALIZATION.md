# Tanda NSC305-G09c — Extraer `commitTypeSpecialization` (merge)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas deterministas: aislar el merge del producto en su slot reservado).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Aislar el merge de un clon producido en su slot reservado (nombre/metadata/attach/Ready) en `commitTypeSpecialization`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Merge del producto de especialización. |
| Revisión de referencia | `development` en `c04414e1`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `commitTypeSpecialization`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `commitTypeSpecialization` hace el merge (texto/metadata, attach, estado Ready, ítems asociados).
- `ensureTypeSpecialization` delega el merge; queda el patrón reservar → ejecutar → merge más explícito.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
