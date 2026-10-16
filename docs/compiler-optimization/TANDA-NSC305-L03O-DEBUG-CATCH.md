# Tanda NSC305-L03o — Extraer `emitDebugCatchParameter` (debug del catch)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer debug: emisión DI del catch → `emitCatchParameterDebugInfo`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la emisión de debug del catch parameter a `emitCatchParameterDebugInfo`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `012097e73`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `emitCatchParameterDebugInfo`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `emitDebugCatchParameter` delega en `emitCatchParameterDebugInfo` (DIBuilder + contexto + builder +
  scopes + lookups por callback). Decimoquinto componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
