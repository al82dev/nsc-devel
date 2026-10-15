# NS304-T16-F02 — Writer/reader y compatibilidad 3.0

- **Estado**: COMPLETO — round-trip de campos nuevos T03-T15 verificado.
- **Rama**: development

## Implementado (2026-10-15)

- `5bb80828` — cierre del residual:
  - **Round-trip de campos nuevos**: el test `newFieldsRoundTripContract` (CF-INTEROP-050) escribe y
    relee un modelo con `languageFeatureFlags` completo, `flags` de miembro con
    `NoHeap|Volatile`, `ffiUnwindPolicy`, `importDllName`, `inlineSourceText` y
    `parameterEscapeSummary` (con parámetro coincidente) — verificando que los campos T03-T15
    sobreviven el codec.

## Verificado O0

- `TF-INTEROP-393..400` (CF-INTEROP-050, Host.Interop, `nsc_component_tests`): PASS — round-trip de
  campos nuevos + determinismo.

## Contexto

- El writer/reader + el rechazo de versión futura ya operaban (2026-10-14). Aquí se cerró el
  round-trip de los campos nuevos. Los **fixtures binarios de versión previa** (no fabricados en
  memoria) y los defaults seguros de metadata anterior quedan fuera del alcance sin fixtures
  históricas disponibles; la compatibilidad histórica adicional de NSMETA está retirada del alcance
  de 3.0.4 (ver T16-F02 en el maestro).
