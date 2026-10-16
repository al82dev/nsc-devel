# Tanda NSC305-L04t — Máquina iterativa (calls: VirtualInstanceCall/InterfaceInstanceCall)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: calls, self-calls de receptor/argumentos de llamadas virtuales/interface).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar la evaluación de receptor/argumentos de las llamadas virtual/interface al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `d2aa252f4`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `lowerVirtualCall`/`lowerInterfaceCall` aceptan `argumentOverrides`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `VirtualInstanceCall`/`InterfaceInstanceCall` comparten con `DirectInstanceCall` la evaluación por
  frames del receptor y los argumentos; `lowerVirtualCall`/`lowerInterfaceCall` aceptan
  `argumentOverrides`. Quedan `DelegateInvoke`/`EventInvoke`, AwaitFuture y aggregate.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
