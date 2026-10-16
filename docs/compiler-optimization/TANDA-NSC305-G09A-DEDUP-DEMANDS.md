# Tanda NSC305-G09a — Deduplicar demandas por stableKey (onda)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas deterministas: primer paso, deduplicar demandas por clave estable).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Deducir demandas de especialización de cuerpo por `stableKey` en O(1) y saltar los duplicados (paso 1 de la onda del planner). |
| Naturaleza | Refactor no funcional (misma deduplicación, ahora real + O(1)). |
| Comportamiento idéntico | Materialización de vistas ReadOnlySpan/Span; el caché ya colapsaba los duplicados. |
| Revisión de referencia | `development` en `daa025b8`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `bodyTypeDemandKeys_`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `bodyTypeDemandKeys_` (conjunto) deduplica por `stableKey` en O(1); los duplicados se saltan.
- El conjunto se limpia tras cada onda procesada.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
