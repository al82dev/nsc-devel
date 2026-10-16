# Tanda NSC305-G09g — Onda determinista (sort por clave estable)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas deterministas: ordenar la onda por clave estable).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `GenericTypeDemand::stableKey` y ordenar la onda por ella (proyecto | namespace | offset). |
| Naturaleza | Refactor no funcional (mismo conjunto, orden determinista). |
| Comportamiento idéntico | Mismas especializaciones; el orden de materialización queda determinista. |
| Revisión de referencia | `development` en `2232662f5`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `GenericTypeDemand::stableKey`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La demanda lleva `stableKey`; `executeTypeDemandWaves` ordena la onda antes de ejecutarla, de modo
  que el orden no dependa del orden de descubrimiento (base del merge determinista de G09).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
