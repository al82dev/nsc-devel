# NS304-T15-F08 — Formatting, traits, generics y bordes decimal

- **Estado**: AUDITADO (traits numéricos/generics existen; decimal sin integrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar formatting, traits, generics y bordes*.
Plan T15-F08 (DeepSeekDocs): *parsing/formatting culture-free por defecto y provider explícito si
existe; integrar traits numéricos/generics sin boxing; corpus min/max, escala 0/28, ceros, carry,
overflow, rounding ties, divide by zero y conversiones límite*.

Estado real:

- Los traits numéricos/generics existen; `decimal` no participa en ellos ni tiene parsing/formatting
  culture-free.

## Gap

Falta el parsing/formatting culture-free (provider explícito) + la integración de `decimal` en traits
numéricos/generics sin boxing + el corpus de bordes (min/max, escala 0/28, ceros, carry, overflow,
rounding ties, divide by zero, conversiones límite).

## Nota

F08 es el cierre de T15. Depende de F01–F07 + los traits/generics existentes. No es una surface nueva;
es la integración final + el corpus de bordes.
