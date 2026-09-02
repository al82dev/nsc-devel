# Stage 55 "Process / Hardware / Diagnostics" -- implementacion paso a paso

Ver `62_stage55_process_hardware_diagnostics.md` para el diseño y las decisiones; este documento
es el registro mecanico. Las tres subetapas (55.1 Process, 55.2 Hardware/System Info, 55.3
Diagnostics) se implementaron EN LA MISMA PASADA por instruccion explicita de Alberto
("agregalo de una vez"), asi que cada archivo del compilador se toco UNA SOLA VEZ para las 15
core operations combinadas (6+6+3).

## 1. `include/nativesharp/Frontend/ProjectMethodBodyModel.h`

15 valores nuevos de `MethodCoreOperationKind`, insertados despues de `EventClose`:
`ProcessGetCurrentProcessIdRaw`, `ProcessGetWorkingSetSizeRaw`, `ProcessGetKernelTimeRaw`,
`ProcessGetUserTimeRaw`, `ProcessGetPriorityClassRaw`, `ProcessSetPriorityClassRaw`,
`SystemInfoGetPageSizeRaw`, `SystemInfoGetProcessorArchitectureRaw`,
`SystemInfoGetAllocationGranularityRaw`, `SystemInfoGetTotalPhysicalMemoryBytesRaw`,
`SystemInfoGetAvailablePhysicalMemoryBytesRaw`, `SystemInfoGetMemoryLoadPercentRaw`,
`DebugIsDebuggerPresentRaw`, `DebugCaptureStackFrameCountRaw`, `DebugOutputStringRaw`. NINGUN
campo nuevo en `MethodCoreBinding` -- las 9 campos existentes (reusados sin modificacion desde
Stage 54.3) alcanzan integros para las 15.

## 2. `src/Frontend/ProjectMethodBodyModel.cpp`

- 15 entradas nuevas en `methodCoreOperationKindName()`.
- Tres bloques de reconocimiento nuevos, insertados justo despues del bloque de `Event`
  (`eventKind`), mirroring exacto de `NativeSharp.Core.Environment`:
  - `NativeSharp.Core.Diagnostics.Process` (estatico): cinco getters de cero argumentos
    devolviendo `builtin:long` en un `if` combinado, mas `SetPriorityClassRaw` (un argumento
    `builtin:int`, `builtin:void`) en un `else if` separado.
  - `NativeSharp.Core.Diagnostics.SystemInfo` (estatico): seis getters de cero argumentos
    devolviendo `builtin:long`, todos bajo la misma condicion de firma (mas simple que Process
    porque las seis comparten shape exacto, sin setter).
  - `NativeSharp.Core.Diagnostics.Debug` (estatico): dos getters de cero argumentos devolviendo
    `builtin:long`, mas `OutputStringRaw` (un argumento `builtin:string`, `builtin:void`).

## 3-4. HIR / MIR

`include/nativesharp/Frontend/ProjectOopHirModel.h` / `ProjectOopMirModel.h`: 15 valores nuevos
de `ProjectOopHirNodeKind`/`ProjectOopMirNodeKind` cada uno, insertados justo antes de
`DelegateCreate`.

`src/Frontend/ProjectOopHirModel.cpp`: `ProcessSetPriorityClassRaw`/`DebugOutputStringRaw` se
agregan al bucket generico "1-o-2-operandos, sin receptor" ya existente (gate de condicion,
lista `expected=1U`, cadena de seleccion de kind) -- CERO shape nuevo, reusando la maquinaria
que Stage 51 establecio para los intrinsics de string. Los 13 getters de cero argumentos
restantes se agregan como tres bloques bespoke nuevos (Process/SystemInfo/Debug, ~25-30 lineas
cada uno), insertados justo antes del bloque `EnvironmentExit`, cada uno siguiendo el patron
exacto de `NativeClock`/`Environment` (guard `addressContext || children().size() != 0`,
`makeNode(kind, expression, "builtin:long")`).

`src/Frontend/ProjectOopMirModel.cpp`: 15 casos de nombre + 15 casos passthrough triviales
(`case ProjectOopHirNodeKind::CoreX: result->kind = ProjectOopMirNodeKind::CoreX; break;`),
insertados despues de `CoreEventClose`.

## 5. `src/LLVM/ProjectOopLlvmLowerer.cpp`

- 15 constantes `constexpr const char*` de nombre de hook (`__nativesharp_process_*`,
  `__nativesharp_system_info_*`, `__nativesharp_debug_*`).
- 30 campos miembro (`LLVMValueRef`/`LLVMTypeRef` por operacion).
- Codigo de declaracion en `declareRuntime`: funciones tipadas `i64()` para los 13 getters de
  cero argumentos, `void(i32)` para `processSetPriorityClass`, `void(ptr)` para
  `debugOutputString`.
- 15 bloques de lowering en el switch grande, usando el helper compartido `buildNoUnwindCall`;
  las dos operaciones void (`ProcessSetPriorityClassRaw`, `DebugOutputStringRaw`) devuelven el
  patron de "resultado dummy i1" (`LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)`) que
  toda operacion void de este compilador ya usa.
- Verificado: `cmake --build . --target nsc_llvm -j2` compila limpio.

## 6. `src/Runtime/WindowsRuntimeBuilder.cpp`

- 11 imports Win32 nuevos, declarados despues de `resetEvent`: `GetCurrentProcessId`,
  `GetCurrentProcess`, `K32GetProcessMemoryInfo`, `GetProcessTimes`, `GetPriorityClass`,
  `SetPriorityClass`, `GetSystemInfo`, `GlobalMemoryStatusEx`, `OutputDebugStringA`,
  `IsDebuggerPresent`, `RtlCaptureStackBackTrace`.
- 15 funciones nuevas (`buildProcessGetCurrentProcessId`, `buildProcessGetWorkingSetSize`,
  `buildProcessGetKernelTime`, `buildProcessGetUserTime`, `buildProcessGetPriorityClass`,
  `buildProcessSetPriorityClass`, `buildSystemInfoGetPageSize`,
  `buildSystemInfoGetProcessorArchitecture`, `buildSystemInfoGetAllocationGranularity`,
  `buildSystemInfoGetTotalPhysicalMemoryBytes`, `buildSystemInfoGetAvailablePhysicalMemoryBytes`,
  `buildSystemInfoGetMemoryLoadPercent`, `buildDebugIsDebuggerPresent`,
  `buildDebugCaptureStackFrameCount`, `buildDebugOutputString`), cada una verificada contra los
  layouts de struct documentados en `62_stage55_process_hardware_diagnostics.md`.
- `buildSystemInfoGetTotalPhysicalMemoryBytes`/`AvailablePhysicalMemoryBytes`/
  `MemoryLoadPercent` comparten la misma alocacion de 64 bytes para `MEMORYSTATUSEX` y la misma
  escritura de `dwLength=64` antes de llamar `GlobalMemoryStatusEx`, cada uno con su propia
  llamada (sin compartir estado entre invocaciones, mismo criterio que
  `buildProcessGetKernelTime`/`GetUserTime`).
- `buildDebugOutputString` reusa `buildNullTerminatedCopy` (el mismo helper de marshaling que
  `Environment.SetEnvironmentVariable`, Stage 53.2b, ya establecio).
- Todas las 15 agregadas al final de la cadena AND grande del driver de construccion.
- Verificado: `cmake --build . --target nsc_runtime -j2` compila limpio.

## 7. `lib/NativeSharp.Core.Diagnostics/Process.ns`, `SystemInfo.ns`, `Debug.ns`

Tres archivos nuevos -- mismo patron de `Environment.ns` (metodos publicos estaticos que
envuelven los `*Raw` privados estaticos, sin capa de instancia). `Process.KernelTime`/`UserTime`
devuelven `TimeSpan` real via `TimeSpan.FromTicks`, la unica de las tres clases con una
`using NativeSharp.Core;` (para `TimeSpan`).

## 8. Sincronizacion de `kernel32Symbols`

Los 11 simbolos de esta ronda (`GetCurrentProcessId`, `GetCurrentProcess`,
`K32GetProcessMemoryInfo`, `GetProcessTimes`, `GetPriorityClass`, `SetPriorityClass`,
`GetSystemInfo`, `GlobalMemoryStatusEx`, `OutputDebugStringA`, `IsDebuggerPresent`,
`RtlCaptureStackBackTrace`) se agregaron a las cinco ubicaciones de siempre (48 -> 59
entradas): `src/Linker/WindowsExecutableBuilder.cpp`, `src/Linker/WindowsLibraryBuilder.cpp`
(mismo formato de una entrada por linea que el anterior), `tests/unit/Stage47HostCAbiTests.cpp`,
`tests/unit/Stage48_3CHostIntegrationTests.cpp`, `tests/unit/Stage48_4CppHostIntegrationTests.cpp`
(los tres en formato compacto de una linea, igual que ya usaban). Verificado con un script
Python que cuenta las entradas reales del array (ignorando lineas de comentario) contra su
tamaño declarado, en cada una de las cinco ubicaciones -- las cinco dieron exactamente 59.

## 9. Ejemplos empaquetados y tests dedicados

Tres proyectos nuevos (`examples/project_stage55_1/Stage55_1Process.nsproj`,
`examples/project_stage55_2/Stage55_2Hardware.nsproj`,
`examples/project_stage55_3/Stage55_3Diagnostics.nsproj`), cada uno con su `Main.ns` de
verificacion numerada por codigo de salida, y tres archivos de test dedicados
(`tests/unit/Stage55_1ProcessTests.cpp`, `Stage55_2HardwareTests.cpp`,
`Stage55_3DiagnosticsTests.cpp`) siguiendo la estructura de tres niveles establecida por
`Stage53_1TimeTests.cpp` (fixture minimo de lowering, compilacion+enlazado real del ejemplo
empaquetado, ejecucion real bajo Wine).

## 10. `CMakeLists.txt`

Tres bloques nuevos (`nsc_stage55_1_process_tests`, `nsc_stage55_2_hardware_tests`,
`nsc_stage55_3_diagnostics_tests`), insertados despues del bloque de Stage 54.5 y antes de
`nsc_add_unit_test(nsc_semantic_tests ...)`, mismo patron `add_executable` +
`target_link_libraries` + `target_compile_features` + `target_compile_definitions` +
`nsc_enable_warnings` + `add_test` que todos los bloques Stage 52-54 anteriores.

## 11. Verificacion final

Reconfigurado (`cmake .` sobre el build existente, que ya tenia `NSC_WARNINGS_AS_ERRORS=ON`).
Build de los tres targets nuevos limpio (0 warnings, 0 errors). Los tres suites nuevos corridos
individualmente: `nsc_stage55_1_process_tests` (27/27), `nsc_stage55_2_hardware_tests` (27/27),
`nsc_stage55_3_diagnostics_tests` (24/24) -- las tres corridas incluyeron ejecucion REAL bajo
Wine (disponible en este entorno, no saltada), confirmando el marshaling de
`PROCESS_MEMORY_COUNTERS`/`SYSTEM_INFO`/`MEMORYSTATUSEX` contra Windows real (via Wine), no solo
IR estructuralmente valido.

Rebuild completo (`cmake --build . -j2`, sin filtro de targets) limpio, sin warnings ni errors
nuevos. `ctest` completo: **226/226 exitosos** (223 previos + los 3 nuevos de esta ronda,
`nsc.stage55-1.process`/`nsc.stage55-2.hardware`/`nsc.stage55-3.diagnostics`) -- sin
regresiones, y sin la falla pre-existente de `nsc.stage52-4.owned-cleanup-runtime` que Stage
54.4/54.5 habian reportado (no reproducida en esta corrida).
