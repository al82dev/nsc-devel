# Tanda NSC305-L04z — Máquina iterativa (aggregate: PlacementConstruct)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: aggregate, self-calls de `PlacementConstruct`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar la evaluación del destino y los argumentos de `PlacementConstruct` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `ca0a00cc3`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `lowerPlacementConstruction` acepta overrides. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `PlacementConstruct` evalúa el destino y los argumentos por frames y entrega
  `storageOverride`/`argumentOverrides` a `lowerPlacementConstruction`. Queda `AggregateInitialize`
  (aggregate) y `AwaitFuture`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
