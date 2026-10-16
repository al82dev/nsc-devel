# Tanda NSC305-L04d — Familia member/index (array/string access)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (expression machine iterativa, familia 3/6: array/string loads → `lowerLoadExpression`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover los loads de array/string (`ArrayElementLoad`/`RangeArrayElementLoad`/`StringElementLoad`) a `lowerLoadExpression`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `9a6093858`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerLoadExpression` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Los loads de índice (`ArrayElementLoad`/`RangeArrayElementLoad`/`StringElementLoad`) salen del
  switch de `lowerExpression` y se incorporan a `lowerLoadExpression`. Cierra la familia 3/6
  (member/index).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
