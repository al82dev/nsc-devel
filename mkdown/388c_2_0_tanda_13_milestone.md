# Milestone — NativeSharp 2.0 tanda 13 / Batch 13 milestone

## Alcance / Scope

La tanda 13 completa los ítems 385–388: núcleo `comptime/#run`, sandbox y presupuestos,
dependencias/caché y ejecución reproducible. / Batch 13 completes items 385–388: the
`comptime/#run` core, sandbox and budgets, dependency caching, and reproducible execution.

## Baseline certificado / Certified baseline

```text
Run: 20260915-171553-419cc1ff
Compiler build: 1.0.1-dev+build.20260915T231555Z.9c22b1f7971db22f
Compiler SHA-256: f9487151495ddb44cd0b1ee47d943b36e440f20b7a43ae7b022516b8b2100509
NSLink SHA-256: 3804675135132ef82280ba53435ca80ff44a282779eb224866d05a98d8eea39a
Source digest S0/S1: 612a20dd2d10c90685702e83b91a9085a2085361036f600e8b0680e13780fc5c
lld-link CTest: 416/416 passed
NSLink CTest: 416/416 passed
lld-link fixtures: 148/148 passed (O0/O2)
NSLink fixtures: 148/148 passed (O0/O2)
lld-link bug triage: 18/18 passed (O0/O2)
NSLink bug triage: 18/18 passed (O0/O2)
Gate 0: LANDED
Elapsed: 713.736 s
```

La evidencia canónica queda en `_abi/gates/G00/gate.json`; el reporte detallado está en
`_nscbuilder/history/20260915-171553-419cc1ff/report.md`. El digest de fuentes fue idéntico antes y
después del baseline. / Canonical evidence lives in `_abi/gates/G00/gate.json`; the detailed report
is in `_nscbuilder/history/20260915-171553-419cc1ff/report.md`. The source digest was unchanged
across the baseline.

## Mensaje de commit / Commit message

`Cierra la tanda 13 / Close batch 13`
