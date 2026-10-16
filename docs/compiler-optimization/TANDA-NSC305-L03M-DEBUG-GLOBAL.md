# Tanda NSC305-L03m — Extraer `emitDebugGlobal` (debug de globales)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer debug: emisión de DI global → `emitGlobalDebugInfo`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la emisión de debug de un static field a `emitGlobalDebugInfo`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `80ed03481`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `emitGlobalDebugInfo`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `emitDebugGlobal` delega en `emitGlobalDebugInfo` (DIBuilder + compile unit + contexto + source
  manager + callbacks de debugFile/debugNamedValueType). Decimotercer componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
