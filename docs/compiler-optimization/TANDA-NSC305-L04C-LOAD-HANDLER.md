# Tanda NSC305-L04c — Familia member/index (subfamilia de loads)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (expression machine iterativa, familia 2/6: loads → `lowerLoadExpression`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la subfamilia de loads (member/index) a `lowerLoadExpression`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `faf9127ea`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerLoadExpression`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La subfamilia de loads (`FieldLoad`/`BitFieldLoad`/`StaticFieldLoad`/`NamespaceStateLoad`) sale del
  switch de `lowerExpression` a `lowerLoadExpression`. Cierra la familia 2/6 (member/index).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
