# Stage 55 "Process / Hardware / Diagnostics" -- tres subetapas, una sola pasada

Alberto partio Stage 55 en tres subetapas (55.1 Process, 55.2 Hardware/System Info, 55.3
Diagnostics -- ver `claude/05_metodo_de_trabajo_y_archivo_historico.md`, seccion "Particion
acordada: Stage 55 en subetapas") y luego pidio implementar las tres JUNTAS en esta misma
pasada ("agregalo de una vez"), el mismo patron que ya se uso para Stage 54.4+54.5. Este
documento cubre el diseño de las tres; `62_stage55_process_hardware_diagnostics_implementacion_
paso_a_paso.md` es el registro mecanico.

## Por que las tres subetapas reusan integramente el mecanismo existente

Las 15 core operations de esta ronda (6 de Process, 6 de SystemInfo, 3 de Debug) son TODAS del
tipo mas simple que este compilador ya sabe manejar: cero argumentos devolviendo `builtin:long`
(13 de las 15), o un argumento simple (`int` o `string`) devolviendo `void` (las 2 restantes,
`Process.SetPriorityClassRaw`/`Debug.OutputStringRaw`). Ninguna necesita un receptor, ningun
HANDLE que viaje entre llamadas NativeSharp separadas (a diferencia de Thread/Mutex/Semaphore/
Event), y ningun campo nuevo en `MethodCoreBinding` -- las 9 campos existentes (reusados sin
modificacion desde Stage 54.3) alcanzan integros. El bucket generico "1-o-2-operandos, sin
receptor" que HIR ya establecio para los intrinsics de string de Stage 51 (y reuso Semaphore/
Event en 54.5) absorbe directamente `ProcessSetPriorityClassRaw`/`DebugOutputStringRaw`; los 13
getters de cero argumentos siguen el patron bespoke exacto de `NativeClock`/`Environment`
(Stage 53.1/53.2).

## Stage 55.1 "Process" -- informacion del proceso EN CURSO, nunca de otro

`NativeSharp.Core.Diagnostics.Process` expone seis operaciones sobre el proceso actual: su PID
(`CurrentProcessId`), memoria fisica en uso (`WorkingSetSize`, via `K32GetProcessMemoryInfo` --
el alias que KERNEL32.dll exporta desde Vista, evitando enlazar `psapi.lib`), tiempo de CPU en
modo kernel/usuario (`KernelTime`/`UserTime`, ambos como `TimeSpan` real -- `GetProcessTimes`
devuelve FILETIME, la MISMA unidad de 100ns que `TimeSpan.Ticks`, asi que `TimeSpan.FromTicks`
lo envuelve sin conversion), y su clase de prioridad (`PriorityClass`, get/set sobre
`GetPriorityClass`/`SetPriorityClass`, expuesta como `int` crudo de Win32 en vez de un enum
NativeSharp nuevo).

El HANDLE del proceso actual (`GetCurrentProcess()`, un pseudo-handle especial, siempre -1,
valido solo dentro del proceso que lo pide) NUNCA se expone a NativeSharp -- cada wrapper de
runtime que lo necesita lo obtiene el mismo, en el momento, con su propia llamada. No hay
`CreateProcess`/spawning de procesos hijos en esta ronda -- explicitamente fuera de alcance (ver
la particion acordada), ni `GetExitCodeProcess` (solo tiene sentido para procesos hijos, que no
existen).

## Stage 55.2 "Hardware / System Info" -- informacion de la MAQUINA, no del proceso

`NativeSharp.Core.Diagnostics.SystemInfo` expone seis lecturas de la maquina, de dos fuentes
Win32 distintas: `GetSystemInfo` (`PageSize`, `ProcessorArchitecture`,
`AllocationGranularity` -- informacion ESTATICA, no cambia mientras el proceso corre) y
`GlobalMemoryStatusEx` (`TotalPhysicalMemoryBytes`, `AvailablePhysicalMemoryBytes`,
`MemoryLoadPercent` -- una foto instantanea del estado de memoria, cambia constantemente).

Deliberadamente NO expone `ProcessorCount` -- `NativeSharp.Core.Environment.ProcessorCount()`
(Stage 53.2, via `GetActiveProcessorCount`) ya responde esa pregunta; se confirmo por grep
contra el codigo existente antes de disenar esta ronda, no se asumio. `MEMORYSTATUSEX.dwLength`
debe establecerse a 64 ANTES de llamar `GlobalMemoryStatusEx` -- Win32 lo exige para validar la
version de la estructura; el wrapper de runtime lo hace explicitamente antes de la llamada.

## Stage 55.3 "Diagnostics" -- tres utilidades basicas

`NativeSharp.Core.Diagnostics.Debug` expone `IsDebuggerPresent` (bool, normalizado desde el
`BOOL` de Win32 en el wrapper de runtime), `CaptureStackFrameCount` (int, via
`RtlCaptureStackBackTrace` -- el export real de KERNEL32.dll detras de la macro
`CaptureStackFrameCount` del SDK; devuelve `USHORT`, declarado `i16` para correccion de ABI y
zero-extendido a `i64`), y `OutputString(string)` (via `OutputDebugStringA`, marshaling
identico al que `Environment.SetEnvironmentVariable` -- Stage 53.2b -- ya establecio con
`buildNullTerminatedCopy`).

Simplificacion deliberada de `CaptureStackFrameCount`: esta ronda expone UNICAMENTE la CANTIDAD
de frames, no las direcciones reales de cada uno -- devolver las direcciones necesitaria un
mecanismo de marshaling de arreglos "owned" que este compilador todavia no tiene para este caso
(mismo tipo de decision de alcance que `Environment.GetCommandLineArgCount`, Stage 53.5).

## Layouts de struct de Win32 verificados manualmente (sin tipo NativeSharp-visible)

Ningun struct de Win32 de esta ronda tiene un tipo NativeSharp equivalente -- cada campo se
extrae por aritmetica de punteros cruda (GEP por offset de bytes) dentro del wrapper de
runtime, exactamente el mismo patron que `WIN32_FIND_DATAA` ya establecio en Stage 53.6:

- `PROCESS_MEMORY_COUNTERS` (72 bytes): `cb`@0 (i32), `PageFaultCount`@4,
  `PeakWorkingSetSize`@8, `WorkingSetSize`@16 (i64).
- `SYSTEM_INFO` (48 bytes): `wProcessorArchitecture`@0 (i16), `dwPageSize`@4 (i32),
  `dwAllocationGranularity`@40 (i32). (`dwNumberOfProcessors`@32 no se usa -- ver la nota de
  SystemInfo arriba.)
- `MEMORYSTATUSEX` (64 bytes): `dwLength`@0 (i32, DEBE escribirse a 64 antes de la llamada),
  `dwMemoryLoad`@4 (i32), `ullTotalPhys`@8 (i64), `ullAvailPhys`@16 (i64).

`GetProcessTimes` necesita CUATRO parametros de salida no omitibles (creation/exit/kernel/user
FILETIME, 8 bytes cada uno) -- `buildProcessGetKernelTime` y `buildProcessGetUserTime` alojan
los cuatro cada uno de forma independiente y descartan tres de los cuatro, en vez de compartir
estado entre ambas funciones de runtime.

## Cobertura de pruebas

Cada subetapa tiene su propio ejemplo empaquetado (`examples/project_stage55_1/_2/_3`) con
verificacion numerada por codigo de salida (mismo patron que Stage 53/54), y su propio archivo
de test dedicado (`tests/unit/Stage55_1ProcessTests.cpp`/`Stage55_2HardwareTests.cpp`/
`Stage55_3DiagnosticsTests.cpp`) con la misma disciplina de tres niveles: fixture minimo de
lowering (cada core operation emite una llamada al hook de runtime exacto), compilacion +
enlazado real del ejemplo empaquetado via el `Driver` real, y ejecucion REAL bajo Wine (no
saltada en este entorno -- Wine esta disponible aca -- confirmando el marshaling de los tres
structs de Win32 de arriba contra un sistema real, no solo contra IR estructuralmente valido).

## Limitaciones documentadas de esta ronda (deliberadas, no descuidos)

- No hay `CreateProcess`/spawning de procesos hijos (fuera de alcance, ver la particion
  acordada).
- No hay `GetExitCodeProcess` (solo tiene sentido para procesos hijos, que no existen).
- `Process.PriorityClass`/`SystemInfo.ProcessorArchitecture` exponen el entero crudo de Win32,
  no un enum NativeSharp -- un solo entero bien documentado no justifica un tipo nuevo para
  cada uno.
- `Debug.CaptureStackFrameCount` expone solo el conteo, no las direcciones de cada frame (ver
  la seccion de Stage 55.3 arriba).

## Que sigue

Con Stage 55.1/55.2/55.3 cerradas, `NativeSharp.Core.Diagnostics` cubre: `NativeClock`/
`Stopwatch` (53.1), `Process` (55.1), `SystemInfo` (55.2), `Debug` (55.3) -- la superficie de
diagnostico/hardware basica de Win32 esta cubierta para los patrones mas comunes de un motor de
juego (medir tiempo de CPU consumido, saber cuanta memoria hay disponible, detectar un
debugger adjunto). Candidatos futuros (fuera de alcance de esta ronda, ver
`06_candidatos_0_3_0.md`): `RtlCaptureStackBackTrace` con direcciones reales (necesita
marshaling de arreglos "owned"), `SetErrorMode`/manejo de crash dumps, contadores de
rendimiento (`PDH`).
