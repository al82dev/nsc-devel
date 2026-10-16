# Tanda NSC305-L04az — Máquina iterativa (casos inline: CoreDirectoryGetFiles/GetDirectories + CoreThreadCreate)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de `CoreDirectoryGetFiles`/`GetDirectories`/`CoreThreadCreate`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de tres intrínsecos al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `822c9c8e4`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `CoreDirectoryGetFiles`/`CoreDirectoryGetDirectories` (2 operandos) y `CoreThreadCreate` (1
  operando) evalúan sus argumentos por frames y emiten la llamada runtime en el trampolín.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
