# Tanda NSC305-L04p — Máquina iterativa (async/iterator: GetEnumerator/Current/Drop/Cancel)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: async/iterator, self-calls de `IteratorGetEnumerator`/`IteratorCurrent`/`IteratorDrop`/`IteratorCancel`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de los iteradores simples al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `bbdf340ce`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `IteratorGetEnumerator` (wrapper transparente), `IteratorCurrent`, `IteratorDrop` e
  `IteratorCancel` delegan en `lowerExpressionIterative`. Quedan `IteratorMoveNext`/
  `IteratorMoveNextAsync` (async/iterator) y las familias calls/aggregate/await.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
