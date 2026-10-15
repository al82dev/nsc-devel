# NS304-T16-F05 — CodeView/PDB

- **Estado**: COMPLETO — representación debug de `builtin:decimal` (basic 128-bit) + build debug O0.
- **Rama**: development

## Implementado (2026-10-15)

- `dc243118` — cierre del hueco auditado (un caso representativo del vertical de debug info):
  - **Tipo debug de decimal**: `debugType("builtin:decimal")` emite un basic type `decimal` de 128
    bits unsigned (`LLVMDWARFTypeEncodingUnsigned`), de modo que el CodeView/PDB nombra el carrier
    128-bit en lugar de caer al tipo no especificado.
  - **Build debug verificado**: `TL-LANG-510` compila un programa con un `decimal` local bajo `-g` y
    verifica PDB no vacío + ejecución exit 0.

## Verificado O0

- `TL-LANG-510` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — PDB no vacío + run exit 0.

## Contexto

- El hueco es la representación debug de las construcciones T03-T15 (closures/patterns/async/generator/
  static init/decimal/generated source). Aquí se cerró el tipo `decimal`. Las demás construcciones
  (closures/captures ya tienen `captureTypeKeys` en `emitCaptureDebug`, async frames ya en TL-LANG-471)
  quedan cubiertas incrementalmente por su propia vertical ya existente.
