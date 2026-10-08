# Tanda 20H — Unidades históricas faltantes

## Hipótesis causal

Veintinueve pruebas clasificadas como `KEEP_CPP_UNIT` conservaban sus owners y
sus expectativas, pero no habían sido registradas en `nsc_unit_tests`. Por eso
la infraestructura nueva no podía enumerarlas ni ejecutarlas individualmente.

## Corrección

Cada owner se incorpora mediante un adaptador independiente que renombra sólo
su función `main` y reutiliza literalmente su cuerpo. No se copiaron assertions,
no se cambiaron expectativas y no se fusionaron varios owners en una misma
unidad de traducción. Los registros se añadieron a los módulos existentes de
backend, semántica y proyecto/driver/metadata, conservando su ID y componente.

El ejecutable consolidado enlaza la clausura que esos owners ya declaraban en
sus targets históricos: compilación, driver, frontend, language service,
linker, LLVM, runtime y target.

## Inventario verificado uno por uno

| ID | Owner | Resultado dirigido |
|---|---|---|
| `nsc.compilation-artifact-cache` | `CompilationArtifactCacheTests.cpp` | PASS |
| `nsc.compilation-graph-scheduler` | `CompilationGraphSchedulerTests.cpp` | PASS |
| `nsc.compilation-worker-pool` | `CompilationWorkerPoolTests.cpp` | PASS |
| `nsc.item363.cpu-feature-selection` | `Item363CpuFeatureSelectionTests.cpp` | PASS |
| `nsc.item386.comptime-sandbox` | `Item386ComptimeSandboxTests.cpp` | PASS |
| `nsc.item404.release-candidate-freeze` | `Item404ReleaseCandidateFreezeTests.cpp` | PASS |
| `nsc.item405.final-compatibility` | `Item405FinalCompatibilityCertificationTests.cpp` | PASS |
| `nsc.item406.final-baseline-contract` | `Item406FinalBaselineContractTests.cpp` | PASS |
| `nsc.item407.official-promotion` | `Item407OfficialPromotionTests.cpp` | PASS |
| `nsc.language-service.identity` | `LanguageServiceIdentityTests.cpp` | PASS |
| `nsc.language.inlining-policy` | `InliningPolicyTests.cpp` | PASS |
| `nsc.ns3.t03.sum-types` | `NativeSharp3T03SumTypeTests.cpp` | PASS |
| `nsc.ns3.t14.numeric` | `NativeSharp3T14NumericTests.cpp` | PASS |
| `nsc.ns3.t15.interaction-matrix` | `NativeSharp3T15InteractionMatrixTests.cpp` | PASS |
| `nsc.ns3.t15.performance-budgets` | `NativeSharp3T15PerformanceBudgetTests.cpp` | PASS |
| `nsc.ns3.t15.whole-program-corpus` | `NativeSharp3T15WholeProgramCorpusTests.cpp` | PASS |
| `nsc.ns3.t16.compatibility` | `NativeSharp3T16CompatibilityTests.cpp` | PASS |
| `nsc.ns3.t17.bilingual-spec-audit` | `NativeSharp3T17BilingualSpecAuditTests.cpp` | PASS |
| `nsc.ns3.t17.conformance-rc` | `NativeSharp3T17ConformanceRcTests.cpp` | PASS |
| `nsc.ns3.t17.official-promotion` | `NativeSharp3T17OfficialPromotionTests.cpp` | PASS |
| `nsc.ns3.t17.release-candidates` | `NativeSharp3T17ReleaseCandidatesTests.cpp` | PASS |
| `nsc.ns3.t17.release-contract` | `NativeSharp3T17ReleaseTests.cpp` | PASS |
| `nsc.ns3.t17.semantic-freeze` | `NativeSharp3T17SemanticFreezeTests.cpp` | PASS |
| `nsc.ns3.t17.syntax-freeze` | `NativeSharp3T17SyntaxFreezeTests.cpp` | PASS |
| `nsc.project-callable-analysis-result` | `ProjectCallableAnalysisResultTests.cpp` | PASS |
| `nsc.project-callable-binding-index` | `ProjectCallableBindingIndexTests.cpp` | PASS |
| `nsc.project-callable-wave-reducer` | `ProjectCallableWaveReducerTests.cpp` | PASS |
| `nsc.project-callable-wave-scheduler` | `ProjectCallableWaveSchedulerTests.cpp` | PASS |
| `nsc.stage69.migration-baseline` | `Stage69MigrationBaselineTests.cpp` | PASS |

## Evidencia

- Construcción dirigida: `cmake --build A:\nsc-devel\build-test-v2 --target nsc_unit_tests --parallel 12`.
- Resultado de build: `nsc_unit_tests.exe` enlazado correctamente.
- Ejecución dirigida: 29 invocaciones independientes con `--case <ID>`.
- Resultado: `directed_total=29`, `directed_failed=0`.
- Catálogo unitario: 318 casos, antes 289.
- `KEEP_CPP_UNIT` sin mapear: 0, antes 29.
- Auditoría global, sin ejecutar la batería: 419 rutas únicas, 41 bloqueadas,
  223 sin mapear, 1 ambigua y 161 destinos finales correctos.

## Complejidad y preservación

El registro y la selección siguen siendo `O(n)` sobre el catálogo consolidado.
Cada caso mantiene su owner original y se ejecuta sin recursión. No se modificó
el compilador, `nscbuilder.py`, las expectativas históricas ni C:. No se ejecutó
baseline, certificación ni la batería completa.

## Estado

`TERMINADA`: los 29 IDs unitarios faltantes están registrados en su capa final,
construyen y ejecutan individualmente sin fallos.
