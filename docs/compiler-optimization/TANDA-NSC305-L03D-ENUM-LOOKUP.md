# Tanda NSC305-L03d — Extraer los lookups de enum del lowerer

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer types: enum underlying-type → `LlvmTypeLayout.h`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover `enumUnderlyingInfo`/`isEnumTypeKey` a `findEnumUnderlyingInfo`/`isEnumTypeKey` libres. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `2d2fa5189`. |
| Archivos | `include/nativesharp/LLVM/LlvmTypeLayout.h`, `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `findEnumUnderlyingInfo`, `isEnumTypeKey`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Los lookups de enum (tipo subyacente) pasan a `LlvmTypeLayout.h`; el lowerer los llama con
  `nameLookup_`. Cuarto componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
