# NS2-PERF-P04 — shared OOP HIR lookup indexes

## Change

`ProjectOopHirModel` previously rebuilt 18 complete syntax-to-binding hash
indexes inside every per-member parallel worker. The indexes are immutable
after construction, so this multiplied allocation, hashing, and memory use by
the number of callable members without changing any result.

The root HIR model now builds each index once, reserves its final capacity, and
shares it read-only with the per-member workers. Worker lifetime remains bounded
by the root worker-pool wait, and all produced functions are still merged in
member ordinal order.

## Measured result

- Before run: `20260917-191114-0bad0d02` (3 measured samples/configuration)
- After run: `20260917-192614-0bad0d02` (5 measured samples/configuration)
- Matrix used for direct comparison: four corpora, object output, `-g0`, O0–O3
- Compilation/execution failures: 0 before, 0 after
- Artifact sizes: unchanged for every compared configuration
- Median OOP HIR phase: 138.095 ms → 8.877 ms (`-93.6%`)

| Corpus | Level | Before | After | Change |
|---|---:|---:|---:|---:|
| cross-project-constraints | O0 | 477.823 ms | 338.471 ms | -29.2% |
| cross-project-constraints | O1 | 1071.754 ms | 936.420 ms | -12.6% |
| cross-project-constraints | O2 | 1181.995 ms | 1050.641 ms | -11.1% |
| cross-project-constraints | O3 | 1176.279 ms | 1070.910 ms | -9.0% |
| generic-overload | O0 | 570.621 ms | 397.836 ms | -30.3% |
| generic-overload | O1 | 1165.295 ms | 1016.386 ms | -12.8% |
| generic-overload | O2 | 1326.712 ms | 1143.034 ms | -13.8% |
| generic-overload | O3 | 1327.797 ms | 1168.571 ms | -12.0% |
| medium-runtime-type-root | O0 | 477.711 ms | 346.741 ms | -27.4% |
| medium-runtime-type-root | O1 | 1066.462 ms | 946.682 ms | -11.2% |
| medium-runtime-type-root | O2 | 1206.264 ms | 1036.861 ms | -14.0% |
| medium-runtime-type-root | O3 | 1204.213 ms | 1075.821 ms | -10.7% |
| minimal-control-flow | O0 | 459.517 ms | 334.684 ms | -27.2% |
| minimal-control-flow | O1 | 1126.219 ms | 930.478 ms | -17.4% |
| minimal-control-flow | O2 | 1180.044 ms | 1037.875 ms | -12.0% |
| minimal-control-flow | O3 | 1168.813 ms | 1051.807 ms | -10.0% |

Median peak working set fell by approximately 8–11 MiB across the direct
comparison matrix.

## Correctness baseline

Canonical baseline run `20260917-192815-1f7e388a` passed with zero failures:

- 435/435 CTests with `lld-link`
- 148/148 fixtures and 18/18 triage cases with `lld-link`
- 435/435 CTests with NSLink
- 148/148 fixtures and 18/18 triage cases with NSLink
- Gate 0 LANDED

The run rebuilt both test trees because the public model header changed. Its
total duration was 593.128 seconds. The installed compiler is
`2.0.0+build.20260918T012817Z.291a2928d16e53f7`.
