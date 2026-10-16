# Tanda NSC305-L04s — Máquina iterativa (calls: DirectInstanceCall)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: calls, self-calls de receptor/argumentos de `DirectInstanceCall`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar la evaluación del receptor y los argumentos de `DirectInstanceCall` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `68d24df6d`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `DirectInstanceCall` evalúa el receptor (fase 1) y los argumentos (fase 2, `retainedDepth` como
  índice) mediante frames, y entrega `receiverOverride`/`argumentOverrides` a `lowerCall`. Quedan
  `VirtualInstanceCall`/`InterfaceInstanceCall`/`DelegateInvoke`/`EventInvoke`, AwaitFuture y aggregate.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
