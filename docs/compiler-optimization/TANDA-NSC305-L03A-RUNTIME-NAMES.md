# Tanda NSC305-L03a — Extraer runtime names del lowerer (movimiento puro)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer runtime: nombres de hooks → `LlvmRuntimeNames.h`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover los nombres de hooks de runtime (`__nativesharp_*` + RaiseException/__C_specific_handler/panic code) a un header. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `0a933478b`. |
| Archivos | `include/nativesharp/LLVM/LlvmRuntimeNames.h` (nuevo), `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `k*FunctionName`, `kNativeSharpPanicExceptionCode`, `kRaiseExceptionFunctionName`, `kSehPersonalityFunctionName`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Los ~98 nombres de hooks de runtime se declaran `inline constexpr` en `LlvmRuntimeNames.h` y se
  eliminan del namespace anónimo del lowerer. Es el primer componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
