# Tanda NSC305-L04al — Máquina iterativa (caso inline: NullConditionalArrayStore)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de `NullConditionalArrayStore`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls (receptor + índice + valor) de `NullConditionalArrayStore` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `5be8a5623`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `lowerNullConditionalArrayStore` acepta overrides. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `NullConditionalArrayStore` evalúa receptor, índice y valor por frames y entrega overrides a
  `lowerNullConditionalArrayStore`. **Cierra la familia NullConditional.** Siguen Core*/String*/Simd*
  y los helpers restantes.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
