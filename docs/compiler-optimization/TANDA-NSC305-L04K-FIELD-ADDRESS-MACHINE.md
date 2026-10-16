# Tanda NSC305-L04k — Máquina iterativa (member/index: FieldAddress/BitFieldAddress)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: member/index, self-call de `fieldAddress` para las direcciones).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar el self-call del receptor de `FieldAddress`/`BitFieldAddress` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `277e9d9b4`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado). |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `FieldAddress`/`BitFieldAddress` evalúan su receptor mediante push de frame + `ValueStack` en
  `lowerExpressionIterative`; `lowerAddressExpression` delega. El `fieldAddress` (helper con
  self-call) sigue sirviendo a `FieldLoad`/`BitFieldLoad`, que se migran en la tanda siguiente.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
