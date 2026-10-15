# NS304-T15-F03 — Identidad, ABI, mangling, NSMETA y FFI

- **Estado**: COMPLETO — FFI del `decimal` con representación declarada (sin metadata versionada, que
  queda prohibida por el scope).
- **Rama**: development

## Implementado (2026-10-15)

- `d07fa446` — cierre del residual dentro del scope:
  - **Gate C ABI**: `isStage47CAbiType` admite `builtin:decimal`, de modo que
    `export extern C decimal` se acepta como tipo de retorno y de parámetro.
  - **Representación declarada**: `CHeaderGenerator::hostType` mapea `builtin:decimal` a `NS_Decimal`,
    y el header emite `typedef struct { uint64_t lo; uint64_t hi; } NS_Decimal;` con el comentario de
    layout (coeficiente 96 bits | escala bits 96..101 | signo bit 127) — la representación se declara,
    no se asume un `__int128` externamente compatible.

## Verificado O0

- `TL-LANG-504` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — un proyecto Library con
  `export extern C decimal Add(decimal,decimal)` emite `NS_Decimal` y
  `NS_Decimal NS_CALL Add(NS_Decimal a, NS_Decimal b)` en el header C.

## Contexto

- Tipo/layout/pass-return/mangling del `decimal` ya operaban (2026-10-14, TL-LANG-422). La metadata
  versionada en NSMETA queda **excluida** por el scope lock (prohibido reactivar versionado NSMETA);
  aquí se cerró el FFI con representación declarada.
