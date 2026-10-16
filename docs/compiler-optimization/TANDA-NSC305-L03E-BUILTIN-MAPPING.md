# Tanda NSC305-L03e — Extraer el mapeo de builtins del lowerer

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer types: builtin carriers → `mapBuiltinLlvmType`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover el mapeo de `builtin:*` → LLVM a `mapBuiltinLlvmType` (función libre). |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `b90a02929`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `mapBuiltinLlvmType`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Los carriers de `builtin:*` salen de `llvmType` a una función libre `mapBuiltinLlvmType`; el
  `llvmType` delega en ella. Quinto componente de la tanda L03 (el mapeo completo `llvmType` sigue).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
