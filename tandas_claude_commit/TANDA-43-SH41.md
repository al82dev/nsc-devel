# Tanda 43 — SH41 — commit

**Estado:** `LANDED`; gate dirigido y baseline acumulado aprobados.

`selfhost(tanda-43): complete the first source-derived self-compilation`

SH41 replaces the historical image replay with real source-derived LLVM/COFF generation. The
NativeSharp nsc1 builds nsc2, nsc2 builds nsc3, and both generations reach a byte-identical object
fixed point for O0-O3. LLD and NSLink execute all 16 generated images, while process-tree auditing
proves that the seed compiler is never invoked during generation.

The commit contains source, contracts, the compact gate report and documentation only. Objects,
executables, import libraries, response files, logs and `selfhost/out` remain excluded.

Baseline `tanda-43-sh41-autocompilacion-r3-20260920`: 476/476 CTest for each linker,
148/148 fixtures and 18/18 triage cases for each linker, zero failures, Gate 0 certified in
2390.086 seconds. The installed compiler is build `4300000000000002` under `A:\native-sharp`.
