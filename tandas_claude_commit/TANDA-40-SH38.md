# Tanda 40 — SH38 — commit

**Estado:** gate dirigido aprobado; listo para commit.

`selfhost(tanda-40): publish Win64 EH regions`

SH38 publishes a deterministic exception-region graph and lowers the normal try/finally path.
Every O0-O3 object exposes Win64 unwind records and returns 42 through LLD and NSLink.

No entran artefactos de `selfhost/out`, objetos, ejecutables, logs ni response files.
