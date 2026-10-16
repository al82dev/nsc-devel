# Tanda NSC305-L04ae — Máquina iterativa (caso inline: RawElementAddress)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de `RawElementAddress`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls (base + índice) de `RawElementAddress` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `74ae6c766`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `rawElementAddressFromParts`; `normalizeNativeIndexFromValue`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `RawElementAddress` evalúa base e índice por frames y entrega a `rawElementAddressFromParts`
  (flexible-array bounds + GEP). `normalizeNativeIndexFromValue` separa la conversión del self-call.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
