# NS2-PERF-P11 — prepared LLVM optimization and emission pipeline

## Change

The project object pipeline previously paid for the same LLVM safety work at
multiple adjacent boundaries:

1. the OOP lowerer verified the completed module;
2. the object generator configured the target;
3. the public optimizer verified and configured it again;
4. the optimizer verified the transformed result;
5. the public emitter verified and configured it again.

Public `LlvmOptimizer::optimize` and `LlvmTargetMachine::emit` remain fully
defensive. A private, friend-only prepared path now lets
`ProjectOopObjectGenerator` reuse the lowerer's successful verification and its
own target configuration. The optimizer still verifies its output before the
prepared emitter can consume it. This reduces three full-module verifications
to two and three target configurations to one without weakening a public API
boundary.

## Measured result

- Before run: `20260917-200807-0bad0d02`
- After run: `20260917-201118-0bad0d02`
- Matrix: four corpora, object output, `-g0`, O0-O3
- Samples: 5 measured plus 1 warmup per configuration
- Compilation failures: 0 before, 0 after
- Artifact sizes: unchanged in every compared configuration
- Median `llvm-optimization`: 398.084 ms -> 374.378 ms (`-6.0%`)
- Median `object-emission`: 367.798 ms -> 364.038 ms (`-1.0%`)

O0 end-to-end medians improved in every corpus:

| Corpus | Before | After | Change |
|---|---:|---:|---:|
| cross-project-constraints | 336.522 ms | 334.885 ms | -0.5% |
| generic-overload | 394.399 ms | 384.222 ms | -2.6% |
| medium-runtime-type-root | 351.240 ms | 346.278 ms | -1.4% |
| minimal-control-flow | 337.566 ms | 331.906 ms | -1.7% |

O1-O3 end-to-end results remained within approximately -1.8% to +3.0% of the
before medians. Those totals are dominated by LLVM pass and target codegen
variance, while the aggregate phase medians above directly measure the work
removed by this change. Peak memory remained effectively unchanged.

## Focused compatibility checks

The following tests passed after the change:

- `nsc.llvm-optimization`
- `nsc.llvm-codegen`
- `nsc.project-oop-lowering`

The canonical baseline also exercised the public invalid-module diagnostics,
artifact cache, ThinLTO, incremental link and both Windows linker paths.

## Correctness baseline

Canonical baseline run `20260917-201258-e9794ee8` passed with zero failures:

- 435/435 CTests with `lld-link`
- 148/148 fixtures and 18/18 triage cases with `lld-link`
- 435/435 CTests with NSLink
- 148/148 fixtures and 18/18 triage cases with NSLink
- Gate 0 LANDED

The run rebuilt both test trees because LLVM public headers changed. Its total
duration was 501.41 seconds. The installed compiler is
`2.0.0+build.20260918T021259Z.9a7c9ad13440d95b`.
