# Tanda NSC305-L03h — Extraer metadata del módulo (target triple + intrinsics)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer metadata: target triple + intrinsics → `LlvmModuleMetadata.h`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover `kTargetTriple` + los nombres de intrinsics a `LlvmModuleMetadata.h`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `833a17743`. |
| Archivos | `include/nativesharp/LLVM/LlvmModuleMetadata.h` (nuevo), `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `kTargetTriple`, `kMemsetIntrinsicName`, `kMemcpyIntrinsicName`, `kAssumeIntrinsicName`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- El target triple y los nombres de intrinsics pasan a `LlvmModuleMetadata.h` (inline constexpr). Octavo
  componente de la tanda L03; el `emitDispatchMetadata` y el debug siguen.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
