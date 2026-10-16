# Tanda NSC305-L03y — Extraer `emitDescriptorAccessorBody` (accessor del descriptor)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer metadata: accessor body del descriptor → `emitDescriptorAccessorBody`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la emisión del accessor body del descriptor a `emitDescriptorAccessorBody`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `edd6c79f4`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `emitDescriptorAccessorBody`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La emisión del accessor body (resolución de base + store atómico + ret) sale del loop de
  `emitDispatchMetadata` a `emitDescriptorAccessorBody`. Cierra la tanda L03 (metadata).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
