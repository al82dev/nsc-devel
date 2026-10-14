# NS304-T15-F02 — Literales y parsing exacto

- **Estado**: PARCIAL (literal `m` reconocido + tipeado como `decimal` + bajado a constante 128-bit; round-trip exacto y validación de overflow/precisión pendientes)
- **Rama**: development

## Resultado (literal + type + lowering — 2026-10-14)

- **Lexer**: `m`/`M` cierra el literal numérico como floating sin pasar por `strtod` (el sufijo queda en el lexema).
- **Sema**: el sufijo `m` tipea el literal `builtin:decimal` (no `double`).
- **LLVM**: `FloatingConstant` con `builtin:decimal` baja a `i128` = coeficiente (96 bits) | escala << 96 |
  signo << 127, derivando coeficiente/escala del round-trip `%.17g` (coeficiente ≤ 64 bits).
- Verificado: `decimal d = 1.5m; return 0;` → exit 0. Registrado `TL-LANG-421` (PASS).

## Residual

- **Round-trip texto-decimal-texto exacto** (dígitos/escala verbatim, sin binary FP) + la validación de
  precisión/exponente/overflow + los spans de error causales. El coeficiente >64 bits devuelve el
  diagnóstico "coefficient exceeds 64 bits" (se amplía en F05).
