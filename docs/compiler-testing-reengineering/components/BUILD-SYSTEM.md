# BuildSystem — disposición y contrato reconstructivo

## 1. Estado y procedencia

- **Revisión:** `93094d49210999de5018765df1c7551e942f43f4`.
- **Universo:** 76 archivos, 11264 líneas, 97 declaraciones/artefactos.
- **Disposición:** DIRECTA=6, INDIRECTA=70.
- **Diseño futuro:** 76 contratos y 456 casos; ninguna prueba fue creada o ejecutada.

## 2. Propósito y frontera

Definir la construcción reproducible del compilador, su grafo de blancos, instalación y adaptadores de certificación histórica.

**Dependencias:** CMake;clang-cl;LLVM 23;NSLink;Python para gates históricos.

`DIRECTA` significa comportamiento propio aislable; `INDIRECTA`, comportamiento observable mediante integración o gate; `UPSTREAM`, responsabilidad del proveedor más smoke de integración; `EXCLUIDA`, fuera de la suite NSC con razón persistida.

## 3. Inventario archivo por archivo

| ID | Ruta | Cat. | Disp. | Contrato | Propósito |
|---|---|---|---|---|---|
| PF-000001 | `cmake/GenerateBuildIdentity.cmake` | `BUILD_HELPER` | `DIRECTA` | `C12B-BLD-001` | Contrato de construcción Generate Build Identity |
| PF-000002 | `cmake/NativeSharpLlvmStaticDependencies.cmake` | `BUILD_HELPER` | `DIRECTA` | `C12B-BLD-002` | Contrato de construcción Native Sharp Llvm Static Dependencies |
| PF-000003 | `cmake/NativeSharpSelfHostProfiles.cmake` | `BUILD_HELPER` | `DIRECTA` | `C12B-BLD-003` | Contrato de construcción Native Sharp Self Host Profiles |
| PF-000004 | `cmake/NativeSharpSelfHostRuntime.cmake` | `BUILD_HELPER` | `DIRECTA` | `C12B-BLD-004` | Contrato de construcción Native Sharp Self Host Runtime |
| PF-000005 | `cmake/VerifyInstalledNativeSharp.cmake` | `BUILD_HELPER` | `DIRECTA` | `C12B-BLD-005` | Contrato de construcción Verify Installed Native Sharp |
| PF-000006 | `cmake/VerifyNativeSharpAdvancedToolchain.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-006` | Adaptador histórico Verify Native Sharp Advanced Toolchain |
| PF-000007 | `cmake/VerifyNativeSharpArraysMemory.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-007` | Adaptador histórico Verify Native Sharp Arrays Memory |
| PF-000008 | `cmake/VerifyNativeSharpAssemblyAbiO0.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-008` | Adaptador histórico Verify Native Sharp Assembly Abi O0 |
| PF-000009 | `cmake/VerifyNativeSharpAssemblyDebugO0.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-009` | Adaptador histórico Verify Native Sharp Assembly Debug O0 |
| PF-000010 | `cmake/VerifyNativeSharpBackendGeneral.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-010` | Adaptador histórico Verify Native Sharp Backend General |
| PF-000011 | `cmake/VerifyNativeSharpBootstrapDependencies.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-011` | Adaptador histórico Verify Native Sharp Bootstrap Dependencies |
| PF-000012 | `cmake/VerifyNativeSharpBootstrapProfile.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-012` | Adaptador histórico Verify Native Sharp Bootstrap Profile |
| PF-000013 | `cmake/VerifyNativeSharpBootstrapSurface.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-013` | Adaptador histórico Verify Native Sharp Bootstrap Surface |
| PF-000014 | `cmake/VerifyNativeSharpCallableGraph.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-014` | Adaptador histórico Verify Native Sharp Callable Graph |
| PF-000015 | `cmake/VerifyNativeSharpCompilerHostWorkload.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-015` | Adaptador histórico Verify Native Sharp Compiler Host Workload |
| PF-000016 | `cmake/VerifyNativeSharpCrossMachineDeterminism.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-016` | Adaptador histórico Verify Native Sharp Cross Machine Determinism |
| PF-000017 | `cmake/VerifyNativeSharpDataLayout.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-017` | Adaptador histórico Verify Native Sharp Data Layout |
| PF-000018 | `cmake/VerifyNativeSharpDebugArtifacts.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-018` | Adaptador histórico Verify Native Sharp Debug Artifacts |
| PF-000019 | `cmake/VerifyNativeSharpDebugScopesT36.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-019` | Adaptador histórico Verify Native Sharp Debug Scopes T36 |
| PF-000020 | `cmake/VerifyNativeSharpDebugValuesT37.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-020` | Adaptador histórico Verify Native Sharp Debug Values T37 |
| PF-000021 | `cmake/VerifyNativeSharpDeveloperExperience.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-021` | Adaptador histórico Verify Native Sharp Developer Experience |
| PF-000022 | `cmake/VerifyNativeSharpDistributionFinal.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-022` | Adaptador histórico Verify Native Sharp Distribution Final |
| PF-000023 | `cmake/VerifyNativeSharpDriverProducts.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-023` | Adaptador histórico Verify Native Sharp Driver Products |
| PF-000024 | `cmake/VerifyNativeSharpEhRegions.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-024` | Adaptador histórico Verify Native Sharp Eh Regions |
| PF-000025 | `cmake/VerifyNativeSharpExtremeDebugO0.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-025` | Adaptador histórico Verify Native Sharp Extreme Debug O0 |
| PF-000026 | `cmake/VerifyNativeSharpFinalCertification.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-026` | Adaptador histórico Verify Native Sharp Final Certification |
| PF-000027 | `cmake/VerifyNativeSharpFirstGeneration.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-027` | Adaptador histórico Verify Native Sharp First Generation |
| PF-000028 | `cmake/VerifyNativeSharpFixedPoint.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-028` | Adaptador histórico Verify Native Sharp Fixed Point |
| PF-000029 | `cmake/VerifyNativeSharpFrontend.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-029` | Adaptador histórico Verify Native Sharp Frontend |
| PF-000030 | `cmake/VerifyNativeSharpFunctionalEquivalence.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-030` | Adaptador histórico Verify Native Sharp Functional Equivalence |
| PF-000031 | `cmake/VerifyNativeSharpGenericsAot.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-031` | Adaptador histórico Verify Native Sharp Generics Aot |
| PF-000032 | `cmake/VerifyNativeSharpImageReplayRetired.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-032` | Adaptador histórico Verify Native Sharp Image Replay Retired |
| PF-000033 | `cmake/VerifyNativeSharpInlineDebugO0.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-033` | Adaptador histórico Verify Native Sharp Inline Debug O0 |
| PF-000034 | `cmake/VerifyNativeSharpInteropBase.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-034` | Adaptador histórico Verify Native Sharp Interop Base |
| PF-000035 | `cmake/VerifyNativeSharpInteropParity.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-035` | Adaptador histórico Verify Native Sharp Interop Parity |
| PF-000036 | `cmake/VerifyNativeSharpIr.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-036` | Adaptador histórico Verify Native Sharp Ir |
| PF-000037 | `cmake/VerifyNativeSharpLlvmBackend.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-037` | Adaptador histórico Verify Native Sharp Llvm Backend |
| PF-000038 | `cmake/VerifyNativeSharpLlvmCBridge.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-038` | Adaptador histórico Verify Native Sharp Llvm CBridge |
| PF-000039 | `cmake/VerifyNativeSharpLowLevelParity.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-039` | Adaptador histórico Verify Native Sharp Low Level Parity |
| PF-000040 | `cmake/VerifyNativeSharpModuleSet.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-040` | Adaptador histórico Verify Native Sharp Module Set |
| PF-000041 | `cmake/VerifyNativeSharpNativeText.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-041` | Adaptador histórico Verify Native Sharp Native Text |
| PF-000042 | `cmake/VerifyNativeSharpNextSkeleton.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-042` | Adaptador histórico Verify Native Sharp Next Skeleton |
| PF-000043 | `cmake/VerifyNativeSharpObjectModel.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-043` | Adaptador histórico Verify Native Sharp Object Model |
| PF-000044 | `cmake/VerifyNativeSharpOwnershipCleanup.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-044` | Adaptador histórico Verify Native Sharp Ownership Cleanup |
| PF-000045 | `cmake/VerifyNativeSharpPerformance.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-045` | Adaptador histórico Verify Native Sharp Performance |
| PF-000046 | `cmake/VerifyNativeSharpProgramIr.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-046` | Adaptador histórico Verify Native Sharp Program Ir |
| PF-000047 | `cmake/VerifyNativeSharpProjectDriver.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-047` | Adaptador histórico Verify Native Sharp Project Driver |
| PF-000048 | `cmake/VerifyNativeSharpPromotionReadiness.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-048` | Adaptador histórico Verify Native Sharp Promotion Readiness |
| PF-000049 | `cmake/VerifyNativeSharpRepeatedFixedPoint.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-049` | Adaptador histórico Verify Native Sharp Repeated Fixed Point |
| PF-000050 | `cmake/VerifyNativeSharpRobustnessFinal.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-050` | Adaptador histórico Verify Native Sharp Robustness Final |
| PF-000051 | `cmake/VerifyNativeSharpRuntimeLibrary.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-051` | Adaptador histórico Verify Native Sharp Runtime Library |
| PF-000052 | `cmake/VerifyNativeSharpSelfHostClosure.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-052` | Adaptador histórico Verify Native Sharp Self Host Closure |
| PF-000053 | `cmake/VerifyNativeSharpSelfHostedGeneration.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-053` | Adaptador histórico Verify Native Sharp Self Hosted Generation |
| PF-000054 | `cmake/VerifyNativeSharpSelfHostRobustness.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-054` | Adaptador histórico Verify Native Sharp Self Host Robustness |
| PF-000055 | `cmake/VerifyNativeSharpSemantics.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-055` | Adaptador histórico Verify Native Sharp Semantics |
| PF-000056 | `cmake/VerifyNativeSharpSourceBoolVoid.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-056` | Adaptador histórico Verify Native Sharp Source Bool Void |
| PF-000057 | `cmake/VerifyNativeSharpSourceControlFlow.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-057` | Adaptador histórico Verify Native Sharp Source Control Flow |
| PF-000058 | `cmake/VerifyNativeSharpSourceDerivedBackend.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-058` | Adaptador histórico Verify Native Sharp Source Derived Backend |
| PF-000059 | `cmake/VerifyNativeSharpSourceExpressions.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-059` | Adaptador histórico Verify Native Sharp Source Expressions |
| PF-000060 | `cmake/VerifyNativeSharpSourceFloatingScalars.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-060` | Adaptador histórico Verify Native Sharp Source Floating Scalars |
| PF-000061 | `cmake/VerifyNativeSharpSourceFunctions.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-061` | Adaptador histórico Verify Native Sharp Source Functions |
| PF-000062 | `cmake/VerifyNativeSharpSourceGraph.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-062` | Adaptador histórico Verify Native Sharp Source Graph |
| PF-000063 | `cmake/VerifyNativeSharpSourceIntegerWidths.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-063` | Adaptador histórico Verify Native Sharp Source Integer Widths |
| PF-000064 | `cmake/VerifyNativeSharpSourceLoopControl.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-064` | Adaptador histórico Verify Native Sharp Source Loop Control |
| PF-000065 | `cmake/VerifyNativeSharpSourcePointers.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-065` | Adaptador histórico Verify Native Sharp Source Pointers |
| PF-000066 | `cmake/VerifyNativeSharpSpecializedCode.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-066` | Adaptador histórico Verify Native Sharp Specialized Code |
| PF-000067 | `cmake/VerifyNativeSharpStaticHostRuntime.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-067` | Adaptador histórico Verify Native Sharp Static Host Runtime |
| PF-000068 | `cmake/VerifyNativeSharpT39FrontendAbi.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-068` | Adaptador histórico Verify Native Sharp T39 Frontend Abi |
| PF-000069 | `cmake/VerifyNativeSharpT40Flow.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-069` | Adaptador histórico Verify Native Sharp T40 Flow |
| PF-000070 | `cmake/VerifyNativeSharpT41Ownership.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-070` | Adaptador histórico Verify Native Sharp T41 Ownership |
| PF-000071 | `cmake/VerifyNativeSharpT42PointersLinkage.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-071` | Adaptador histórico Verify Native Sharp T42 Pointers Linkage |
| PF-000072 | `cmake/VerifyNativeSharpT43CallablesEvents.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-072` | Adaptador histórico Verify Native Sharp T43 Callables Events |
| PF-000073 | `cmake/VerifyNativeSharpT44MetadataClosure.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-073` | Adaptador histórico Verify Native Sharp T44 Metadata Closure |
| PF-000074 | `cmake/VerifyNativeSharpWin64VarargAbi.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-074` | Adaptador histórico Verify Native Sharp Win64 Vararg Abi |
| PF-000075 | `cmake/VerifyReproducibleNativeSharpPackage.cmake` | `HISTORICAL_ADAPTER` | `INDIRECTA` | `C12B-BLD-075` | Adaptador histórico Verify Reproducible Native Sharp Package |
| PF-000076 | `CMakeLists.txt` | `BUILD_ROOT` | `DIRECTA` | `C12B-BLD-076` | Grafo raíz de construcción, instalación y registro histórico |

## 4. Contratos mantenidos

| Contrato | Nombre | Capa | Entradas | Oráculo |
|---|---|---|---|---|
| `C12B-BLD-001` | Contrato de construcción Generate Build Identity | `build-contract` | variables CMake, entorno y paths declarados por el helper | variables, targets, archivos y diagnóstico de configuración exactos |
| `C12B-BLD-002` | Contrato de construcción Native Sharp Llvm Static Dependencies | `build-contract` | variables CMake, entorno y paths declarados por el helper | variables, targets, archivos y diagnóstico de configuración exactos |
| `C12B-BLD-003` | Contrato de construcción Native Sharp Self Host Profiles | `build-contract` | variables CMake, entorno y paths declarados por el helper | variables, targets, archivos y diagnóstico de configuración exactos |
| `C12B-BLD-004` | Contrato de construcción Native Sharp Self Host Runtime | `build-contract` | variables CMake, entorno y paths declarados por el helper | variables, targets, archivos y diagnóstico de configuración exactos |
| `C12B-BLD-005` | Contrato de construcción Verify Installed Native Sharp | `build-contract` | variables CMake, entorno y paths declarados por el helper | variables, targets, archivos y diagnóstico de configuración exactos |
| `C12B-BLD-006` | Adaptador histórico Verify Native Sharp Advanced Toolchain | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-007` | Adaptador histórico Verify Native Sharp Arrays Memory | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-008` | Adaptador histórico Verify Native Sharp Assembly Abi O0 | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-009` | Adaptador histórico Verify Native Sharp Assembly Debug O0 | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-010` | Adaptador histórico Verify Native Sharp Backend General | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-011` | Adaptador histórico Verify Native Sharp Bootstrap Dependencies | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-012` | Adaptador histórico Verify Native Sharp Bootstrap Profile | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-013` | Adaptador histórico Verify Native Sharp Bootstrap Surface | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-014` | Adaptador histórico Verify Native Sharp Callable Graph | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-015` | Adaptador histórico Verify Native Sharp Compiler Host Workload | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-016` | Adaptador histórico Verify Native Sharp Cross Machine Determinism | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-017` | Adaptador histórico Verify Native Sharp Data Layout | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-018` | Adaptador histórico Verify Native Sharp Debug Artifacts | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-019` | Adaptador histórico Verify Native Sharp Debug Scopes T36 | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-020` | Adaptador histórico Verify Native Sharp Debug Values T37 | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-021` | Adaptador histórico Verify Native Sharp Developer Experience | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-022` | Adaptador histórico Verify Native Sharp Distribution Final | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-023` | Adaptador histórico Verify Native Sharp Driver Products | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-024` | Adaptador histórico Verify Native Sharp Eh Regions | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-025` | Adaptador histórico Verify Native Sharp Extreme Debug O0 | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-026` | Adaptador histórico Verify Native Sharp Final Certification | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-027` | Adaptador histórico Verify Native Sharp First Generation | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-028` | Adaptador histórico Verify Native Sharp Fixed Point | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-029` | Adaptador histórico Verify Native Sharp Frontend | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-030` | Adaptador histórico Verify Native Sharp Functional Equivalence | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-031` | Adaptador histórico Verify Native Sharp Generics Aot | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-032` | Adaptador histórico Verify Native Sharp Image Replay Retired | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-033` | Adaptador histórico Verify Native Sharp Inline Debug O0 | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-034` | Adaptador histórico Verify Native Sharp Interop Base | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-035` | Adaptador histórico Verify Native Sharp Interop Parity | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-036` | Adaptador histórico Verify Native Sharp Ir | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-037` | Adaptador histórico Verify Native Sharp Llvm Backend | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-038` | Adaptador histórico Verify Native Sharp Llvm CBridge | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-039` | Adaptador histórico Verify Native Sharp Low Level Parity | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-040` | Adaptador histórico Verify Native Sharp Module Set | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-041` | Adaptador histórico Verify Native Sharp Native Text | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-042` | Adaptador histórico Verify Native Sharp Next Skeleton | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-043` | Adaptador histórico Verify Native Sharp Object Model | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-044` | Adaptador histórico Verify Native Sharp Ownership Cleanup | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-045` | Adaptador histórico Verify Native Sharp Performance | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-046` | Adaptador histórico Verify Native Sharp Program Ir | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-047` | Adaptador histórico Verify Native Sharp Project Driver | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-048` | Adaptador histórico Verify Native Sharp Promotion Readiness | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-049` | Adaptador histórico Verify Native Sharp Repeated Fixed Point | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-050` | Adaptador histórico Verify Native Sharp Robustness Final | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-051` | Adaptador histórico Verify Native Sharp Runtime Library | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-052` | Adaptador histórico Verify Native Sharp Self Host Closure | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-053` | Adaptador histórico Verify Native Sharp Self Hosted Generation | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-054` | Adaptador histórico Verify Native Sharp Self Host Robustness | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-055` | Adaptador histórico Verify Native Sharp Semantics | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-056` | Adaptador histórico Verify Native Sharp Source Bool Void | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-057` | Adaptador histórico Verify Native Sharp Source Control Flow | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-058` | Adaptador histórico Verify Native Sharp Source Derived Backend | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-059` | Adaptador histórico Verify Native Sharp Source Expressions | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-060` | Adaptador histórico Verify Native Sharp Source Floating Scalars | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-061` | Adaptador histórico Verify Native Sharp Source Functions | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-062` | Adaptador histórico Verify Native Sharp Source Graph | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-063` | Adaptador histórico Verify Native Sharp Source Integer Widths | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-064` | Adaptador histórico Verify Native Sharp Source Loop Control | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-065` | Adaptador histórico Verify Native Sharp Source Pointers | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-066` | Adaptador histórico Verify Native Sharp Specialized Code | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-067` | Adaptador histórico Verify Native Sharp Static Host Runtime | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-068` | Adaptador histórico Verify Native Sharp T39 Frontend Abi | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-069` | Adaptador histórico Verify Native Sharp T40 Flow | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-070` | Adaptador histórico Verify Native Sharp T41 Ownership | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-071` | Adaptador histórico Verify Native Sharp T42 Pointers Linkage | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-072` | Adaptador histórico Verify Native Sharp T43 Callables Events | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-073` | Adaptador histórico Verify Native Sharp T44 Metadata Closure | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-074` | Adaptador histórico Verify Native Sharp Win64 Vararg Abi | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-075` | Adaptador histórico Verify Reproducible Native Sharp Package | `historical-weekly` | productos de etapas anteriores, contrato y herramientas explícitas | comando delegado, artefactos requeridos, resultado y reporte del gate |
| `C12B-BLD-076` | Grafo raíz de construcción, instalación y registro histórico | `build-contract` | toolchain, opciones CMake, fuentes y configuración | grafo de blancos, propiedades, lista de instalación y catálogo registrados |

## 5. Invariantes y fallos

- Ningún consumidor se diagnostica mientras falte su productor.
- Un path, hash, versión o artifact incidental sólo es oráculo cuando el contrato lo declara semántico.
- No se usa cache, fallback, producto viejo ni conteo rígido para fabricar éxito.
- La evidencia upstream no se reetiqueta como cobertura propia.

## 6. Estrategia de prueba futura

- Casos PR: 380.
- Casos NIGHTLY/weekly: 76.
- Los casos de integración usan el compilador construido y programas `.ns` mínimos; los gates self-host se reservan para frecuencia nocturna o semanal.
- Los upstream reciben smoke de construcción/enlace desde BuildSystem, no duplicación de sus suites internas.

## 7. Reconstructibilidad

La revisión conserva archivos, símbolos, contratos, errores, dependencias, oráculos y casos suficientes para implementar la suite sin redescubrir la intención.