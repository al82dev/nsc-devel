# Tanda NSC305-L04bj — Máquina iterativa (casos inline: CoreFileCopy/CoreFileMove/CoreDirectoryMove)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de `CoreFileCopy`/`CoreFileMove`/`CoreDirectoryMove`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de `CoreFileCopy`/`CoreFileMove`/`CoreDirectoryMove` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `8d36ed623`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `CoreFileCopy` (3 operandos) y `CoreFileMove`/`CoreDirectoryMove` (2 operandos) evalúan sus
  argumentos por frames y emiten la llamada runtime en el trampolín.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
