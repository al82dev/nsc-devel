# Milestone — NativeSharp 2.0 tanda 14 / Batch 14 milestone

## Alcance / Scope

La tanda 14 completa los ítems 389–393: sanitizers nativos, LTO completo, ThinLTO, enlace
incremental y la matriz N-1/N de compatibilidad. / Batch 14 completes items 389–393: native
sanitizers, full LTO, ThinLTO, incremental linking, and the N-1/N compatibility matrix.

## Baseline certificado / Certified baseline

```text
Run: 20260915-182514-ff7c8fb1
Compiler build: 1.0.1-dev+build.20260916T002515Z.4e712f79297a45e9
Compiler SHA-256: 60dc8edbdc405d9dd252f18d6acd580ce5701b4b1ed15d0300ec73616d4209cf
NSLink SHA-256: 3804675135132ef82280ba53435ca80ff44a282779eb224866d05a98d8eea39a
Source digest S0/S1: 2e11ac83cba72afe426b49004fe5a1bad1d2920e3c91cea25ec811fcc6cde9a2
lld-link CTest: 421/421 passed
NSLink CTest: 421/421 passed
lld-link fixtures: 148/148 passed (O0/O2)
NSLink fixtures: 148/148 passed (O0/O2)
lld-link bug triage: 18/18 passed (O0/O2)
NSLink bug triage: 18/18 passed (O0/O2)
Gate 0: LANDED
Elapsed: 1113.047 s
```

La evidencia canónica queda en `_abi/gates/G00/gate.json`; el reporte detallado está en
`_nscbuilder/history/20260915-182514-ff7c8fb1/report.md`. El digest de fuentes fue idéntico antes y
después del baseline. La ejecución desatendida terminó sin diálogos interactivos ni reportes WER
nuevos. / Canonical evidence lives in `_abi/gates/G00/gate.json`; the detailed report is in
`_nscbuilder/history/20260915-182514-ff7c8fb1/report.md`. The source digest was unchanged across the
baseline. The unattended run completed without interactive dialogs or new WER reports.

## Mensaje de commit / Commit message

`Cierra la tanda 14 / Close batch 14`
