# Tanda NSC305-L04ab — Máquina iterativa (aggregate: AggregateInitialize)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: aggregate, self-calls de campos de `AggregateInitialize`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar la evaluación de los campos de `AggregateInitialize` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `8ecdfcdf8`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `initializeAggregateIntoSlot`/`lowerAggregateInitializer` aceptan `fieldValues`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `AggregateInitialize` evalúa el valor anidado de cada campo por frame y entrega `fieldValues` a
  `lowerAggregateInitializer`/`initializeAggregateIntoSlot`. **Cierra la familia aggregate.** Quedan
  los self-calls de los casos inline/helpers restantes.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
