# Tanda NSC305-L04v — Máquina iterativa (aggregate: ObjectDestroy)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: aggregate, self-call de `ObjectDestroy`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar el self-call del operando de `ObjectDestroy` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `4a343e862`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `ObjectDestroy` evalúa su operando por frame y emite la destrucción dinámica en el trampolín.
  Primer caso de la familia aggregate; quedan `Deallocate`/`ValueConstruct`/`AllocateAndConstruct`/
  `AggregateInitialize`/`PlacementConstruct` y `AwaitFuture`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
