# Tanda NSC305-L04 — Eliminación de recursión COMPLETA (cierre)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Tanda: L04 (eliminación de recursión: los 191 self-calls de `lowerExpression` desaparecen).

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Eliminar TODOS los self-calls recursivos del switch de `lowerExpression`. |
| Naturaleza | Movimiento puro (sin cambio de conducta). |
| Comportamiento idéntico | Texto LLVM normalizado idéntico. |
| Revisión de referencia | `development` en `c65293058`. |
| Archivos | `src/LLVM/ProjectOopLlvmLowerer.cpp`. |
| Símbolos | `lowerExpressionIterative` (ampliado); helpers `*FromParts`/overrides añadidos. |
| Pruebas | `Host.Frontend` completo (544 casos). |
| Cierre | 544/544; expediente + commit local aislado (único). |

## 2. Cambio

Cierre de la eliminación de recursión de L04. Quedaban self-calls en los intrínsecos y edge-cases;
todos se migraron al trampolín `lowerExpressionIterative`:

- Atomic/Interlocked: `CoreAtomicLoadInt32/Int64/Pointer`, `CoreAtomicStoreInt32/Int64/Pointer`,
  `CoreInterlockedIncrement/DecrementInt32/Int64`, `CoreInterlockedAdd/ExchangeInt32/Int64`,
  `CoreInterlockedCompareExchangeInt32/Int64/Pointer`, `CoreInterlockedExchangePointer`,
  `CoreMonitorWait`.
- SIMD: `CoreSimdFloat32x4Multiply`, `CoreSimdFloat256 Add/Subtract/Multiply`,
  `CoreSimdInt128Multiply`, `CoreSimdInt256 Add/Subtract/Multiply`, `CoreSimdLoad/Store Aligned/Unaligned128`,
  `CoreSimdSqrt/ReciprocalSqrt/FusedMultiplyAdd`, `CoreSimdSwizzleWzyx/ShuffleLowHigh`.
- Bit/Half/Cpu: `CoreBitRotateLeft/Right/ByteSwap`, `CoreBitPopulationCount/LeadingZeroCount/TrailingZeroCount`,
  `CoreHalfFromFloat/ToFloat`, `CoreCpuPrefetchRead`.
- Span/StringView/Temporary: `SpanFromArray/ReadOnlySpanFromArray`, `SpanFromFixedArray/ReadOnlySpanFromFixedArray`,
  `StringViewFromString`, `TemporaryAddress`.

El switch de `lowerExpression` queda con **0 self-calls** (los 191 originales, eliminados).

## 3. Evidencia

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL** (texto LLVM normalizado idéntico).
