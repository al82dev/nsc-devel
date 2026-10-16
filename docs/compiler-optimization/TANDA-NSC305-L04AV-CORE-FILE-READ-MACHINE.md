# Tanda NSC305-L04av — Máquina iterativa (casos inline: CoreFile ReadAllText/GetSize/ReadAllBytes)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de `CoreFileReadAllText`/`GetSize`/`ReadAllBytes`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de tres intrínsecos de lectura de archivo al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `543f2bfb4`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `CoreFileReadAllText`/`CoreFileGetSize`/`CoreFileReadAllBytes` evalúan su path por frame y emiten
  la llamada runtime en el trampolín.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
