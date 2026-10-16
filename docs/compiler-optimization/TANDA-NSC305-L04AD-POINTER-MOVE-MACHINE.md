# Tanda NSC305-L04ad — Máquina iterativa (casos inline: PointerLoad/MoveOwnedField/MoveIteratorCurrent)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de `PointerLoad`/`MoveOwnedField`/`MoveIteratorCurrent`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de tres casos inline al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `9f9616f08`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `PointerLoad`, `MoveOwnedField` y `MoveIteratorCurrent` (cada uno 1 self-call + load/store) delegan
  en `lowerExpressionIterative`. Siguen los self-calls restantes de los casos inline/helpers.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
