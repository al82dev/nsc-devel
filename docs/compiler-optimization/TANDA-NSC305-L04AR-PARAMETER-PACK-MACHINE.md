# Tanda NSC305-L04ar — Máquina iterativa (caso inline: ParameterPack)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: self-calls de argumentos de `ParameterPack`).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Migrar los self-calls de argumentos de `ParameterPack` al trampolín. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `dd63f0a58`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); `lowerParameterPack` acepta `argumentOverrides`. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado. |

## 2. Cambio

- `ParameterPack` evalúa sus argumentos por frames y entrega `argumentOverrides` a
  `lowerParameterPack`.

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL**.
