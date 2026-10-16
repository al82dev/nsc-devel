# Tanda NSC305-L04j — Máquina iterativa (member/index: FutureFieldAddress)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: member/index, self-call de `FutureFieldAddress`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar el self-call de `FutureFieldAddress` al trampolín iterativo. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `562816ca6`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `FutureFieldAddress` evalúa su receptor mediante un push de frame + `ValueStack` en
  `lowerExpressionIterative`; `lowerAddressExpression` delega. Primer self-call de member/index
  migrado. Quedan los self-calls de `fieldAddress` y `lowerArrayElementAddress`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
