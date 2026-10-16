# Tanda NSC305-L03x — Extraer `emitDescriptorInitializer` (initializer del descriptor)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L03 (extraer metadata: initializer del class_descriptor → `emitDescriptorInitializer`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Mover la emisión del initializer del descriptor a `emitDescriptorInitializer`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `f640cc68b`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `emitDescriptorInitializer`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- La emisión del initializer del class_descriptor sale del loop de `emitDispatchMetadata` a
  `emitDescriptorInitializer` (tipos + tablas + lookups por callback). Vigésimo cuarto componente.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
