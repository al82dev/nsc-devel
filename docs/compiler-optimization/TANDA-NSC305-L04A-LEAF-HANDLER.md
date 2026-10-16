# Tanda NSC305-L04a — Familia literals/names (handler de hojas)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (expression machine iterativa, familia 1/6: literals/names → `lowerLeafExpression`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la familia de hojas (literals/names) a `lowerLeafExpression` (dispatcher del machine). |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `b1aeaca98`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerLeafExpression`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La familia de hojas (`ThisValue`/`ParameterValue`/`LocalValue`/`IntegerConstant`/
  `FloatingConstant`/`BooleanConstant`/`NullConstant`/`DefaultValue`/`StringLiteral`/
  `Utf8StringLiteral`/`EnumName`) sale del switch de `lowerExpression` a `lowerLeafExpression`
  (handler del dispatcher). Familia 1/6 de la máquina iterativa; sin recursión de subexpresiones.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
