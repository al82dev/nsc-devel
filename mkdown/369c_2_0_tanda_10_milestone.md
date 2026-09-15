# Milestone — NativeSharp 2.0 tanda 10 / Batch 10 milestone

## Alcance / Scope

La tanda 10 completa los ítems 364–369: modos floating-point `precise`, `fast` y
`deterministic`, política de contracción FMA, control de redondeo y denormales, estado
floating-point aislado por hilo y matemáticas trascendentales deterministas. / Batch 10 completes
items 364–369: `precise`, `fast`, and `deterministic` floating-point modes, FMA contraction policy,
rounding and denormal controls, per-thread floating-point state, and deterministic transcendental
math.

## Baseline certificado / Certified baseline

```text
Run: 20260915-141134-b662ed37
Compiler build: 1.0.1-dev+build.20260915T201138Z.acf3cbc49790c267
Compiler SHA-256: 08a942848ef59c783510e7798f80297acd01f03534252cf905856fa320160d3f
NSLink SHA-256: 3804675135132ef82280ba53435ca80ff44a282779eb224866d05a98d8eea39a
Source digest S0/S1: 12cf28b21e73c4ed3e8eeecedbcad3a245d82ab5c391baef37c1c2ebc213f3e5
lld-link CTest: 404/404 passed
NSLink CTest: 404/404 passed
lld-link fixtures: 148/148 passed (O0/O2)
NSLink fixtures: 148/148 passed (O0/O2)
lld-link bug triage: 18/18 passed (O0/O2)
NSLink bug triage: 18/18 passed (O0/O2)
Gate 0: LANDED
Elapsed: 675.001 s
```

La evidencia canónica queda en `_abi/gates/G00/gate.json`; el reporte detallado está en
`_nscbuilder/history/20260915-141134-b662ed37/report.md`. El digest de fuentes fue idéntico antes y
después del baseline. / Canonical evidence lives in `_abi/gates/G00/gate.json`; the detailed report
is in `_nscbuilder/history/20260915-141134-b662ed37/report.md`. The source digest was unchanged
across the baseline.

## Mensaje de commit / Commit message

`Cierra la tanda 10 / Close batch 10`
