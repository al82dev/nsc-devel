# Tanda NSC305-L03k — Extraer `debugFile` (creación de DIFile)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer debug: DIFile → `createDebugFile`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la creación/caché de DIFile a `createDebugFile` (función libre). |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `cf9c34b4a`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `createDebugFile`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `debugFile` delega en `createDebugFile` (DIBuilder + source manager + prefix maps + cachés como
  argumentos). Undécimo componente de la tanda L03.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
