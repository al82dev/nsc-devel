# NS304-T16-F01 — Versiones definitivas HIR/MIR/NSMETA/runtime ABI

- **Estado**: AUDITADO (LibraryMetadata/HIR/MIR con versiones históricas; matriz de cambios sin construir)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar versiones definitivas de formatos*.
Plan T16-F01 (DeepSeekDocs): *construir matriz de cambios reales y reservar una sola versión por
formato cuando sea necesario; definir feature bits, invariantes y rechazo de datos inconsistentes; no
incrementar versión por documentación ni reutilizar bits*.

Estado real:

- `LibraryMetadata`, el codec/merger/API diff, los modelos HIR/MIR y el LLVM lowerer existen con
  versiones históricas. No hay una matriz de cambios T03–T15 que reserve una sola versión por formato
  ni feature bits/invariantes.

## Gap

Falta la matriz de cambios reales (T03–T15) + reservar una sola versión por formato + los feature
bits/invariantes + el rechazo de datos inconsistentes (sin incrementar por documentación ni reutilizar
bits).

## Nota

F01 es la base de T16 (antes de F02 writer/reader + F03 producer/consumer). Es la integración de las
versiones de formatos; un hueco reabre su tanda causal T03–T15.
