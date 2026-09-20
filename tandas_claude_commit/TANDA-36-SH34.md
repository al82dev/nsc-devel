# Tanda 36 — SH34 — commit

**Estado:** gate dirigido aprobado; listo para commit.

`selfhost(tanda-36): canonicalize generic AOT instances`

SH34 preserves explicit generic arguments at call sites and builds a deterministic AOT instance
registry keyed by name, normalized arguments and arity. Duplicate call sites collapse, ordinal
output is identical across profiles, and the O0-O3 LLD/NSLink matrix returns 42.

No entran artefactos de `selfhost/out`, objetos, ejecutables, logs ni response files.
