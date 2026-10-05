# NS3-T14-F02 — Política de overflow / Overflow policy

## ES

Las operaciones enteras `add`, `sub`, `mul`, `div`, `shift left` y `shift right` consumen una política explícita: `checked` diagnostica, `wrapping` reduce módulo 2^N, `saturating` limita al mínimo/máximo y `unchecked` marca overflow como comportamiento indefinido para el optimizador. División por cero siempre es error; el shift fuera del ancho se diagnostica en contexto checked.

La misma evaluación se usa en runtime, constantes, lanes SIMD e intrinsics. La optimización y la configuración Debug/Release no cambian resultados ni convierten accidentalmente una política en otra. Los resultados conservan bits, indicador de overflow y marcador de UB por separado.

Prueba dirigida: `nsc_ns3_t14_overflow_policy_tests` cubre add/sub/mul/div/shift, signed/unsigned, checked/wrapping/saturating/unchecked y paridad runtime/comptime/SIMD/intrinsic.

## EN

Integer `add`, `sub`, `mul`, `div`, `shift left`, and `shift right` operations consume an explicit policy: `checked` diagnoses, `wrapping` reduces modulo 2^N, `saturating` clamps to the minimum/maximum, and `unchecked` marks overflow as undefined behavior for optimization. Division by zero is always an error; an out-of-width shift is diagnosed in checked context.

The same evaluator serves runtime, constants, SIMD lanes, and intrinsics. Optimization and Debug/Release configuration cannot change results or accidentally select a different policy. Result bits, overflow status, and UB status remain separate.

Focused test: `nsc_ns3_t14_overflow_policy_tests` covers add/sub/mul/div/shift, signed/unsigned, checked/wrapping/saturating/unchecked, and runtime/comptime/SIMD/intrinsic parity.
