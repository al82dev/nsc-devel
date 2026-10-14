# NS304-T15-F07 — Consteval/runtime y lowering decimal

- **Estado**: AUDITADO (consteval + lowering existen para enteros/FP; decimal sin integrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar consteval/runtime y lowering*.
Plan T15-F07 (DeepSeekDocs): *una sola semántica compartida o corpus diferencial exhaustivo; lowering
usa intrinsics/helpers estáticos versionados con ABI estable; consteval y runtime producen mismo bit
pattern o mismo error*.

Estado real:

- El consteval + el lowering existen para enteros/FP; `decimal` no tiene semántica compartida ni
  lowering (intrinsics/helpers estáticos).

## Gap

Falta la semántica compartida de decimal (o corpus diferencial) + el lowering por intrinsics/helpers
estáticos con ABI estable + la equivalencia consteval/runtime (mismo bit pattern o error).

## Nota

F07 depende de F04/F05 (aritmética/modos) + el consteval/lowering existente. No es una surface nueva;
es el lowering + la equivalencia consteval/runtime del decimal.
