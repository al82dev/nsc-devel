# Tanda 35 — SH33 — commit

**Estado:** gate dirigido aprobado; listo para commit.

`selfhost(tanda-35): lower native interop signatures`

SH33 separates ABI declarations from emitted bodies, preserves exact extern-C names, lowers
ref/out/in as addresses, and carries terminal varargs into LLVM function types and calls. The
O0-O3 LLD/NSLink matrix imports GetProcessHeap and returns 42 in every route.

No entran artefactos de `selfhost/out`, objetos, ejecutables, logs ni response files.
