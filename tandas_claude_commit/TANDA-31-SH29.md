# Tanda 31 — SH29 — commit

**Estado:** gate dirigido aprobado; listo para commit.

## Commit

`selfhost(tanda-31): lower static data and verify layouts`

SH29 exposes deterministic project layout metadata for enums, structs, classes and fields; emits
typed scalar globals with constant initialization and stable names; and loads them by semantic
symbol. It fixes public-const recognition and switches LLVM object generation to PIC after the gate
found invalid ADDR32 image relocations. O0–O3 objects now use REL32 globals and return 42 through
both LLD and NSLink.

No entran objetos, ejecutables, response files, logs ni salidas de `selfhost/out`.
