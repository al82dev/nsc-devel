# Tanda NSC305-L04af — Máquina iterativa (caso inline: PointerArithmetic)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de `PointerArithmetic`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls (dos operandos) de `PointerArithmetic` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `22f9e5143`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `pointerArithmeticFromParts`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `PointerArithmetic` evalúa sus dos operandos por frames y entrega a `pointerArithmeticFromParts`
  (ptrdiff o GEP de bytes, según el tipo). `normalizeNativeIndexFromValue` reutilizado.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
