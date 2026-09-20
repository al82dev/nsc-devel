# Tanda 39 — SH37 — commit

**Estado:** gate dirigido aprobado; listo para commit.

`selfhost(tanda-39): lower bootstrap compile-time forms`

SH37 lowers typed default, sizeof and alignof constants and carries static assertions through the
validated backend. AST inspection and O0-O3 execution pass through LLD and NSLink.

No entran artefactos de `selfhost/out`, objetos, ejecutables, logs ni response files.
