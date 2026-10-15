# NS304-T15-F02 — Literales y parsing exacto

- **Estado**: COMPLETO — round-trip exacto de literal decimal (dígitos/escala verbatim, sin binary FP) + validación de precisión/exponente.
- **Rama**: development

## Implementado (2026-10-15)

- `36ff4952` — cierre del residual:
  - **Round-trip exacto**: `parseDecimalLiteral(spelling, error)` (`NumericSemantics`) recupera los
    dígitos y la escala **verbatim** del texto del literal (no pasa por `double`/`strtod`/`%.17g`):
    `0.1` → coeficiente 1 / escala 1, `1e2` → coeficiente 100 / escala 0, `-0.001` → signo + coef 1 /
    escala 3. Acepta el sufijo `m`/`M` y separadores `_`.
  - **Validación de precisión/exponente/overflow**: escala > 28 → `decimal scale exceeds 28`;
    coeficiente > 96 bits → `decimal coefficient exceeds 96 bits`; exponente fuera de rango y
    caracteres/trailing inválidos reportan errores causales.

## Verificado O0

- `TL-LANG-503` (CL-LANG-016, Host.Language, `nsc_component_tests`): PASS — `0.1`/`1.5m`/`1e2`/`-0.001`
  exactos; escala >28 y coeficiente >96 bits rechazados con error causal.
- Unit `nsc_ns3_t14_numeric_tests`: PASS (16 checks).

## Contexto

- El literal `m` + type `builtin:decimal` + constante 128-bit ya operaban (2026-10-14) vía `%.17g`
  (coeficiente ≤ 64 bits). Aquí se cerró el parse exacto del texto. El cableado del lexer/AST/MIR a
  `parseDecimalLiteral` (para que `1.5m` en fuente use los dígitos verbatim en vez del `double`) y la
  ampliación a coeficiente > 64 bits corresponden a F05/F07 y no se sustituyeron por ABI inventado.
