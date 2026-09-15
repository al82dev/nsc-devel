# Milestone — NativeSharp 2.0 tanda 9 / Batch 9 milestone

## Alcance / Scope

La tanda 9 completa los ítems 347–363: vectores SIMD nativos de 128 y 256 bits, memoria alineada
y no alineada, vectores compactos, matrices, `half`, formatos GPU, shuffle, matemáticas vectoriales,
intrínsecos de bits, prefetch/pause, contador de CPU y perfiles reproducibles de features con
fallback. / Batch 9 completes items 347–363: native 128- and 256-bit SIMD vectors, aligned and
unaligned memory, compact vectors, matrices, `half`, GPU formats, shuffle, vector math, bit
intrinsics, prefetch/pause, CPU timestamps, and reproducible CPU feature profiles with fallback.

## Baseline certificado / Certified baseline

```text
Run: 20260915-130611-44b177fc
Compiler build: 1.0.1-dev+build.20260915T190612Z.a73b248084ee67a8
Compiler SHA-256: b7a50cea4b5df7a397513cca564b4934039fb4bb74f1bf1ec1da45fa0185f139
NSLink SHA-256: 3804675135132ef82280ba53435ca80ff44a282779eb224866d05a98d8eea39a
Source digest S0/S1: efbeb3976e201943925f03fdaead45f27fefc56ebef7fda43ed05cd757017c0e
lld-link CTest: 398/398 passed
NSLink CTest: 398/398 passed
lld-link fixtures: 148/148 passed (O0/O2)
NSLink fixtures: 148/148 passed (O0/O2)
lld-link bug triage: 18/18 passed (O0/O2)
NSLink bug triage: 18/18 passed (O0/O2)
Gate 0: LANDED
Elapsed: 922.501 s
```

La evidencia canónica queda en `_abi/gates/G00/gate.json`; el reporte detallado está en
`_nscbuilder/history/20260915-130611-44b177fc/report.md`. El digest de fuentes fue idéntico antes y
después del baseline. / Canonical evidence lives in `_abi/gates/G00/gate.json`; the detailed report
is in `_nscbuilder/history/20260915-130611-44b177fc/report.md`. The source digest was unchanged
across the baseline.

## Mensaje de commit / Commit message

`Cierra la tanda 9 / Close batch 9`
