# NS3-T14-F03 — Conversiones numéricas / Numeric conversions

## ES

La matriz distingue identidad, widening sin pérdida, constante representable, narrowing, cambio de signo, frontera float/integer y enum/underlying. Sólo identidad, widening demostrado y constantes demostrablemente representables son implícitos. El ranking de overload prioriza identidad y después conversiones lossless; no existe promoción dependiente del orden de candidatos.

Narrowing entero, cambio de signo, float→integer y enum/underlying requieren cast explícito; los casos que pueden salir del rango entero consumen la política `checked/wrapping/saturating/unchecked`. Integer→float sólo es implícito cuando todos los valores del tipo fuente caben exactamente en la mantisa. Los splats SIMD reutilizan exactamente la matriz escalar.

Prueba dirigida: `nsc_ns3_t14_numeric_conversion_tests` cubre pares fundamentales, ranking, constantes representables, enum underlying y SIMD splats.

## EN

The matrix distinguishes identity, lossless widening, representable constants, narrowing, sign changes, the float/integer boundary, and enum/underlying conversions. Only identity, proven widening, and provably representable constants are implicit. Overload ranking prefers identity and then lossless conversions; candidate order cannot create a promotion.

Integer narrowing, sign changes, float-to-integer, and enum/underlying conversions require explicit casts; cases that may leave the integer range consume the `checked/wrapping/saturating/unchecked` policy. Integer-to-float is implicit only when every source value fits the target mantissa exactly. SIMD splats reuse the scalar matrix exactly.

Focused test: `nsc_ns3_t14_numeric_conversion_tests` covers fundamental pairs, ranking, representable constants, enum underlying conversion, and SIMD splats.
