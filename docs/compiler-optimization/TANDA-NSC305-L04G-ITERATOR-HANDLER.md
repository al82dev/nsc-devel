# Tanda NSC305-L04g — Familia cleanup/exceptions/async (subfamilia de iteradores)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (expression machine iterativa, familia 6/6: iteradores → `lowerIteratorExpression`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Extraer la subfamilia de iteradores a `lowerIteratorExpression`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `4ba8f7438`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerIteratorExpression`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La subfamilia de iteradores (`IteratorCurrent`/`IteratorMoveNext`/`IteratorGetEnumerator`/
  `IteratorDrop`/`IteratorMoveNextAsync`/`IteratorCancel`) sale del switch de `lowerExpression` a
  `lowerIteratorExpression`. Tercera subfamilia de la familia 6/6.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
