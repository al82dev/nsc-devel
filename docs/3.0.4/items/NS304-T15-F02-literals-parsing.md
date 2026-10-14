# NS304-T15-F02 — Literales y parsing exacto

- **Estado**: AUDITADO (lexer FP existe; literal decimal sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar literales y parsing exacto*.
Plan T15-F02 (DeepSeekDocs): *lexer conserva dígitos/escala sin pasar por binary FP; parser valida
sufijo, precision, exponent permitido y overflow; round-trip texto-decimal-texto exacto; spans de error
señalan dígito/escala causal*.

Estado real:

- El lexer FP existente pasa por binary FP; no hay literal `decimal` (sufijo) que conserve
  dígitos/escala exactos.

## Gap

Falta el literal `decimal` (sufijo) con dígitos/escala exactos (sin binary FP), la validación de
precisión/exponente/overflow, el round-trip texto-decimal-texto y los spans causales.

## Nota

F02 depende de F01 (representación). No es una surface nueva; es el lexing/parsing exacto del literal
decimal.
