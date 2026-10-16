# Tanda NSC305-L04bi — Máquina iterativa (casos inline: CoreProcessSetPriorityClass/CoreDebugOutputString/CoreThreadSleep)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de `CoreProcessSetPriorityClass`/`CoreDebugOutputString`/`CoreThreadSleep`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de tres intrínsecos de utilidad al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `f94a347e6`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `CoreProcessSetPriorityClass`/`CoreDebugOutputString`/`CoreThreadSleep` evalúan su operando por
  frame y emiten la llamada runtime en el trampolín.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
