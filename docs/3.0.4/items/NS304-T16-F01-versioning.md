# NS304-T16-F01 — Versiones definitivas HIR/MIR/NSMETA/runtime ABI

- **Estado**: COMPLETO — invariantes consolidados + matriz de versiones (sin gate de versionado NSMETA).
- **Rama**: development

## Implementado (2026-10-15)

- `5b055aac` — cierre del residual:
  - **Invariantes consolidados**: `verifyLibraryMetadataInvariants(metadata, error)` comprueba que los
    feature bits reservados de `languageFeatureFlags` sean cero y delega en `verifyFrozenAbiContract`
    (las cuatro versiones ABI + la celda delegate) — el rechazo de datos inconsistentes sin
    incrementar por documentación ni reutilizar bits.

## Matriz de versiones (decisiones físicas que justifican cada bump)

| Constante | Valor | Decisión física que la fija |
|---|---|---|
| `kNativeAbiMajor.Minor` | 1.0 | contrato binario NativeSharp 1.0 (fingerprint ABI) |
| `kRuntimeAbiMajor.Minor` | de `runtime` | runtime ABI enlazado |
| `kManglingAbiVersion` | 1 | esquema de mangling por formato (sin colisión `decimal`/`long`) |
| `kObjectAbiVersion` | 2 | item 319: descriptor de clase con entry de destrucción sólo-objeto (field 7) |
| `kDelegateAbiVersion` | 1 | celda delegate 32B + `extern C delegate` = puntero C crudo |
| `kExceptionAbiVersion` | 1 | transporte SEH de excepciones |

Los feature bits (flags de miembro/campo/tipo/lenguaje) están documentados en `LibraryMetadata.h`
(bits 0..30 de miembro, 0..11 de tipo, 0..2 de lenguaje) — sin reutilizar bits y sin versionado
NSMETA (retirado de la matriz por decisión del usuario, ver T08-F07).

## Verificado O0

- `TF-INTEROP-385..392` (CF-INTEROP-049, Host.Interop, `nsc_component_tests`): PASS — metadata
  consistente pasa; bits reservados rechazados; ABI mismatch rechazado; determinismo.

## Contexto

- Las versiones ABI por formato ya estaban reservadas (2026-10-14). Aquí se cerró la validación
  consolidada de invariantes + la matriz documentada. El writer/reader con fixtures históricos queda
  para T16-F02.
