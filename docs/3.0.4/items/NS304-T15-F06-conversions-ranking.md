# NS304-T15-F06 — Conversiones y overload ranking

- **Estado**: IMPLEMENTADO (cast a `decimal` + integral↔decimal + decimal↔FP + ranking)
- **Rama**: development

## Resultado (2026-10-14)

- **Parser**: `decimal` se reconoce como tipo builtin contextual en el lookahead de cast
  (`scanTypeAt`/`canStartType`), de modo que `(decimal)valor` parsea como cast.
- **Conversión implícita**: `sbyte/byte/short/ushort/int/uint/long/ulong/char → decimal` es widening
  numérico exacto (coeficiente `|v|`, escala 0, bit de signo 127); float/double **no** son implícitos.
- **LLVM**: `int → decimal` (sext/zext + abs + signo), `decimal → int` (coeficiente/10^escala truncado
  hacia cero, con máscara de escala 0x3F y signo), `decimal → float/double` (coeficiente/10^escala),
  `float/double → decimal` (redondeo a 6 fraccionarios) y **NaN/∞ rechazados** con trap `DecimalOverflow`
  (exit 145).
- **Ranking**: `int → long` vence a `int → decimal` en overload (conversión integral más específica).
- Verificado: `TL-LANG-479` (casts + overload, exit 0), `TL-LANG-480` (NaN → exit 145). Guards
  425/426/428/478 PASS.

## Residual

- Coeficiente >64 bits y escala >19 en las vías de 64 bits (queda para T15-F05 con `checked`/overflow).
