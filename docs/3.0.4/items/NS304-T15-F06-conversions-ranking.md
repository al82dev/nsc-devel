# NS304-T15-F06 — Conversiones y overload ranking

- **Estado**: AUDITADO (classifier de conversiones existe; conversiones decimales sin congelar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar conversiones y overload ranking*.
Plan T15-F06 (DeepSeekDocs): *enteros exactos cuando caben; FP requiere política explícita de rounding
y valores no finitos rechazados; decimal a entero respeta modo; congelar ranking y ambigüedades,
incluidas literals sin sufijo y generics*.

Estado real:

- `ProjectConversionClassifier` rankea conversiones nominales; las conversiones decimal↔entero↔FP
  (exactas/rechazo de no finitos/modo) no están definidas ni rankeadas.

## Gap

Falta las conversiones decimal↔entero (exactas), decimal↔FP (rounding explícito + no finitos
rechazados), decimal→entero (modo), y el ranking/ambigüedades (literals sin sufijo + generics).

## Nota

F06 depende de F01/F04/F05 (representación/aritmética/modos) + el classifier existente. No es una
surface nueva; es el ranking de conversiones del decimal.
