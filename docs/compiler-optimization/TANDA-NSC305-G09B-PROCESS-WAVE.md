# Tanda NSC305-G09b — Extraer `processBodyTypeDemandWave` (una onda)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas deterministas: aislar el procesamiento de una onda).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Aislar el procesamiento de una onda (sort por clave estable + ejecutar demandas) en `processBodyTypeDemandWave`. |
| Naturaleza | Extracción pura (sin cambio de conducta). |
| Comportamiento idéntico | Orden y materialización de demandas de cuerpo. |
| Revisión de referencia | `development` en `66b7df90`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `processBodyTypeDemandWave`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `processBodyTypeDemandWave` ordena y ejecuta una onda; `materializeBodyTypeSpecializationDemands`
  queda como el bucle de ondas (swap + clear + onda).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
