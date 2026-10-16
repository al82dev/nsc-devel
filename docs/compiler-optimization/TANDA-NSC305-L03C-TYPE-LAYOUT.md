# Tanda NSC305-L03c — Extraer los lookups de layout del lowerer

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer types: lookups de struct/class → `LlvmTypeLayout.h`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover `structLayout`/`classLayout` a funciones libres `findStructLayout`/`findClassLayout`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `bc3bfcd06`. |
| Archivos | `include/nativesharp/LLVM/LlvmTypeLayout.h` (nuevo), `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `findStructLayout`, `findClassLayout`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- Los lookups de struct/class del MIR pasan a `LlvmTypeLayout.h` (funciones libres inline) y el
  lowerer las llama con `mirModel_`. Tercer componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
