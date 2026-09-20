# Tanda 33 — SH31 — commit

**Estado:** gate dirigido aprobado; listo para commit.

`selfhost(tanda-33): emit native text literals`

SH31 emits deterministic zero-terminated UTF-8 globals through LLVM-C, lowers character literals
and escapes, accepts the semantic string-literal-to-char-pointer conversion, folds literal
concatenation, and executes byte indexing. The O0-O3 LLD/NSLink matrix returns 42 and object
inspection proves the `México` UTF-8 payload.

No entran artefactos de `selfhost/out`, objetos, ejecutables, logs ni response files.
