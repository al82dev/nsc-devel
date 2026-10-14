# NS304-T16-F01 — Versiones definitivas HIR/MIR/NSMETA/runtime ABI

- **Estado**: PARCIAL (versiones ABI por formato ya reservadas; matriz de cambios T03–T15 + feature bits/invariantes pendientes)
- **Rama**: development

## Resultado (reserva de versiones — 2026-10-14)

- **Constantes ABI**: `kManglingAbiVersion`/`kObjectAbiVersion`/`kDelegateAbiVersion`/
  `kExceptionAbiVersion` ya existen como "una sola versión por formato" en `LibraryMetadata.h`.

## Residual

- La **matriz de cambios reales (T03–T15)** que justifique cada bump + los **feature bits/invariantes**
  + el **rechazo de datos inconsistentes** (sin incrementar por documentación ni reutilizar bits) —
  el doc + la validación del codec.
