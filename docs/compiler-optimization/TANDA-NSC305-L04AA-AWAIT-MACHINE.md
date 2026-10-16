# Tanda NSC305-L04aa — Máquina iterativa (async: AwaitFuture)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: async, self-call del hijo de `AwaitFuture`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar el self-call del hijo de `AwaitFuture` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `1bf685426`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `lowerAwaitExpression` acepta `childOverride`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `AwaitFuture` evalúa su hijo por frame y entrega `childOverride` a `lowerAwaitExpression`.
  Queda `AggregateInitialize` (aggregate) y los self-calls restantes de los casos inline/helpers.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
