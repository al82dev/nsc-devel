# Parte B 3.0.4 — residuales pendientes (estado de sesión)

Resumen honesto de lo que queda tras ~56 commits. Los ítems cerrados/avanzados están en el
`NATIVESHARP-3.0.3-A-3.0.6-TRACKING-MAESTRO.md` (marcas `[x]`).

## Residuos acotados (multi-capa media)

- **T08-F03** — lookup Sema de `T.Miembro()` estático de extensión. Causa raíz: en
  `ProjectNameLookup::predeclareNode` la predeclaración de funciones de namespace exige
  `!namespaceSegments.empty()`; la función estática de extensión en scope global cae en ese hueco
  (namespace vacío) y no se indexa como función global.
- **T09-F05** — builder protocol y semántica de copia de single-spread (`[..a]` aún aliasa la
  fuente en vez de materializar una copia independiente).

## Bloques duros (multi-capa grande)

- **T05-F03+ / T06-F02+ / T07** — state machines async/generator/async-streams (transformación
  productiva a HIR/MIR, frame layout, liveness, unwind).
- **T03-F03** — Span/ReadOnlySpan sobre array/inline (bloqueado: `ensureTypeSpecialization` no
  materializa el tipo desde cuerpos de método; clave cerrada correcta).
- **T15** — decimal.
- **T10-F01** — literales UTF-8 tipados (`u8"..."`/`Utf8`/`ReadOnlySpan<byte>`).
- **T10-F03** — interpolation handlers productivos (stack/writer/custom sink).
- **T11-T17** — spans/ref-like, partial members, allocators, scope/in-place, concurrency, async
  iterators, etc.

## Nota

Los commits aislados por ítem (`deepseek - ...`) y las pruebas puntuales (`TL-LANG-*`, `TP-PAR-*`,
`TL-LEX-*`, `TA-AST-*`) se mantienen verdes: `TL-LANG-300..356` sin fallos a la fecha.
