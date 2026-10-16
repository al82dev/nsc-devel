# Tanda NSC305-L01a — Peso histórico bounded por code unit

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L01 (perfil por code unit: peso histórico bounded).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `historicalCost` al code unit + `boundedHistoricalWeight` (media móvil 1:1 acotada). |
| Naturaleza | Añadido (el peso histórico no altera la partición actual). |
| Comportamiento idéntico | Particiones y salida iguales; sólo se añade el peso. |
| Revisión de referencia | `development` en `09c7f5396`. |
| Archivos | `include/nativesharp/LLVM/ProjectOopCodePartition.h`, `src/LLVM/ProjectOopCodePartition.cpp`. |
| Símbolos | `ProjectOopCodeUnit::historicalCost`, `boundedHistoricalWeight`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `boundedHistoricalWeight` mezcla el peso histórico previo con el observado (1:1) y satura en el tope;
  una primera observación siembra el peso. El `build` actualiza `historicalCost` por unidad.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
