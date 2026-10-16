# Tanda NSC305-L04b — Familia member/index (subfamilia de direcciones)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (expression machine iterativa, familia 2/6: direcciones → `lowerAddressExpression`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la subfamilia de direcciones (member/index) a `lowerAddressExpression`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `e8e49814f`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerAddressExpression`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La subfamilia de direcciones (`ParameterAddress`/`LocalAddress`/`FieldAddress`/`BitFieldAddress`/
  `StaticFieldAddress`/`NamespaceStateAddress`/`FutureFieldAddress`) sale del switch de
  `lowerExpression` a `lowerAddressExpression`. Parte de la familia 2/6 (member/index).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
