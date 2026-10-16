# Tanda NSC305-L04f — Familia cleanup/exceptions/async (subfamilia de cleanup)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (expression machine iterativa, familia 6/6: cleanup → `lowerCleanupExpression`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la subfamilia de cleanup a `lowerCleanupExpression`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `ab1200f85`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerCleanupExpression`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La subfamilia de cleanup (`OwnedTemporaryBorrow`/`FullExpressionCleanup`/`Conditional`/
  `LoopExpression`/`MatchExpression`/`MatchValue`/`OverflowPolicy`/`NullCoalescing`/
  `OptionalCoalescing`/`NullCoalescingStore`/`BorrowValue`) sale del switch de `lowerExpression` a
  `lowerCleanupExpression`. Segunda subfamilia de la familia 6/6.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
