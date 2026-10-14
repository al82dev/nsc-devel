# NS304-T15-F03 — Identidad, ABI, mangling, NSMETA y FFI

- **Estado**: PARCIAL (tipo/layout/pass-return/mangling del `decimal` operativos; metadata versionada + FFI con representación declarada pendientes)
- **Rama**: development

## Resultado (identidad + mangling — 2026-10-14)

- **Tipo/layout/pass-return**: heredados de F01 (type key `builtin:decimal` + `BuiltinLayoutKind::Decimal`
  16B + LLVM `i128`); el paso por valor y el retorno de `decimal` ya bajan por el ABI Win64 (i128).
- **Mangling**: `typeSuffix` usa `symbolFragmentForIdentityKey(typeKey)`, por lo que `builtin:decimal`
  es un fragmento distinto y estable — `M(decimal)` y `M(long)` coexisten sin colisión.
- Verificado: overload `M(decimal)`/`M(long)` + dispatch → exit 0. Registrado `TL-LANG-422` (PASS).

## Residual

- **Metadata versionada** del `decimal` en NSMETA y el **FFI con representación declarada** (no asumir
  compatibilidad externa del layout de 128 bits) — se completan con la integración de biblioteca/FFI.
