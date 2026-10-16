# Tanda NSC305-L04l — Máquina iterativa (member/index: FieldLoad/BitFieldLoad)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: member/index, self-call del receptor de `fieldAddress` en loads).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar el self-call de `FieldLoad`/`BitFieldLoad` al trampolín; retirar `fieldAddress`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `e9a48d0f0`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado), `fieldAddressFromReceiver` (nuevo); retirado `fieldAddress`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `FieldLoad`/`BitFieldLoad` delegan en `lowerExpressionIterative`; el receptor se evalúa por frame y
  el cómputo de dirección se factoriza en `fieldAddressFromReceiver`. `fieldAddress` (helper con
  self-call) se retira. Cierra el acceso a campos de member/index; queda `lowerArrayElementAddress`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
