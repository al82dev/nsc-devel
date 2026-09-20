# Tanda 30 — SH28 — commit

**Estado:** gate dirigido aprobado; listo para commit.

## Commit

`selfhost(tanda-30): require validated whole-program IR`

SH28 introduces `ValidatedIrProgram` as the only backend admission object, removes the raw
syntax/semantic backend entry, recursively discovers namespaced functions in MIR, and exposes
deterministic whole-project HIR/MIR/CFG/dominance/SSA dumps. All five surfaces match byte-for-byte
across O0–O3 and contain the three functions executed by the dependent SH27 linker matrix.

No se incluyen salidas de `selfhost/out`, objetos, ejecutables, logs ni temporales.
