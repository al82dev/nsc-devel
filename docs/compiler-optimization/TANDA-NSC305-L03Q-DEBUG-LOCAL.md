# Tanda NSC305-L03q — Extraer `emitDebugLocal` (debug de locales)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer debug: emisión DI de un local → `emitLocalDebugInfo`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la emisión de debug de un local a `emitLocalDebugInfo`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `47f009fdf`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `emitLocalDebugInfo`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `emitDebugLocal` delega en `emitLocalDebugInfo` (DIBuilder + contexto + builder + scopes + lookups
  por callback). Decimoséptimo componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
