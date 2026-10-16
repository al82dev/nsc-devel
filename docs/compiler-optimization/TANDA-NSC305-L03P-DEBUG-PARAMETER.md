# Tanda NSC305-L03p — Extraer `emitDebugParameter` (debug de parámetros)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer debug: emisión DI de un parámetro → `emitParameterDebugInfo`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la emisión de debug de un parámetro ordinario a `emitParameterDebugInfo`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `69cdce4f6`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `emitParameterDebugInfo`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `emitDebugParameter` delega en `emitParameterDebugInfo` (DIBuilder + contexto + builder + member
  model + source manager + lookups por callback). Decimosexto componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
