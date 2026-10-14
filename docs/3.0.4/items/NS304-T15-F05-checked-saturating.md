# NS304-T15-F05 — Checked/wrapping/saturating decimal

- **Estado**: AUDITADO (modos enteros existen; modos decimales sin definir)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar checked/wrapping/saturating decimal*.
Plan T15-F05 (DeepSeekDocs): *definir qué modos tienen significado; checked diagnostica/retorna error
según contexto; saturating limita a min/max; wrapping sólo si existe definición matemática aprobada,
nunca por accidente de bits*.

Estado real:

- Los modos checked/wrapping/saturating existen para enteros; para `decimal` no está definido qué
  modos tienen significado (overflow/rounding del coeficiente 96 bits + escala 0–28).

## Gap

Falta definir los modos decimales: checked (diagnostica/error según contexto), saturating (min/max),
wrapping (sólo si hay definición matemática aprobada, nunca por accidente de bits).

## Nota

F05 depende de F04 (aritmética) + los modos enteros existentes. No es una surface nueva; es la
definición de overflow/rounding decimal.
