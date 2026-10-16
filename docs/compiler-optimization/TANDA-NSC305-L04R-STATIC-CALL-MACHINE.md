# Tanda NSC305-L04r — Máquina iterativa (calls: DirectStaticCall)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: calls, self-calls de argumentos de `DirectStaticCall`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar la evaluación de argumentos de `DirectStaticCall` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `85d6a0f37`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado, argumentos por frames). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `DirectStaticCall` evalúa sus argumentos mediante frames (el `phase` lleva el índice de argumento)
  y entrega un `argumentOverrides` a `lowerStaticCall`. Primer caso de la familia calls; quedan
  `DirectInstanceCall`/`VirtualInstanceCall`/`InterfaceInstanceCall`, AwaitFuture y aggregate.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
