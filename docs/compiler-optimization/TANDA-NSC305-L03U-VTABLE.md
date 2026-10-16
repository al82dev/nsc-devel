# Tanda NSC305-L03u — Extraer `emitVtableGlobal` (vtable del dispatch)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer metadata: emisión del vtable → `emitVtableGlobal`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la emisión del vtable a `emitVtableGlobal` (módulo + ptr + tabla + lookup por callback). |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `015f9f17a`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `emitVtableGlobal`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La emisión del vtable (global + entradas + initializer + ODR) sale del loop de
  `emitDispatchMetadata` a `emitVtableGlobal`. Vigésimo primer componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
