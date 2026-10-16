# Tanda NSC305-L04h — Familia cleanup/exceptions/async (subfamilia await)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (expression machine iterativa, familia 6/6: await → `lowerAwaitExpression`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer el caso `AwaitFuture` a `lowerAwaitExpression`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `97b461201`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerAwaitExpression`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- El caso `AwaitFuture` (sync + suspend/resume) sale del switch de `lowerExpression` a
  `lowerAwaitExpression`. Cierra la familia 6/6 (cleanup/exceptions/async): no queda ningún caso
  inline de la familia en el switch.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
