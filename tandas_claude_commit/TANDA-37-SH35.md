# Tanda 37 — SH35 — commit

**Estado:** gate dirigido aprobado; listo para commit.

`selfhost(tanda-37): enforce ownership exit cleanup`

SH35 preserves ownership on compound types, lowers move/release operations, and inserts reverse
cleanup-if-live records before exits. The ownership fixture is verified in MIR and returns 42 for
all O0-O3 LLD/NSLink routes.

No entran artefactos de `selfhost/out`, objetos, ejecutables, logs ni response files.
