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

- Before run: `20260917-194607-0bad0d02` (P10)
- After run: `20260917-202422-0bad0d02` (P11)
- Matrix: four corpora, executable output, `-g0`, O0-O3, lld-link and NSLink
- Samples: 3 measured plus 1 warmup before; 5 measured plus 1 warmup after
- Compilation failures: 0 before, 0 after
- Execution failures: 0 before, 0 after
- Artifact sizes: unchanged in every compared configuration
- Median `llvm-object-generation`: 182.888 ms -> 185.743 ms (`+1.6%`)
- Median `executable-build`: 272.956 ms -> 262.584 ms (`-3.8%`)

The direct end-to-end configurations were mixed and the two aggregate phase
movements point in opposite directions. Therefore this change is classified as
removal of provably redundant work with **no independently measurable aggregate
speedup on the current small corpus**. It is retained because it removes two
whole-module walks per code unit while preserving both verification boundaries,
and because the full correctness baseline is clean; no percentage improvement
is claimed for P11.

Runs `20260917-200807-0bad0d02` and `20260917-201118-0bad0d02` exercised direct
object generation. They remain useful compatibility evidence, but that driver
action does not use `ProjectOopObjectGenerator` and therefore must not be cited
as the performance measurement for this change.

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
