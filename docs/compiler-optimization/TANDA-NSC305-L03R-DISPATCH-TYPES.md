# Tanda NSC305-L03r — Extraer `createDispatchTypes` (tipos del dispatch metadata)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer metadata: tipos del dispatch → `createDispatchTypes`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la creación de los tipos de dispatch (interface_map_entry/interface_map/class_descriptor/accessor) a `createDispatchTypes`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `0d8d902f9`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `createDispatchTypes`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `emitDispatchMetadata` delega la creación de los tipos carrier en `createDispatchTypes` (contexto +
  tipos por referencia). Decimoctavo componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
