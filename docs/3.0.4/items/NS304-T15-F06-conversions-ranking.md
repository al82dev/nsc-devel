# NS304-T15-F06 — Conversiones y overload ranking

- **Estado**: PARCIAL (decimal→entero truncado operativo; entero→decimal, decimal↔FP y ranking pendientes)
- **Rama**: development

## Resultado (decimal→entero — 2026-10-14)

- **LLVM**: `decimal → int/long/...` = coeficiente (low 96) ÷ 10^escala (truncado hacia cero), con
  `10^escala` construido por un bucle runtime (escala dinámica 0–28) y la división por una vía de 64
  bits (el `udiv` de 128 bits necesitaría `__udivti3`).
- Verificado: `(int)1.5m` → 1 (exit 0). Registrado `TL-LANG-425` (PASS).

## Residual

- **entero→decimal** (el cast `(decimal)i` no tokeniza aún — surface de cast de `decimal` como
  destino), **decimal↔FP** (rounding explícito + no-finitos rechazados) y el **ranking/ambigüedades**
  (literales sin sufijo + generics). El coeficiente >64 bits y la escala >19 quedan para F05.
