# Tanda NSC305-L03f — Extraer el mapeo pointer-like del lowerer

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer types: pointer-like carriers → `mapPointerLikeLlvmType`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover el mapeo ref/refmut/string/native-array/reference → `ptr` a `mapPointerLikeLlvmType`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `da4ce6726`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `mapPointerLikeLlvmType`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Los carriers pointer-like (ref/refmut/string/native-array/reference) salen de `llvmType` a
  `mapPointerLikeLlvmType`; el `llvmType` delega en ella. Sexto componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
