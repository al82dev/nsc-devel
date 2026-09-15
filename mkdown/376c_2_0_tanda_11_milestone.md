# Milestone — NativeSharp 2.0 tanda 11 / Batch 11 milestone

## Alcance / Scope

La tanda 11 completa los ítems 370–376: sincronización con nombre, `ThreadPool`, trabajos
tipados, dependencias y cancelación, colas locales por trabajador, robo de trabajo y propagación
del estado floating-point del compilador a sus trabajadores. / Batch 11 completes items 370–376:
named synchronization, `ThreadPool`, typed jobs, dependencies and cancellation, per-worker local
queues, work stealing, and compiler floating-point state propagation to its workers.

## Baseline certificado / Certified baseline

```text
Run: 20260915-151750-1061201a
Compiler build: 1.0.1-dev+build.20260915T211808Z.0e692a7c29fc7b80
Compiler SHA-256: c828d2876fa34ac2ae72832de81b83d8abfac4707a4c8a7e7e7d4dda33efdb74
NSLink SHA-256: 3804675135132ef82280ba53435ca80ff44a282779eb224866d05a98d8eea39a
Source digest S0/S1: 4db86f3c2c6485a4ee6c37f99656aabce71d331ffaadaed4b1b320c7c95f8336
lld-link CTest: 411/411 passed
NSLink CTest: 411/411 passed
lld-link fixtures: 148/148 passed (O0/O2)
NSLink fixtures: 148/148 passed (O0/O2)
lld-link bug triage: 18/18 passed (O0/O2)
NSLink bug triage: 18/18 passed (O0/O2)
Gate 0: LANDED
Elapsed: 974.245 s
```

La evidencia canónica queda en `_abi/gates/G00/gate.json`; el reporte detallado está en
`_nscbuilder/history/20260915-151750-1061201a/report.md`. El digest de fuentes fue idéntico antes y
después del baseline. / Canonical evidence lives in `_abi/gates/G00/gate.json`; the detailed report
is in `_nscbuilder/history/20260915-151750-1061201a/report.md`. The source digest was unchanged
across the baseline.

## Mensaje de commit / Commit message

`Cierra la tanda 11 / Close batch 11`
