# Tanda NSC305-L04x — Máquina iterativa (aggregate: ValueConstruct)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: aggregate, self-calls de argumentos de `ValueConstruct`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar la evaluación de argumentos de `ValueConstruct` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `9f8a0cdd1`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `lowerValueConstruction` acepta `argumentOverrides`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `ValueConstruct` evalúa sus argumentos por frames y entrega `argumentOverrides` a
  `lowerValueConstruction`. Quedan `AllocateAndConstruct`/`AggregateInitialize`/
  `PlacementConstruct` y `AwaitFuture`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
