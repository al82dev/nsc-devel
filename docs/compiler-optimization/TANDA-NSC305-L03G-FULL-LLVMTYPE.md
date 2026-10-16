# Tanda NSC305-L03g — Extraer el mapeo `llvmType` completo (simd/array/named)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer types: `mapLlvmType`/`mapNonBuiltinLlvmType` → función libre).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover simd/fixed-array/named → `mapNonBuiltinLlvmType` + `mapLlvmType` (recursión mutua). |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `96e980588`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `mapLlvmType`, `mapNonBuiltinLlvmType`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `llvmType` queda como delegado de `mapLlvmType`; el resto (simd/fixed-array/named) vive en
  `mapNonBuiltinLlvmType` y la recursión pasa por `mapLlvmType`. Cierra el componente "types" de L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
