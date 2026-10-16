# Tanda NSC305-L04n — Máquina iterativa (cleanup: BorrowValue/FullExpressionCleanup)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: cleanup, self-calls de `BorrowValue`/`FullExpressionCleanup`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de `BorrowValue`/`FullExpressionCleanup` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `ad51f201f`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado, frame con `retainedDepth`). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `BorrowValue` (wrapper transparente) y `FullExpressionCleanup` (manejo de regiones de cleanup con
  profundidad retenida) delegan en `lowerExpressionIterative`; el frame guarda la profundidad de
  cleanup previa. Quedan `OwnedTemporaryBorrow`/`OverflowPolicy` y las familias calls/aggregate/async.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
