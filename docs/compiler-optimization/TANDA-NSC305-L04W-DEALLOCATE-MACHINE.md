# Tanda NSC305-L04w — Máquina iterativa (aggregate: Deallocate)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: aggregate, self-call de `Deallocate`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar el self-call del operando de `Deallocate` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `cb1b57857`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `lowerDeallocation` acepta `loweredOperandOverride`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `Deallocate` evalúa su operando por frame y entrega `loweredOperandOverride` a `lowerDeallocation`.
  Quedan `ValueConstruct`/`AllocateAndConstruct`/`AggregateInitialize`/`PlacementConstruct` y
  `AwaitFuture`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
