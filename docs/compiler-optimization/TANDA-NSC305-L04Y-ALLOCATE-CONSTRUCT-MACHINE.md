# Tanda NSC305-L04y — Máquina iterativa (aggregate: AllocateAndConstruct)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: aggregate, self-calls de argumentos de `AllocateAndConstruct`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar la evaluación de argumentos de `AllocateAndConstruct` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `1caf5300c`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `lowerConstruction` acepta `argumentOverrides`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `AllocateAndConstruct` evalúa sus argumentos por frames y entrega `argumentOverrides` a
  `lowerConstruction`. Quedan `AggregateInitialize`/`PlacementConstruct` y `AwaitFuture`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
