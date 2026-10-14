# NS304-T15-F07 — Consteval/runtime y lowering decimal

- **Estado**: PARCIAL (const decimal materializado como i128 operativo; equivalencia consteval/runtime exhaustiva + intrinsics/helpers estáticos pendientes)
- **Rama**: development

## Resultado (const decimal — 2026-10-14)

- **Wrapper LLVM-C**: declaré `LLVMConstIntOfArbitraryPrecision` en `LlvmCApi.h` (faltaba y es la
  vía para constantes de 96/128 bits).
- **Materialización**: `const decimal K = 1.5m;` baja a un `i128` = coeficiente (low 96) | escala<<96
  (via `staticField.constantFloatingValue` + el helper `decimalCoefficientAndScale`).
- Verificado: `const decimal K=1.5m; (int)K==1` → exit 0. Registrado `TL-LANG-428` (PASS).

## Residual

- La **equivalencia consteval/runtime exhaustiva** (mismo bit pattern o mismo error en todo el corpus)
  + los intrinsics/helpers estáticos con ABI estable; el coeficiente >64 bits queda para F05.
