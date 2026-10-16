# Tanda NSC305-L04e — Familia cleanup/exceptions/async (subfamilia de excepciones)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (expression machine iterativa, familia 6/6: excepciones → `lowerExceptionExpression`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la subfamilia de excepciones (Throw/Unreachable/Panic) a `lowerExceptionExpression`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `6b0cfa778`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExceptionExpression`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La subfamilia de excepciones (`Throw`/`Unreachable`/`Panic`) sale del switch de `lowerExpression` a
  `lowerExceptionExpression`. Primera subfamilia de la familia 6/6 (cleanup/exceptions/async).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
