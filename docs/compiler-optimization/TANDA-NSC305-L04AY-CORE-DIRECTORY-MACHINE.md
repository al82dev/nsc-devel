# Tanda NSC305-L04ay — Máquina iterativa (casos inline: CoreDirectory/Path + GetCommandLineArgs)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de `CoreDirectoryDelete`/`CorePathGetFullPath`/
> `CoreDirectoryGetFileCount`/`CoreDirectoryGetDirectoryCount`/`CoreEnvironmentGetCommandLineArgs`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de cinco intrínsecos de directorio/ruta/args al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `51c5b2b21`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `CoreDirectoryDelete`/`CorePathGetFullPath`/`CoreDirectoryGetFileCount`/`CoreDirectoryGetDirectoryCount`
  y `CoreEnvironmentGetCommandLineArgs` evalúan su operando por frame y emiten la llamada runtime
  en el trampolín. `CoreEnvironmentGetCommandLineArgCount` (cero operandos) se conserva intacto.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
