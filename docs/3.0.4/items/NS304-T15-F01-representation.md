# NS304-T15-F01 — Representación decimal

- **Estado**: PARCIAL (tipo builtin `decimal` + layout 128-bit cerrados; aritmética/overflow/conversiones/formato pendientes)
- **Rama**: development

## Resultado (tipo + layout — 2026-10-14)

- **Builtin**: `decimal` se resuelve como tipo valor (`builtin:decimal`) en `ProjectNameLookup` y
  `ProjectMemberModel::canonicalTypeKey`; el parser ya no lo difiere con NS3049.
- **Layout**: `TargetDataLayout::BuiltinLayoutKind::Decimal` = 16 bytes / alineación 8; LLVM lo
  representa como `i128`.
- Verificado: `struct S { decimal d; }` + `(int)sizeof(S)` → **16**. Registrado `TL-LANG-420` (PASS).

## Residual

- El **contrato numérico** (canonicalización, precision/scale 0–28, equality/hashing, rounding,
  excepciones) + los asserts/layout tests exhaustivos + la ABI classification (sin bit patterns
  reservados desde safe code) — es la documentación/invariantes que cierra F01 antes de F02 literales.
