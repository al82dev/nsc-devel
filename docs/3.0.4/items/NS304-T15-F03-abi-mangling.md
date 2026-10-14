# NS304-T15-F03 — Identidad, ABI, mangling, NSMETA y FFI

- **Estado**: AUDITADO (type/layout/mangling/FFI existen para tipos; decimal sin integrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar identidad/ABI/mangling/NSMETA/FFI del decimal*.
Plan T15-F03 (DeepSeekDocs): *tipo distinto de enteros/FP; layout/alignment y pass/return Win64
explícitos; mangling estable; metadata versionada; FFI sólo con representación declarada, no asumir
compatibilidad externa*.

Estado real:

- type/layout/mangling/ABI/FFI existen para enteros/FP; `decimal` (128 bits) no tiene type key, layout,
  mangling ni metadata.

## Gap

Falta el tipo `decimal` distinto de enteros/FP + layout/alignment + pass/return Win64 + mangling
estable + metadata versionada + FFI con representación declarada.

## Nota

F03 depende de F01 (representación) + el type/layout/mangling existente. No es una surface nueva; es
el integrar `decimal` al type-system + ABI + NSMETA.
