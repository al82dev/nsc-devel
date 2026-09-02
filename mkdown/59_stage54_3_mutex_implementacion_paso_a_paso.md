# Stage 54.3 -- implementacion paso a paso

Ver `59_stage54_3_mutex.md` para el diseno completo y el razonamiento (incluidos los dos bugs de
concurrencia preexistentes que esta ronda encontro y corrigio). Este documento resume el orden real
en que se toco cada capa.

1. **Señal de arranque**: "continua", instruccion explicita de Alberto tras el cierre y entrega de
   Stage 54.2, siguiendo el desglose tentativo 54.1-54.5 y la disciplina de "una etapa por vez" --
   "continua" es la señal explicita para arrancar la siguiente subronda, Stage 54.3.
2. **Eleccion de diseño (mutex de kernel, no CRITICAL_SECTION)**: descartada la CRITICAL_SECTION de
   entrada por la misma razon estructural que goberno el primer pivot de Stage 54.2 -- su API
   completa exige exponer la DIRECCION de una struct inline de 40 bytes, y la tuberia OOP moderna no
   baja `&`/`*`. Un mutex de kernel usa un `HANDLE` opaco, el mismo patron `nint`
   round-tripped-entre-llamadas que Stage 54.1 (`Thread`) ya establecio -- cero mecanismo de
   compilador nuevo requerido para la caracteristica en si.
3. **Frontend, capa por capa** (mismo mecanismo de 4 capas de cada core operation anterior):
   - `ProjectMethodBodyModel.h`: 4 valores nuevos en `MethodCoreOperationKind`
     (`MutexCreate`/`MutexLock`/`MutexUnlock`/`MutexClose`).
   - `ProjectMethodBodyModel.cpp`: bloque de reconocimiento para los 4 metodos privados estaticos de
     `NativeSharp.Core.Threading.Mutex`, identico en forma al bloque de `Thread`.
   - `ProjectOopHirModel.h`/`.cpp`: 4 nodos HIR nuevos (`CoreMutexCreate`/`Lock`/`Unlock`/`Close`);
     extendido el bucket compartido de "operaciones de String" -- PRIMERA rama nueva `expected = 0U`
     (antes solo existian 1U/2U/3U), especificamente para `MutexCreate`, la primera core operation de
     todo el proyecto sin receptor ni argumentos.
   - `ProjectOopMirModel.h`/`.cpp`: 4 nodos MIR nuevos, conversion HIR->MIR trivial (mismo patron de
     Thread) -- sin logica adicional, los hijos se copian genericamente despues del switch de kind.
4. **`ProjectOopLlvmLowerer.cpp` (capa de lowering para la caracteristica)**: 4 bloques de lowering
   nuevos (`CoreMutexCreate`/`Lock`/`Unlock`/`Close`), calcando exactamente el patron de
   `ThreadCreate`/`Join`/`Detach` -- `MutexCreate` sin argumentos, llamada directa vía
   `buildNoUnwindCall` con 0 args, retorno real `i64`; `Lock`/`Unlock`/`Close` con 1 argumento
   `nint`, retorno dummy `i1` descartado.
5. **`WindowsRuntimeBuilder.cpp` (capa de runtime para la caracteristica)**: `buildMutexCreate`
   (`CreateMutexA(NULL, FALSE, NULL)`, HANDLE anonimo sin dueño, convertido a `i64` via
   `LLVMBuildPtrToInt`), `buildMutexLock` (reusa `waitForSingleObject`/`waitForSingleObjectType`
   pasados como parametros, SIN nueva declaracion), `buildMutexUnlock` (`ReleaseMutex`, retorno
   BOOL deliberadamente descartado -- asi `Unlock()` sobre un mutex nunca adquirido es un no-op
   seguro), `buildMutexClose` (reusa `closeHandle`/`closeHandleType` de Stage 53.3b). Dos imports de
   Win32 nuevos declarados en `buildInternal`: `createMutexAType`/`createMutexA`,
   `releaseMutexType`/`releaseMutex`.
6. **`lib/NativeSharp.Core.Threading/Mutex.ns`** (nuevo): clase `Mutex` con constructor/`Lock`/
   `Unlock`/destructor sobre el `HANDLE` (`nint`), comentario de cabecera documentando la eleccion
   de kernel-mutex-vs-CRITICAL_SECTION, la reentrancia, y el comportamiento de `Unlock()` sin
   adquisicion previa.
7. **`examples/project_stage54_3`** (nuevo): `Stage54_3Mutex.nsproj` + `Main.ns` con 10 chequeos
   numerados en 8 secciones -- Lock/Unlock basico, reentrancia recursiva del mismo hilo, `Unlock()`
   sobre mutex nunca adquirido, destruccion via `raii`, agotamiento modesto (200 mutex creados y
   destruidos), y las dos secciones criticas: 4 hilos reales sobre un contador NO-atomico compartido
   protegido por `Mutex.Lock()`/`Unlock()` (Seccion 6), y un SEGUNDO par mutex+contador independiente
   confirmando ausencia de interferencia entre HANDLEs (Seccion 7).
8. **Primera señal de problema**: corriendo el ejemplo empaquetado repetidamente bajo Wine, algunas
   corridas (no todas) terminaban con exit code 135 y, en el log de Wine,
   `err:virtual:virtual_setup_exception nested exception on signal stack` -- un patron intermitente,
   consistente con una condicion de carrera, no con una causa unica deterministica.
9. **Descarte disciplinado de "es Wine, otra vez"**: dado el precedente de bug de Wine confirmado en
   este proyecto (`13_bug_raii_trycatch_confirmado_bug_de_wine.md`), se aplico el mismo nivel de
   rigor en la direccion contraria. Se escribio `/tmp/mutex_ref/ref.c`, un programa de referencia en
   C puro con el mismo patron exacto (2 hilos x 2500 iteraciones, `CreateMutexA`/
   `WaitForSingleObject`/`ReleaseMutex`), compilado con el cross-compiler real
   `x86_64-w64-mingw32-gcc` (ya disponible en esta sandbox) -- NO con NativeSharp. Corrio **30/30
   veces limpio** bajo el MISMO Wine donde el equivalente NativeSharp fallaba ~50% de las veces:
   confirmacion concluyente de que el bug es de NativeSharp, no de Wine.
10. **Aislamiento sistematico**, reduciendo el programa hasta la causa exacta (`/tmp/mutex_isolate_a`
    a `_f`): (a) solo agotamiento de mutex sin hilos -- limpio; (b) 4 hilos reales -- 7-25% de
    fallo; (c) 2 hilos -- ~50% de fallo (MAS contencion sobre el mismo momento critico, con MENOS
    hilos -- descarta "volumen de llamadas" como causa); (d) un solo hilo, 20000 lock/unlock sin
    contencion -- limpio (descarta volumen de `WaitForSingleObject` por si solo); (e) removida la
    escritura al contador compartido, dejando solo Lock/Unlock -- seguia fallando (descarta el
    acceso al campo compartido); (f) volcado de IR (`--dump-project-oop-llvm`) del cctor de la clase
    con el `Mutex` estatico -- ahi aparecio la causa real.
11. **BUG #1 identificado**: `defineTypeInitializers` en `ProjectOopLlvmLowerer.cpp` usaba un global
    `i8` de estado PLANO, no atomico, con `load`-comparar-`branch`-`store` sin ninguna
    sincronizacion -- un TOCTOU clasico. Dos hilos tocando por primera vez los estaticos de la MISMA
    clase (exactamente la Seccion 6 de esta ronda, nunca ejercitado por ninguna ronda anterior)
    podian AMBOS ver `estado == 0` y AMBOS correr el inicializador -- llamando `CreateMutexA` dos
    veces, escribiendo sin proteccion sobre los mismos globales. Bug preexistente, fundacional,
    ajeno a `Mutex` conceptualmente -- afecta a cualquier programa multi-hilo con campos estaticos.
12. **Primera correccion**: reescrito `defineTypeInitializers` con `LLVMBuildAtomicCmpXchg` (ya
    disponible desde Stage 54.2) para la transicion 0->1, mas `LLVMSetOrdering` (funcion NUEVA
    agregada a `LlvmCApi.h`, verificada contra el header real `/usr/include/llvm-c-20/llvm-c/Core.h`
    de `llvm-20-dev`, lineas 4535-4536) para el camino rapido y el spin-wait. Estructura de bloques:
    `entry` (carga atomica rapida) -> `casAttempt` (CAS 0->1, `seq_cst`) -> `spinWait` (busy-spin
    sobre carga atomica) -> `initialize` (inicializadores + publicacion atomica de `estado = 2`) ->
    `done`. Busy-spin elegido deliberadamente sobre cualquier primitiva de espera del SO, para
    mantener el guard autocontenido (preocupacion de bootstrapping: no puede depender de una
    primitiva como `Mutex` que podria no estar construida todavia).
13. **Verificacion de la primera correccion**: reconstruido `nsc`; el isolamiento (c) (2 hilos x
    2500, ~50% de fallo antes) corrio **0/40 fallos** despues del fix, bajo Wine.
14. **Prueba proactiva de un edge case, ANTES de dar el fix por cerrado**: un patron nunca ejercitado
    por ninguna ronda anterior -- inicializador de campo estatico que lee otro campo estatico de SU
    PROPIO tipo (`static int A = 5; static int B = Foo.A + 1;`). Compilado y corrido bajo Wine con
    timeout de 15 segundos: **se colgo** (`timeout` devolvio exit code 124).
15. **BUG #2 identificado**: `ensureTypeInitialized` llama a `__nsc_cctor_N()` INCONDICIONALMENTE en
    cada acceso a campo estatico, sin chequeo de "¿ya estoy adentro del cctor de este tipo?".
    Evaluar el inicializador de `B` (mientras el cctor de `Foo` sigue corriendo) lee el campo `A`,
    disparando una llamada RECURSIVA al mismo `__nsc_cctor_Foo()`, desde el mismo hilo/call stack,
    con `estado == 1`. Bajo el fix de la correccion #1 (solo CAS), esa llamada recursiva pierde la
    CAS y cae en `spinWait`, esperando un `estado == 2` que solo la llamada EXTERNA (bloqueada
    esperandose a si misma) podria producir: self-deadlock infinito, introducido por la propia
    correccion del Bug #1.
16. **Diseño de la segunda correccion, primer borrador con error de dominancia SSA**: el primer plan
    calculaba `currentTid` (via `GetCurrentThreadId`) dentro del bloque `recursionCheck`, pero ese
    mismo valor SSA se referenciaba tambien en `initialize` -- que se alcanza DIRECTO desde
    `casAttempt` en el camino de "gane la CAS", sin pasar por `recursionCheck`. `recursionCheck` no
    domina `initialize`: uso invalido de SSA. Detectado por razonamiento sobre el grafo de control
    ANTES de compilar, corregido moviendo el calculo de `currentTid` a `casAttempt` (que si domina a
    ambos sucesores).
17. **Segunda correccion, implementada**: global `i32` nuevo por tipo
    (`__nsc_cctor_owner_<ordinal>`, cero-inicializado -- 0 es un centinela seguro, un TID real de
    Windows nunca es 0) mas hook de runtime interno NUEVO,
    `__nativesharp_get_current_thread_id` (pass-through trivial sobre `GetCurrentThreadId()`,
    agregado a `WindowsRuntimeBuilder.cpp` como `buildGetCurrentThreadId` -- NO es una core
    operation visible desde NativeSharp). Nuevo bloque `recursionCheck`: compara el owner guardado
    contra `currentTid`; igual -> auto-recursion, salta a `done`; distinto -> carrera genuina entre
    hilos, va a `spinWait`. El camino ganador (`initialize`) guarda `currentTid` en el owner ANTES
    de correr los inicializadores.
18. **Verificacion de la segunda correccion**: reconstruido `nsc`; `--verify-project-oop-llvm` sobre
    el programa de auto-referencia paso limpio ("139 lowered callables"); el mismo programa que se
    colgaba (exit 124) corrio limpio bajo Wine con el mismo timeout de 15s (exit 0, sin timeout).
19. **Verificacion combinada de ambas correcciones**: isolamiento (c) reconfirmado en 0/40 fallos;
    ejemplo empaquetado completo a O0 y O2, 40 corridas cada uno bajo Wine -- **80/80 limpias**.
20. **Rebuild completo del proyecto** (`cmake --build build -j$(nproc)`, con `timeout 590` -- el
    primer intento sin timeout explicito choco con el limite de 2 minutos de la herramienta Bash):
    compilo limpio, `exit=0`, cada target de test de cada ronda anterior compilo y enlazo sin error.
21. **Suite completa (`ctest`), primera corrida tras el rebuild**: 220 tests, **1 fallo NUEVO**
    (`nsc.stage37.static-initialization`) ademas del preexistente conocido
    (`nsc.stage52-4.owned-cleanup-runtime`) -- investigado de inmediato, sin descartarlo ni
    trabajarlo alrededor.
22. **Diagnostico del fallo nuevo**: el test de Stage 37 buscaba textualmente `"store i8 1, ptr
    @__nsc_cctor_state_"` en el IR -- el `store` PLANO no-atomico de la version VIEJA (insegura) del
    guard, que ya no existe (la transicion 0->1 ahora es un `cmpxchg`). Confirmado con
    `--dump-project-oop-llvm` sobre un programa minimo, mostrando el IR real generado
    (`cmpxchg ptr @__nsc_cctor_state_1, i8 0, i8 1 seq_cst seq_cst`) antes de tocar el test, para no
    adivinar la sintaxis. Conclusion: NO es una regresion de comportamiento, es una aserción que
    verificaba un detalle de implementacion que esta ronda cambio a proposito (por seguridad).
23. **Correccion del test de Stage 37**: reemplazadas las dos aserciones que buscaban
    `"store i8 1/2, ptr @__nsc_cctor_state_"` por `"cmpxchg ptr @__nsc_cctor_state_"` (el CAS nuevo)
    y `"store atomic i8 2, ptr @__nsc_cctor_state_"` (la publicacion atomica de "listo") --
    preservando exactamente la propiedad de orden original (reclamado antes de publicado), solo
    actualizando como se ve esa reclamacion en el IR. Recompilado el target de ese test solo,
    confirmado en verde de forma aislada antes de re-correr la suite completa.
24. **Suite completa, segunda corrida**: 220/221 -- unico fallo el preexistente conocido
    (`nsc.stage52-4.owned-cleanup-runtime`). Cero regresiones nuevas del rewrite del guard de cctor.
25. **`tests/unit/Stage54_3MutexTests.cpp`** (nuevo): helpers copiados del patron de Stage 54.1/54.2
    (`writeFile`/`writeProject`/`commandAvailable`/`wineCommand`/`runUnderWine`), mas un helper NUEVO
    (`writeProjectWithThreadingLibraries`) que referencia por RUTA ABSOLUTA las bibliotecas reales
    `NativeSharp.Core`/`NativeSharp.Core.Threading` (necesario para las dos fixtures de regresion,
    que exigen ejecucion real multi-hilo, no clases stub). Cinco fixtures: (1) operaciones core
    minimas -> llamadas de runtime exactas, incluido el chequeo especifico de CERO operandos para
    `MutexCreate`; (2) modulo de runtime -> confirma los imports Win32 reales
    (`CreateMutexA`/`ReleaseMutex`/`GetCurrentThreadId`) y que `WaitForSingleObject`/`CloseHandle` se
    REUSAN, verificado contra el IR real via `--dump-runtime-llvm` antes de escribir las aserciones
    exactas; (3) ejemplo empaquetado a O0/O2, 5 corridas Wine cada uno; (4) **regresion dedicada del
    Bug #1**: 8 hilos race-condition sobre el primer touch de una clase nueva
    (`RaceState`), 3000 iteraciones cada uno, 8 corridas Wine a O0 Y O2 (16 en total) -- separada del
    ejemplo empaquetado, para que una regresion futura falle con señal propia; (5) **regresion
    dedicada del Bug #2**: dos patrones de auto-referencia (lectura directa de campo, y via llamada a
    metodo estatico), acotada por el `timeout 20` ya incorporado en `runUnderWine` -- un self-deadlock
    reintroducido fallaria con un exit code claro, nunca colgando la suite.
26. **`CMakeLists.txt`**: registrado `nsc_stage54_3_mutex_tests` / `nsc.stage54-3.mutex`, mismo
    patron exacto de registro que Stage 54.1/54.2.
27. **176/176 checks** en `nsc_stage54_3_mutex_tests`, estable en tres corridas consecutivas del
    binario completo (cada corrida incluye las ejecuciones reales bajo Wine de las 5 fixtures
    descritas arriba: 10 del ejemplo empaquetado, 16 de la regresion del Bug #1, 3 de la del Bug #2).
28. **Suite completa, con el test nuevo registrado**: 221 tests, 220 pasan -- unico fallo el mismo
    preexistente de siempre (`nsc.stage52-4.owned-cleanup-runtime`); `nsc.stage54-3.mutex` en 35.56
    segundos.
29. **Cierre de Stage 54.3**: `Mutex` completo y verificado, MAS una correccion de concurrencia
    fundacional que beneficia a cualquier programa NativeSharp multi-hilo con campos estaticos.
    Stage 54.4 (variables de condicion) y el resto del desglose tentativo permanecen bloqueados
    hasta nueva señal explicita de Alberto.
