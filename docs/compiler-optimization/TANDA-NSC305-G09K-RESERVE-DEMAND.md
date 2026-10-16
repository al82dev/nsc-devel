# Tanda NSC305-G09k — `reserveTypeDemand` (pre-reserva determinista)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: G09 (ondas paralelas: pre-reserva determinista antes del execute paralelo).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `reserveTypeDemand` (clave + nombre + dedup + budget + reserva del ordinal). |
| Naturaleza | Infraestructura (sin cambio de conducta; aún no se consume en la onda). |
| Comportamiento idéntico | Sin cambio (función nueva). |
| Revisión de referencia | `development` en `86fd3124d`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `reserveTypeDemand`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `reserveTypeDemand` hace la reserva determinista (clave + nombre + dedup + budget + ordinal) que la
  onda paralela ejecutará ANTES del execute, de modo que los ordinales no dependan del orden de los
  workers.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
