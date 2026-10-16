# Tanda NSC305-L03j — Extraer los flags de debug del módulo

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer debug: flags del módulo → `configureDebugModuleFlags`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover los `LLVMAddModuleFlag` de debug a `configureDebugModuleFlags` (función libre). |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `0aa50be5a`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `configureDebugModuleFlags`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Los flags de debug (Debug Info Version / CodeView / NativeSharp Debug Metadata) salen de
  `initializeDebugInformation` a `configureDebugModuleFlags`. Décimo componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
