# Tanda NSC305-L04m — Máquina iterativa (member/index: array/string access)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: member/index, self-calls de `lowerArrayElementAddress`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de array/string (`ArrayElementAddress`/`ArrayElementLoad`/`StringElementLoad`/`StringCharacterPointer`) al trampolín; retirar `lowerArrayElementAddress`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `ae852fd82`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado), `arrayElementAddressFromParts` (nuevo); retirado `lowerArrayElementAddress`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- El cómputo de la dirección de elemento se factoriza en `arrayElementAddressFromParts`; los cuatro
  casos de índice evalúan receptor e índice por frames en `lowerExpressionIterative`.
  `lowerArrayElementAddress` (helper con self-calls) se retira. **Cierra member/index.**

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
