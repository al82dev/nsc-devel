# Tanda NSC305-L04ai — Máquina iterativa (caso inline: NullConditionalFieldLoad)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-call del receptor de `NullConditionalFieldLoad`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar el self-call del receptor de `NullConditionalFieldLoad` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `cbd649c10`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `lowerNullConditionalField` acepta `receiverOverride`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `NullConditionalFieldLoad` evalúa el receptor por frame y entrega `receiverOverride` a
  `lowerNullConditionalField`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
