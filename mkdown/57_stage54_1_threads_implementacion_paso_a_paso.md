# Stage 54.1 -- implementacion paso a paso

Ver `57_stage54_1_threads.md` para el diseno completo y el razonamiento (incluida la discusion de
diseno con Alberto sobre el riesgo de tiempos de vida entre hilos). Este documento resume el
orden real en que se toco cada capa.

1. **Senal de arranque**: Alberto confirmo el cierre formal de Stage 53 ("compilo", en su
   maquina Windows real), pregunto si Stage 54 tambien se iba a partir en pedazos (confirmado,
   con una propuesta tentativa de subrondas 54.1-54.5), establecio la regla de rigor de testing
   extra para Stage 54 (registrada en `claude/05_metodo_de_trabajo_y_archivo_historico.md`), y
   dio la señal explicita de arranque: "comienza, recuerda, ultra verbosity y ultra didactico (md
   para el curso es crucial)".
2. **Fase de diseno, sin escribir codigo todavia**: se identifico el riesgo arquitectonico
   especifico de NativeSharp (sin GC, RAII determinista -- ver la seccion correspondiente en
   `57_stage54_1_threads.md`), se presento via `AskUserQuestion`, Alberto pregunto "pero que no
   se supone que el developer debe de crear algo para que eso no pase?", se le respondio con la
   diferencia estructural real y una mitigacion concreta (Thread como RAII con auto-Join() en el
   destructor, mirror de `std::jthread`), confirmada con "ok".
3. **Investigacion de patrones existentes** (sin escribir codigo): sintaxis de destructor
   (`~ClassName()`, via `List.ns`); `nint` como tipo built-in real (`"builtin:nint"`, lowerea a
   `i64`); `Stopwatch.ns` como precedente de "clase no struct, RAII, envuelve una operacion
   nucleo estatica"; sintaxis de declaracion de delegates (`Delegates.ns`); lectura profunda de
   `allocateDelegateCell`/`emitDelegateCall`/`lowerLambdaCreate`/`lowerDelegateCreate` en
   `ProjectOopLlvmLowerer.cpp` para confirmar el layout fisico exacto de un delegate (celda de 16
   bytes en heap, `code`@0/`receiver`@8) que el trampolin de runtime tendria que replicar;
   `delegateTypeOrdinalForTypeKey` para el formato real de un typeKey de delegate
   (`"type:" + identity.key()`, DONDE `key()` incluye el nombre del modulo -- NO simplemente
   `"type:<FullyQualifiedName>"`, una correccion sobre una suposicion inicial equivocada, resuelta
   resolviendo el ordinal del delegate por nombre completamente calificado en vez de asumir un
   string literal con el modulo hardcodeado).
4. **Frontend, capa por capa** (mismo patron de las 4 capas + runtime de cada ronda anterior):
   - `ProjectMethodBodyModel.h`/`.cpp`: tres `MethodCoreOperationKind` nuevos
     (`ThreadCreate`/`ThreadJoin`/`ThreadDetach`); bloque de reconocimiento nuevo para
     `NativeSharp.Core.Threading.Thread` (clase + `isStatic` + nombre + firma exactos, mismo
     patron que el bloque de `NativeSharp.Core.Environment`), con el detalle extra de resolver el
     ordinal del delegate `ThreadStart` por nombre completamente calificado para el parametro de
     `ThreadCreate` (ver el punto 3 arriba).
   - `ProjectOopHirModel.h`/`.cpp`: tres `ProjectOopHirNodeKind` nuevos
     (`CoreThreadCreate`/`CoreThreadJoin`/`CoreThreadDetach`), los tres sumados al bucket
     compartido de UN operando (`expected = 1U`) -- `ThreadCreate` es la PRIMERA core operation
     cuyo operando es un delegate en vez de un string/array/nint, pero el bloque HIR compartido
     nunca exigio un tipo de operando particular, asi que entra sin cambiar el mecanismo.
   - `ProjectOopMirModel.h`/`.cpp`: tres `ProjectOopMirNodeKind` nuevos, `toString()` y
     conversion HIR->MIR, calco letra por letra del patron de cada core operation anterior (los
     hijos se copian de forma generica despues del `switch`, sin necesitar ningun cambio extra).
5. **`ProjectOopLlvmLowerer.cpp`**: tres simbolos de runtime nuevos
   (`__nativesharp_thread_create`/`_join`/`_detach`); tres entradas en `declareRuntime()`
   (`i64(ptr)` para Create -- SIN el patron "retorno i1 dummy", resultado real y significativo,
   igual que las operaciones Count de Stage 53.6; `void(i64)` para Join/Detach); tres bloques de
   lowering, donde Join/Detach usan el mismo patron "la funcion de runtime devuelve void de
   verdad, y es el PROPIO bloque de lowering el que sintetiza su propio valor `i1` dummy en IR
   despues de la llamada" que `CoreDirectoryDelete`/`CoreFileDelete` ya establecieron (una
   correccion sobre un primer diseño que asumia, incorrectamente, que la funcion de runtime en si
   misma devolvia el `i1` dummy).
6. **`WindowsRuntimeBuilder.cpp`**: dos declaraciones Win32 nuevas (`CreateThread`,
   `WaitForSingleObject`); cuatro funciones constructoras nuevas --
   `buildThreadTrampolineFunction` (construida PRIMERO, sin simbolo de linker, ya que
   `buildThreadCreate` necesita su `LLVMValueRef` horneado directamente en el argumento
   `lpStartAddress` de `CreateThread`), `buildThreadCreate`, `buildThreadJoin`,
   `buildThreadDetach` (las ultimas dos reusan `closeHandle`/`closeHandleType`, ya declarados
   desde Stage 53.3b). Un ajuste durante la implementacion: el primer intento de
   `buildThreadTrampolineFunction` llamaba `LLVMSetLinkage(function, LLVMInternalLinkage)` para
   marcar el trampolin como no visible fuera del modulo -- el wrapper `LlvmCApi.h` de este
   proyecto (un subconjunto de la API C de LLVM mantenido a mano) no declara
   `LLVMSetLinkage`/`LLVMLinkage`, asi que se opto por dejar la funcion con linkage externo por
   defecto (inofensivo: nada la busca por nombre de todas formas, Win32 la alcanza por VALOR de
   puntero de funcion) en vez de ampliar ese wrapper para un cambio puramente cosmetico.
7. **Linker** (`WindowsExecutableBuilder.cpp` + `WindowsLibraryBuilder.cpp` + las tres copias
   locales de `kernel32Symbols` en `Stage47HostCAbiTests.cpp`/`Stage48_3CHostIntegrationTests.cpp`/
   `Stage48_4CppHostIntegrationTests.cpp`): dos simbolos nuevos (`CreateThread`,
   `WaitForSingleObject`) en `kernel32Symbols` (33->35) en las CINCO ubicaciones, sincronizadas
   desde el principio (la leccion de Stage 53.5, aplicada correctamente desde el inicio en cada
   ronda desde entonces).
8. **Build limpio**: `cmake --build build --target nsc` compilo sin errores tras completar las
   capas 4-7, con una unica correccion de compilacion real en el camino (el `LLVMSetLinkage`
   descrito en el punto 6).
9. **`lib/NativeSharp.Core.Threading/Thread.ns`** (nuevo, junto con su
   `NativeSharp.Core.Threading.nsproj`): `ThreadStart` (delegate, cero argumentos, cero retorno) y
   `Thread` (clase RAII, campos `nint handle_`/`bool joined_`, constructor que arranca el hilo de
   inmediato, `Join()`/`Detach()` idempotentes via `joined_`, destructor con auto-`Join()`, y los
   tres helpers privados estaticos con cuerpos placeholder que el compilador intercepta).
10. **`examples/project_stage54_1`** (nuevo): `Stage54_1Threads.nsproj` + `Main.ns` con 9
    chequeos numerados -- hilo static, hilo de instancia (receptor vivo en el mismo scope,
    declarado antes del Thread), hilo de lambda (captura de escalar, resultado escrito en un
    campo estatico ya que la lambda no puede "devolver" a la variable capturada), tres hilos
    concurrentes cada uno en su propio campo, la verificacion central de auto-Join() sin ningun
    Join() explicito visible, idempotencia de Join()/Detach(), Detach() como primera operacion, y
    un control negativo.
11. **`tests/unit/Stage54_1ThreadsTests.cpp`** (nuevo): fixture minima (con una correccion sobre
    el diseño inicial del test -- ver el punto 12) + el ejemplo empaquetado en O0/O2, cada nivel
    ejecutado bajo Wine **5 veces** (no 1, la regla de rigor extra de Stage 54), exigiendo exit
    code 0 y ausencia de crash en cada corrida individual.
12. **Correccion durante el testing**: el primer intento de la fixture minima verificaba que el
    texto de IR del modulo del PROGRAMA contuviera el nombre `__nativesharp_thread_trampoline` --
    fallo, porque el trampolin vive exclusivamente en el modulo del RUNTIME (un modulo LLVM
    completamente separado, nunca fusionado a nivel de texto con el modulo del programa, solo
    combinado a nivel de objeto COFF por el linker). Se corrigio quitando esa asercion incorrecta
    y documentando por que la correctitud del trampolin se verifica por ejecucion real bajo Wine,
    no por inspeccion de texto de IR del modulo equivocado.
13. **Control negativo manual**: copia temporal de `Main.ns` con
    `StaticSlots.FromStaticMethod != 999` en vez de `!= 111` -- las 10 corridas de Wine (5 por
    nivel) fallaron de inmediato con exit code 1 como se esperaba, confirmando discriminacion
    real antes de restaurar el archivo original.
14. **66/66 checks** en `nsc_stage54_1_threads_tests`, estable en multiples corridas del binario
    completo (cada corrida ya incluye las 10 ejecuciones reales bajo Wine).
15. **`CMakeLists.txt`**: registrado `nsc_stage54_1_threads_tests` / `nsc.stage54-1.threads`.
16. **Suite completa**: 218/219 -- el unico fallo (`nsc.stage52-4.owned-cleanup-runtime`) es el
    mismo hallazgo preexistente ya documentado desde Stage 53.5, no relacionado con esta ronda.
17. **Cierre de Stage 54.1**: `NativeSharp.Core.Threading.Thread` completo y verificado. Stage
    54.2 (Atomics) y el resto del desglose tentativo de Stage 54 permanecen bloqueados hasta
    nueva senal explicita de Alberto.
