# Milestone — NativeSharp 2.0 tanda 15 / Batch 15 milestone

## Alcance / Scope

La tanda 15 completa los ítems 394–401: verificadores obligatorios entre etapas, HIR del lenguaje
completo, MIR explícito para excepciones, genéricos e interfaces, encapsulamiento del backend LLVM,
contrato ABI versionado del runtime y separación del runtime Win32 en una biblioteca independiente. /
Batch 15 completes items 394–401: mandatory inter-stage verifiers, complete-language HIR, explicit
exception/generic/interface MIR, LLVM backend encapsulation, a versioned runtime ABI contract, and
the independent Win32 runtime library.

## Baseline certificado / Certified baseline

```text
Run: 20260915-213347-12284275
Compiler build: 1.0.1-dev+build.20260916T033348Z.9b0890ab62c966d3
Compiler SHA-256: 7dc0cf275b904cda83b1afaca9891d66108581a8045287f061c2cd0700f39b40
NSLink SHA-256: 3804675135132ef82280ba53435ca80ff44a282779eb224866d05a98d8eea39a
Source digest S0/S1: 300b6fe042e1dadf22868fdb4a2a85e9010b3f0ce6d3df30c58dd90e1cadcf1a
lld-link CTest: 429/429 passed
NSLink CTest: 429/429 passed
lld-link fixtures: 148/148 passed (O0/O2)
NSLink fixtures: 148/148 passed (O0/O2)
lld-link bug triage: 18/18 passed (O0/O2)
NSLink bug triage: 18/18 passed (O0/O2)
Gate 0: LANDED
Elapsed: 889.887 s
```

La evidencia canónica queda en `_abi/gates/G00/gate.json`; el reporte detallado está en
`_nscbuilder/history/20260915-213347-12284275/report.md`. El digest de fuentes fue idéntico antes y
después del baseline. La ejecución desatendida usó `--interactive-debug-mode 0` en ambas matrices y
terminó sin diálogos interactivos. / Canonical evidence lives in `_abi/gates/G00/gate.json`; the
detailed report is in `_nscbuilder/history/20260915-213347-12284275/report.md`. The source digest was
unchanged across the baseline. Both matrices ran unattended with `--interactive-debug-mode 0` and
completed without interactive dialogs.

## Mensaje de commit / Commit message

`Cierra la tanda 15 / Close batch 15`
