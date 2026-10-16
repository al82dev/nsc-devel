# Tanda NSC305-L04q — Máquina iterativa (async/iterator: MoveNext/MoveNextAsync)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: async/iterator, self-calls de `IteratorMoveNext`/`IteratorMoveNextAsync`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de `IteratorMoveNext`/`IteratorMoveNextAsync` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `cdb0c1492`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `IteratorMoveNext` (con propagación de error) e `IteratorMoveNextAsync` delegan en
  `lowerExpressionIterative`. **Cierra la subfamilia de iteradores.** Quedan `AwaitFuture`, calls y
  aggregate.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
