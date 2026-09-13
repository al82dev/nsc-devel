# 297 — Validación x64 y CPU de asm / Assembly x64 and CPU validation

## Resultado / Result

Assembly se valida contra un target explícito `x86-64-v1`: nombres físicos reales y extensiones
garantizadas. Registros inventados o instrucciones que necesitan una feature ausente se rechazan
antes de HIR. / Assembly is validated against an explicit `x86-64-v1` target: real physical names
and guaranteed extensions. Unknown registers and instructions requiring absent features are
rejected before HIR.

## Archivos y funciones / Files and functions

- `include/nativesharp/Target/TargetInfo.h` y `src/Target/TargetInfo.cpp`
  - `cpuBaselineName()` fija la identidad reproducible.
  - `supportsCpuFeature()` enumera garantías, sin consultar el host.
  - `isAssemblyRegister()` valida GPR, MMX, x87 y XMM de x64.
  - `requiredAssemblyFeature()` clasifica extensiones no baseline.
- `src/Parser/Parser.cpp`
  - Valida constraints, clobbers e instrucciones mediante `TargetInfo`.
- `include/nativesharp/Diagnostics/DiagnosticId.h`
  - Agrega `InvalidInlineAssemblyTarget`.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp` y `TargetDataLayoutTests.cpp`
  - Certifican registros válidos/inválidos y rechazo AVX con feature nombrada.
- `docs/1.0.1`
  - Actualiza especificación, contratos, mapas, estado, changelog y roadmap.

## Implementación y decisiones / Implementation and design decisions

El catálogo pertenece al target, no al parser, para que lowering, diagnostics y futuros targets
consulten una sola verdad. El baseline no depende de CPUID del equipo de build: el mismo source
produce la misma decisión en cualquier host. / The catalog belongs to the target, not the parser,
so lowering, diagnostics, and future targets share one truth. The baseline never depends on build
host CPUID, preserving deterministic decisions.

## Evidencia enfocada / Focused evidence

```text
nsc.integrated-assembly.grammar: PASS: 47 checks
nsc.target-data-layout: passed with direct TargetInfo assertions
```

No se ejecutó baseline. / No baseline was run.

## Mensaje de commit / Commit message

**Título / Title**

`Valida asm contra el target x64 y sus features / Validate asm against x64 target features`

**Descripción / Description**

`Centraliza registros y baseline x86-64-v1 en TargetInfo y rechaza extensiones ausentes antes de HIR. / Centralize registers and the x86-64-v1 baseline in TargetInfo and reject absent extensions before HIR.`
