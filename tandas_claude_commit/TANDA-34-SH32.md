# Tanda 34 — SH32 — commit

**Estado:** gate dirigido aprobado; listo para commit.

`selfhost(tanda-34): lower heap and stack arrays`

SH32 lowers native heap arrays through `GetProcessHeap`/`HeapAlloc`, releases `delete[]` through
`HeapFree`, and combines them with stack allocation and typed indexing. The O0-O3 LLD/NSLink
matrix returns 42, while MIR and COFF inspection prove allocation, indexing, release and imports.

No entran artefactos de `selfhost/out`, objetos, ejecutables, logs ni response files.
