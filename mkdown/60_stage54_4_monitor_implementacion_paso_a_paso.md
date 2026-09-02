# Stage 54.4 "Condition Variables" -- implementacion paso a paso

Ver `60_stage54_4_monitor.md` para el diseño y las decisiones; este documento es el registro
mecanico de que se toco, en que orden, y como se verifico cada paso.

## 1. `include/nativesharp/Frontend/ProjectMethodBodyModel.h`

- 5 valores nuevos de `MethodCoreOperationKind`: `MonitorEnter`, `MonitorExit`, `MonitorWait`,
  `MonitorPulse`, `MonitorPulseAll`.
- Noveno campo de `MethodCoreBinding`: `std::optional<std::size_t>
  secondaryImplicitFieldMemberOrdinal` -- usado SOLO por `MonitorWait` (guarda el ordinal de
  `lock_`, mientras el ya existente `implicitFieldMemberOrdinal` guarda el de `cv_`).

## 2. `src/Frontend/ProjectMethodBodyModel.cpp`

- **Paso preventivo, ANTES de escribir codigo nuevo**: los 16 call sites existentes de
  `MethodCoreBinding{...}` (aggregate-init posicional) se parcharon para agregar un segundo
  `std::nullopt` correspondiente al noveno campo -- via un script Python con aserciones de
  linea+contenido exactas, la misma tecnica que corrigio el bug real de Stage 54.3. Verificado con
  grep que exactamente 16 sitios (mas un falso positivo no relacionado en otra struct) fueron
  afectados.
- Bloque de reconocimiento de `NativeSharp.Core.Threading.Monitor`: busca la clase por nombre
  completamente calificado, resuelve los ordinales de los campos `lock_`/`cv_` recorriendo
  `memberModel_->members()`, y arma el `MethodCoreBinding` correspondiente para cada uno de los 5
  metodos (`Enter`/`Exit`/`Wait`/`Pulse`/`PulseAll`) segun firma exacta.
- 5 entradas nuevas en la funcion de nombre de depuracion (`"monitor-enter"`, etc.).
- Verificado: `cmake --build . --target nsc -j$(nproc)` compila limpio.

## 3. `include/nativesharp/Frontend/ProjectOopHirModel.h` / `.cpp`

- 5 valores nuevos de `ProjectOopHirNodeKind`: `CoreMonitorEnter`, `CoreMonitorExit`,
  `CoreMonitorWait`, `CoreMonitorPulse`, `CoreMonitorPulseAll`.
- Nuevo campo `std::optional<std::size_t> secondaryMemberOrdinal` en `ProjectOopHirNode` -- SEGURO
  de agregar sin tocar call sites existentes (esta struct se construye via `makeNode()` +
  asignacion de campos, no aggregate-init).
- Dos bloques nuevos en `ProjectOopHirModel.cpp`: uno para Enter/Exit/Pulse/PulseAll (baja el
  receptor una vez, arma UN nodo `FieldAddress` sobre `cv_`... salvo que en realidad usan
  `implicitFieldMemberOrdinal` de forma identica al patron de Atomics, sin necesitar el segundo
  ordinal), y uno dedicado para `MonitorWait` (baja el receptor una sola vez SIN envolverlo en
  `FieldAddress`, baja el argumento `int` de timeout, y arma el nodo `CoreMonitorWait` con
  `memberOrdinal` = ordinal de `cv_` y `secondaryMemberOrdinal` = ordinal de `lock_`).
- Verificado: rebuild completo compila limpio.

## 4. `include/nativesharp/Frontend/ProjectOopMirModel.h` / `.cpp`

- 5 valores nuevos de `ProjectOopMirNodeKind` (mirror exacto de los de HIR).
- Nuevos campos en `ProjectOopMirNode`: `secondaryMemberOrdinal` (mirror del de HIR) y
  `secondaryImmediate` (`uint64_t`, gemelo del ya existente `immediate`).
- `lowerNode()`: copia generica de `secondaryMemberOrdinal` en el preambulo; casos trivial-
  passthrough para Enter/Exit/Pulse/PulseAll; caso dedicado para `CoreMonitorWait` que resuelve
  AMBOS ordinales via `fieldLayout()` (llamado dos veces) y los guarda en
  `immediate`/`secondaryImmediate`.
- Verificado: rebuild completo compila limpio.

## 5. `src/Runtime/WindowsRuntimeBuilder.cpp`

- 5 nuevos imports Win32 declarados: `AcquireSRWLockExclusive`, `ReleaseSRWLockExclusive`,
  `WakeConditionVariable`, `WakeAllConditionVariable`, `SleepConditionVariableSRW` (BOOL/i32 de
  retorno real, no i1 -- el runtime module mantiene el ancho nativo de Win32 en el import, la
  conversion a `bool` de NativeSharp pasa en el wrapper).
- 5 funciones nuevas: `buildMonitorEnter`/`Exit`/`Pulse`/`PulseAll` (void(ptr)->void, sin
  null-check -- el receptor de Monitor ya esta garantizado no-nulo rio arriba) y `buildMonitorWait`
  (`i1(ptr cv, ptr lock, i32 ms)`, llama `SleepConditionVariableSRW` con `Flags=0` y convierte el
  BOOL de retorno a `i1` via comparacion `!= 0`).
- Verificado: `cmake --build . --target nsc_runtime -j$(nproc)` compila limpio.

## 6. `src/LLVM/ProjectOopLlvmLowerer.cpp`

- 5 constantes de nombre de funcion, declaraciones `LLVMAddFunction` para los 5 wrappers, y sus
  null-checks en el bloque de verificacion grande.
- Casos de lowering: Enter/Exit/Pulse/PulseAll bajan el unico hijo `FieldAddress` y llaman al
  wrapper correspondiente; `CoreMonitorWait` baja el receptor UNA VEZ (`checkedReceiver()`
  compartido para ambos GEPs), calcula los dos punteros (`cv`/`lock`) via GEP directo sobre el
  offset de cada campo, baja el argumento de timeout, y llama `__nativesharp_monitor_wait` con los
  3 argumentos.
- Verificado: `cmake --build . --target nsc_llvm -j$(nproc)` compila limpio, seguido de un rebuild
  completo del proyecto (`cmake --build . -j$(nproc)`) sin regresiones en ningun target existente.

## 7. `lib/NativeSharp.Core.Threading/Monitor.ns`

Archivo nuevo -- clase publica con los campos `lock_`/`cv_` (ambos `nint`, cero-inicializados por
diseño, sin constructor explicito) y los 5 metodos publicos (`Enter`/`Exit`/`Wait`/`Pulse`/
`PulseAll`) cuyos cuerpos son placeholders (`return false;`/vacios) reemplazados enteramente por
lowering nativo -- el mismo patron que `AtomicInt32`/`AtomicInt64` establecieron en Stage 54.2.

## 8. Verificacion end-to-end antes de escribir tests/ejemplo formales

Un proyecto scratch minimo (`/tmp/smoke_monitor`) referenciando `Monitor`/`Semaphore`/`Event`
compilo y corrio bajo Wine ANTES de escribir la suite formal -- confirmando que las cinco etapas
del pipeline (`--check-project-method-bodies`, `--check-project-oop-hir`, `--check-project-oop-
mir`, `--verify-project-oop-llvm`, `--emit-project-oop-exe`) pasan de punta a punta con el codigo
nuevo, y detectando temprano (antes de invertir en tests/ejemplo) el paso de sincronizacion de
`kernel32Symbols` que faltaba (ver el siguiente punto).

## 9. Sincronizacion de `kernel32Symbols` en las cinco ubicaciones (regla del proyecto)

El primer intento de `--emit-project-oop-exe` fallo en el LINKER (`lld-link: undefined symbol`)
para los 10 imports nuevos de esta ronda mas los 5 de Stage 54.5 -- exactamente el fallo esperado
por la regla del proyecto ("cada vez que el runtime module gana imports Win32 nuevos, sincronizar
`kernel32Symbols` en las CINCO ubicaciones"), confirmando que el runtime module por si solo NO es
suficiente para que un ejecutable real linkee: el generador de la tabla de imports del ejecutable
final vive en un mecanismo separado (los linker files). Corregido en las cinco ubicaciones exactas
(`src/Linker/WindowsExecutableBuilder.cpp`, `src/Linker/WindowsLibraryBuilder.cpp`,
`tests/unit/Stage47HostCAbiTests.cpp`, `tests/unit/Stage48_3CHostIntegrationTests.cpp`,
`tests/unit/Stage48_4CppHostIntegrationTests.cpp`), agregando los 10 simbolos de Monitor+
Semaphore/Event de una sola vez (38 -> 48 en las cinco), verificado con un script Python que
cuenta las entradas reales (ignorando comentarios) contra el tamaño declarado del `std::array` en
cada una de las cinco ubicaciones antes de volver a compilar.

## 10. Ejemplo empaquetado y tests dedicados

Ver `60_stage54_4_monitor.md`, seccion "Cobertura de pruebas", para el detalle de
`examples/project_stage54_4` y `tests/unit/Stage54_4MonitorTests.cpp`.

## 11. Verificacion final

`cmake --build . -j$(nproc)` limpio con `-DNSC_WARNINGS_AS_ERRORS=ON` activo (confirmado en
`CMakeCache.txt` antes de compilar). `ctest -R "stage54-4|stage54-5"`: 2/2 exitosos. `ctest`
completo: 222/223 -- la unica falla (`nsc.stage52-4.owned-cleanup-runtime`) es un test PRE-
EXISTENTE, de una etapa completamente distinta (Stage 52.4, ownership/RAII), que no fue tocado en
absoluto por esta ronda -- un "negative test" que espera un crash real bajo Wine (double-free
deliberado) y que dejo de reproducirse en este entorno especifico, confirmado reproducible de
forma consistente (3 corridas) e independiente de reiniciar `wineserver`. Documentado como hallazgo
separado, fuera de alcance de esta ronda -- ver la nota en el manifiesto de cambios.
