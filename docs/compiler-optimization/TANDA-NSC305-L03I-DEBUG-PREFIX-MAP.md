# Tanda NSC305-L03i — Extraer el remapeo de rutas de debug del lowerer

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer debug: remapeo de source paths → `LlvmDebugPrefixMap.h`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover `portablePath`/`foldedWindowsPath`/`pathHasPrefix`/`remapDebugSourcePath` a un header. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `2bebc76f4`. |
| Archivos | `include/nativesharp/LLVM/LlvmDebugPrefixMap.h` (nuevo), `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `portablePath`, `foldedWindowsPath`, `pathHasPrefix`, `remapDebugSourcePath`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- El remapeo de rutas de debug (prefix maps) pasa a `LlvmDebugPrefixMap.h` (funciones libres inline).
  Noveno componente de la tanda L03; la emisión debug y el dispatch metadata siguen.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
