# NS304-T16-F02 — Writer/reader y compatibilidad 3.0

- **Estado**: PARCIAL (writer/reader + rechazo de versión futura operativos; fixtures binarios + round-trip de campos nuevos pendientes)
- **Rama**: development

## Resultado (codec + rechazo — 2026-10-14)

- **Writer/reader**: el codec versiona por `formatMinor` (los campos nuevos van `if (minor >= N)` en
  writer y reader); el rechazo explícito de versión futura ya existe
  (`formatMajor != kFormatMajor || formatMinor > kFormatMinor` → error, `LibraryMetadata.cpp:3656/4218`).

## Residual

- El **round-trip de cada campo nuevo T03–T15** + los **defaults seguros de metadata 3.0 anterior** +
  los **fixtures binarios de versión previa** (no fabricados en memoria) — el corpus de compatibilidad.
