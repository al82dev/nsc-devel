# Tanda NSC305-L03b — Extraer la clasificación de tipos del lowerer

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer types: clasificación pointer/class/delegate → `LlvmTypeClassification.h`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover `isPointerLike`/`isClassReference`/`isDelegateReference` a funciones libres en un header. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `118a47ff9`. |
| Archivos | `include/nativesharp/LLVM/LlvmTypeClassification.h` (nuevo), `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `isPointerLikeType`, `isClassReferenceType`, `isDelegateReferenceType`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Las tres funciones de clasificación pasan a `LlvmTypeClassification.h` (funciones libres inline); el
  lowerer las llama con `nameLookup_` donde hace falta. Segundo componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
