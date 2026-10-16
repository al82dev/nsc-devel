# Tanda NSC305-L04o — Máquina iterativa (cleanup: OwnedTemporaryBorrow/OverflowPolicy)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: cleanup, self-calls de `OwnedTemporaryBorrow`/`OverflowPolicy`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de `OwnedTemporaryBorrow`/`OverflowPolicy` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `755ce7491`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado, frame con `overflowPolicy`). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `OwnedTemporaryBorrow` (slot oculto + push de región de cleanup) y `OverflowPolicy` (salva/restaura
  la política por frame) delegan en `lowerExpressionIterative`. **Cierra la subfamilia cleanup.**
  Quedan las familias calls, aggregate y async.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
