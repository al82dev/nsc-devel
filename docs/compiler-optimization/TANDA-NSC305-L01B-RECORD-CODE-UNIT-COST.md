# Tanda NSC305-L01b — Registrar el coste real por code unit

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L01 (perfil por code unit: alimentar el peso histórico con el coste medido).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir `recordCodeUnitCost` y llamarlo desde el generador con el coste real observado. |
| Naturaleza | Añadido (telemetría/observabilidad; no cambia la partición ni la salida). |
| Comportamiento idéntico | Particiones y salida iguales; sólo se actualiza el peso histórico. |
| Revisión de referencia | `development` en `784a51e4e`. |
| Archivos | `include/nativesharp/LLVM/ProjectOopCodePartition.h`, `src/LLVM/ProjectOopCodePartition.cpp`, `src/LLVM/ProjectOopObjectGenerator.cpp`. |
| Símbolos | `recordCodeUnitCost`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `recordCodeUnitCost(ordinal, observedCost)` mezcla el coste observado (la suma de las fases
  fingerprint/lookup/lower/optimize/emit/verify/store de la telemetría) en el peso histórico acotado.
  El generador lo invoca por unidad particionada al final de la generación.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
