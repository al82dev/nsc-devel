# Tanda NSC305-L03v — Extraer `emitInterfaceMapGlobals` (interface maps del dispatch)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer metadata: emisión de interface maps → `emitInterfaceMapGlobals`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la emisión de los interface maps a `emitInterfaceMapGlobals`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `31568148d`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `emitInterfaceMapGlobals`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La emisión de las method tables + entradas + headers de interface map sale del loop de
  `emitDispatchMetadata` a `emitInterfaceMapGlobals`. Vigésimo segundo componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
