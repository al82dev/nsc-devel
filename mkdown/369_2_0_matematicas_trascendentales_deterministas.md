# 369 — Matemáticas trascendentales deterministas / Deterministic transcendental math

## Español

`NativeSharp.Core.DeterministicMath` ofrece `Sin`, `Cos`, `Exp` y `Log` para `double`. Las
implementaciones usan reducción de rango y polinomios de coeficientes fijos escritos enteramente
en NativeSharp. No llaman al `libm` del host; al compilar con `--fp-mode=deterministic`, cada paso
aritmético queda sujeto al redondeo `nearest-even` mediante IR restringido.

El contrato prioriza reproducibilidad bit a bit. `Exp` satura fuera del rango finito de `double`,
`Log(0)` produce infinito negativo y `Log` de un valor negativo produce NaN.

## English

`NativeSharp.Core.DeterministicMath` provides `Sin`, `Cos`, `Exp`, and `Log` for `double`. The
implementations use fixed-coefficient range reduction and polynomial evaluation written entirely
in NativeSharp. They do not call host `libm`; under `--fp-mode=deterministic`, each arithmetic step
uses constrained IR with `nearest-even` rounding.

The contract prioritizes bit-for-bit reproducibility. `Exp` saturates outside finite `double`
range, `Log(0)` yields negative infinity, and a negative `Log` input yields NaN.
