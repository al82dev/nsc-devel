# Stage 54.5 "Otras primitivas de sincronizacion" -- implementacion paso a paso

Ver `61_stage54_5_semaphore_event.md` para el diseño y las decisiones; este documento es el
registro mecanico. Los primeros seis pasos se hicieron EN LA MISMA PASADA que Stage 54.4 (Alberto
pidio ambas sub-etapas juntas en esta ronda), asi que varios archivos se tocaron una sola vez para
las 14 core operations combinadas (5 de Monitor + 9 de Semaphore/Event) -- ver
`60_stage54_4_monitor_implementacion_paso_a_paso.md` para el detalle compartido de
`ProjectMethodBodyModel`/`ProjectOopHirModel`/`ProjectOopMirModel`/`ProjectOopLlvmLowerer`; este
documento se enfoca en lo especifico de Semaphore/Event.

## 1. `include/nativesharp/Frontend/ProjectMethodBodyModel.h`

9 valores nuevos de `MethodCoreOperationKind`: `SemaphoreCreate`, `SemaphoreWait`,
`SemaphoreRelease`, `SemaphoreClose`, `EventCreate`, `EventSet`, `EventReset`, `EventWait`,
`EventClose`. NINGUN campo nuevo en `MethodCoreBinding` -- a diferencia de Monitor, estas nueve
son HANDLE-based puras, mismo shape exacto que las cuatro de Mutex.

## 2. `src/Frontend/ProjectMethodBodyModel.cpp`

- Bloque de reconocimiento de `NativeSharp.Core.Threading.Semaphore` (estatico, HANDLE-based,
  mirroring exacto de Mutex): `SemaphoreCreate` (retorna `builtin:nint`, 2 parametros
  `builtin:int`), `SemaphoreWait` (retorna `builtin:bool`, parametros `nint`+`int`),
  `SemaphoreRelease`/`SemaphoreClose` (void, un parametro `nint`).
- Bloque de reconocimiento de `NativeSharp.Core.Threading.Event`: `EventCreate` (retorna
  `builtin:nint`, 2 parametros `builtin:bool`), `EventSet`/`EventReset`/`EventClose` (void, un
  parametro `nint`), `EventWait` (retorna `builtin:bool`, parametros `nint`+`int`).
- 9 entradas nuevas en la funcion de nombre de depuracion.

## 3-4. HIR / MIR

`ProjectOopHirModel.cpp`: las 9 operaciones se agregan a la lista de condiciones, logica de
`expected` (conteo de operandos) y mapeo de kind del bucket compartido de "String core operation"
ya existente -- reusando los buckets `expected=0U/1U/2U` ya existentes, sin ningun shape nuevo (a
diferencia de Monitor, que si necesito un bloque HIR propio para `MonitorWait`).
`ProjectOopMirModel.cpp`: casos passthrough triviales para las 9, sin logica especial (a diferencia
del caso dedicado que `CoreMonitorWait` si necesito).

## 5. `src/Runtime/WindowsRuntimeBuilder.cpp`

- 5 nuevos imports Win32: `CreateSemaphoreA`, `ReleaseSemaphore`, `CreateEventA`, `SetEvent`,
  `ResetEvent`. `WaitForSingleObject`/`CloseHandle` se REUSAN de Stage 54.1/53.3b, no se
  re-declaran.
- 8 funciones nuevas: `buildSemaphoreCreate` (`i64(i32,i32)`), `buildSemaphoreRelease` (`void(i64)`,
  null-checked, `ReleaseSemaphore(handle, 1, NULL)`), `buildSemaphoreClose` (`void(i64)`,
  null-checked, reusa `closeHandle`), `buildEventCreate` (`i64(i1,i1)`, zero-extiende los dos
  `bool` a `i32` para `CreateEventA`), `buildEventSet`/`buildEventReset` (`void(i64)`,
  null-checked), `buildEventClose` (`void(i64)`, null-checked, reusa `closeHandle`), y la funcion
  COMPARTIDA `buildWaitForHandleTimeout` (`i1(i64 handleAsInt, i32 ms)` -- null-handle-safe via un
  patron `alloca`+store condicional, llamada directamente por AMBOS `SemaphoreWait` y `EventWait`
  en el LLVM lowerer, no duplicada).
- Verificado: `cmake --build . --target nsc_runtime -j$(nproc)` compila limpio.

## 6. `src/LLVM/ProjectOopLlvmLowerer.cpp`

- Constantes de nombre de funcion, declaraciones, null-checks para las 5 funciones de runtime
  nuevas mas la compartida (`waitForHandleTimeoutFunction_`).
- Casos de lowering: `CoreSemaphoreCreate` (2 argumentos `i32` explicitos, resultado `i64` real);
  `CoreSemaphoreWait`/`CoreEventWait` -- **UN SOLO bloque `case A: case B:`** llamando
  `waitForHandleTimeoutFunction_` con `[handle, timeout]`, la confirmacion en codigo de que ambos
  comparten el wrapper; `CoreSemaphoreRelease/Close`, `CoreEventSet/Reset/Close` (cada uno 1
  argumento handle, llamada void con retorno dummy `i1`); `CoreEventCreate` (2 argumentos `i1`
  explicitos, resultado `i64` real).
- Verificado: `cmake --build . --target nsc_llvm -j$(nproc)` compila limpio, seguido de rebuild
  completo sin regresiones.

## 7. `lib/NativeSharp.Core.Threading/Semaphore.ns` y `Event.ns`

Archivos nuevos -- mismo patron exacto que `Mutex.ns` (helpers privados y estaticos separados de
la API publica), con constructores que llaman al `Create` correspondiente y un destructor que
llama al `Close` correspondiente.

## 8. Sincronizacion de `kernel32Symbols`

Ver `60_stage54_4_monitor_implementacion_paso_a_paso.md`, seccion 9 -- los 10 simbolos de esta
ronda (5 de Monitor + 5 de Semaphore/Event) se agregaron JUNTOS, en una sola pasada, a las cinco
ubicaciones (38 -> 48 entradas), verificados con el mismo script Python de conteo real.

## 9. Ejemplo empaquetado y tests dedicados

Ver `61_stage54_5_semaphore_event.md`, seccion "Cobertura de pruebas", para el detalle de
`examples/project_stage54_5` y `tests/unit/Stage54_5SyncPrimitivesTests.cpp`.

## 10. Verificacion final

Igual que Stage 54.4 -- ver `60_stage54_4_monitor_implementacion_paso_a_paso.md`, seccion 11. Full
rebuild con `-DNSC_WARNINGS_AS_ERRORS=ON` limpio, `ctest -R "stage54-4|stage54-5"` 2/2, ctest
completo 222/223 (unica falla pre-existente y no relacionada, documentada por separado).
