# Tanda NSC305-L04i — Máquina iterativa (familia unary/binary)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: unary/binary → `lowerExpressionIterative`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Eliminar la recursión de unary/binary con un trampolín de frames + ValueStack. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `148a539aa`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative`; retirados `lowerUnary`/`lowerBinary`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Los casos `Unary`/`Binary` delegan en `lowerExpressionIterative`, un trampolín con pila explícita
  de frames y `ValueStack`; los self-calls de los operandos se convierten en pushes. `lowerUnary` y
  `lowerBinary` se retiran (código muerto). Primera familia con recursión eliminada (el núcleo del
  GATE). Las demás familias siguen en el `lowerExpression` recursivo y se migran en tandas sucesivas.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
