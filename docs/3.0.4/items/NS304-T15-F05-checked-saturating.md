# NS304-T15-F05 — Checked/wrapping/saturating decimal

- **Estado**: PARCIAL (overflow del coeficiente atrapado; modos checked/saturating explícitos pendientes)
- **Rama**: development

## Resultado (overflow del coeficiente — 2026-10-14)

- **LLVM**: el producto decimal comprueba `coeficiente > 2^96−1` y atrapa `DecimalOverflow`
  (exit 145) en vez de envolver silenciosamente — "nunca por accidente de bits" (F05).
- Verificado: `1e15m * 1e15m` → exit 145. Registrado `TL-LANG-429` (PASS).

## Residual

- Los **modos explícitos** checked/saturating (keywords/métodos con contexto) y el diagnóstico
  compile-time de literales fuera de rango; wrapping queda prohibido sin definición matemática.
