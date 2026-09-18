# NS2-PERF-P10 — skip unused textual LLVM IR serialization

## Change

The ordinary Windows EXE/DLL/LIB path serialized every optimized LLVM module
to text even though import planning is based on the canonical runtime import
closure and the project's explicit external imports. The runtime module was
also serialized again immediately before link planning.

The compiler now retains textual module IR only when it is required by the
compilation artifact cache or ThinLTO. Normal builds feed the import planner an
empty compatibility argument and continue to fingerprint emitted object files
for incremental linking. Cache keys, ThinLTO summaries, object verification,
external imports and linker inputs keep their previous behavior.

## Measured result

- Before run: `20260917-194341-0bad0d02`
- After run: `20260917-194607-0bad0d02`
- Matrix: four corpora, executable output, `-g0`, O0-O3, lld-link and NSLink
- Samples: 3 measured plus 1 warmup per configuration
- Compilation failures: 0 before, 0 after
- Execution failures: 0 before, 0 after
- Artifact sizes: unchanged in every compared configuration
- Median `llvm-object-generation`: 197.742 ms -> 182.888 ms (`-7.5%`)
- Median `executable-build`: 272.588 ms -> 272.956 ms (unchanged within noise)

O0 end-to-end medians improved in all eight corpus/linker combinations:

| Corpus | Linker | Before | After | Change |
|---|---|---:|---:|---:|
| cross-project-constraints | NSLink | 552.777 ms | 518.746 ms | -6.2% |
| cross-project-constraints | lld-link | 655.859 ms | 617.193 ms | -5.9% |
| generic-overload | NSLink | 603.969 ms | 545.161 ms | -9.7% |
| generic-overload | lld-link | 699.623 ms | 669.315 ms | -4.3% |
| medium-runtime-type-root | NSLink | 560.152 ms | 515.470 ms | -8.0% |
| medium-runtime-type-root | lld-link | 660.390 ms | 623.925 ms | -5.5% |
| minimal-control-flow | NSLink | 553.764 ms | 502.780 ms | -9.2% |
| minimal-control-flow | lld-link | 648.535 ms | 602.600 ms | -7.1% |

O1-O3 generally improved by approximately 1-6%; two of the 24 combinations
were within +1.3% of the before median, consistent with normal process noise.
Peak working set fell by approximately 3-9 MiB. The optimization therefore
removes measurable work at every optimization level without changing output.

## Focused compatibility checks

The following tests passed after the change:

- `nsc.project-oop-lowering`
- `nsc.compilation-artifact-cache`
- `nsc.compilation-artifact-cache-driver`
- `nsc.item391-thinlto`
- `nsc.item392-incremental-link`

## Correctness baseline

Canonical baseline run `20260917-195631-62626793` passed with zero failures:

- 435/435 CTests with `lld-link`
- 148/148 fixtures and 18/18 triage cases with `lld-link`
- 435/435 CTests with NSLink
- 148/148 fixtures and 18/18 triage cases with NSLink
- Gate 0 LANDED

The total duration was 423.34 seconds. The installed compiler is
`2.0.0+build.20260918T015632Z.4c5630f369395410`.

An earlier baseline attempt produced one transient return-value mismatch in
`nsc.assembly.abi-c-cpp-o0` while running 16 tests concurrently. The same test
then passed 20 consecutive isolated repetitions and passed in the clean
canonical rerun above. No failed run was accepted as evidence.
