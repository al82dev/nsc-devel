# Tanda NSC305-L04as — Máquina iterativa (casos inline: CoreConsoleWriteLine/ReadLine)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de `CoreConsoleWriteLine`/`CoreConsoleReadLine`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de dos intrínsecos Core al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `d628b9a87`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `CoreConsoleWriteLine`/`CoreConsoleReadLine` evalúan su argumento por frame y emiten la llamada
  runtime en el trampolín. Siguen los intrínsecos Core*/String*/Simd* restantes.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
