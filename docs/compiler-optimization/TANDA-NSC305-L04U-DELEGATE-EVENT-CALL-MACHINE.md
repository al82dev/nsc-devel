# Tanda NSC305-L04u — Máquina iterativa (calls: DelegateInvoke/EventInvoke)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: calls, self-calls de `DelegateInvoke`/`EventInvoke`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar la evaluación del head/delegate y los argumentos de `DelegateInvoke`/`EventInvoke` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `434b1ed23`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `lowerDelegateInvoke`/`lowerEventInvoke` aceptan overrides. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `DelegateInvoke`/`EventInvoke` evalúan el head/delegate y los argumentos por frames y entregan
  overrides a `lowerDelegateInvoke`/`lowerEventInvoke`. **Cierra la familia calls.** Quedan
  `AwaitFuture` y aggregate.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
