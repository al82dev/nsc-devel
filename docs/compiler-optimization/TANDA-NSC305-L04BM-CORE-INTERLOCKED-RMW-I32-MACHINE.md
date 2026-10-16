# Tanda NSC305-L04bm — Máquina iterativa (casos inline: CoreInterlockedAdd/ExchangeInt32)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de `CoreInterlockedAddInt32`/`ExchangeInt32`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de `CoreInterlockedAddInt32`/`ExchangeInt32` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `40cb77b72`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `CoreInterlockedAddInt32`/`ExchangeInt32` (2 operandos) evalúan location y value por frames y
  emiten el RMW ordenado o la llamada runtime (según `integerValue`) en el trampolín.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
