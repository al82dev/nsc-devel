# Stage 54.5 "Otras primitivas de sincronizacion" -- Semaphore y Event, HANDLE-based sin mecanismo nuevo de compilador

## Que resuelven que Mutex y Monitor no

`Mutex` (Stage 54.3) es un contador binario (0 o 1) -- exclusion mutua pura. `Monitor` (Stage
54.4) agrega la capacidad de esperar una condicion arbitraria. Ninguno de los dos cubre dos casos
de uso clasicos:

- **Limitar cuantos hilos usan un recurso compartido SIMULTANEAMENTE, cuando ese limite es mayor a
  uno** (un pool de conexiones de tamaño fijo, limitar cuantos workers procesan una cola a la
  vez) -- eso es exactamente lo que `Semaphore` provee: un contador con limite superior que N
  hilos pueden decrementar (`Wait`) y cualquier hilo puede incrementar (`Release`).
- **Avisarle a quien este esperando que un trabajo esta listo, sin proteger ningun dato compartido
  -- la señal ES el dato** -- eso es `Event`: una señal binaria que uno o mas hilos pueden
  esperar, sin ningun lock ni condicion involucrada.

## Ambas son 100% HANDLE-based -- cero riesgo arquitectonico nuevo

A diferencia de Monitor (que necesito el mecanismo nuevo de "direccion de dos campos propios"),
`Semaphore` y `Event` son HANDLE-based de punta a punta -- calco directo del patron que
`Mutex`/`Thread` ya establecieron desde Stage 54.1/54.3: un `nint` guardando el HANDLE devuelto
por la funcion `Create*` de Win32 correspondiente, un HANDLE de 0 tratado como no-op seguro en
todos lados (la funcion `Create*` fallo -- caso excepcionalmente raro de agotamiento de recursos,
no verificado explicitamente por este compilador). Nada de FieldAddress, nada de ordinales
secundarios -- las nueve core operations de esta ronda (`SemaphoreCreate/Wait/Release/Close`,
`EventCreate/Set/Reset/Wait/Close`) son estructuralmente identicas a las cuatro de Mutex.

## Wait() con timeout REAL -- la primera vez que NativeSharp lo expone

Cada wait anterior en este compilador (`Thread.Join()`, `Mutex.Lock()`) es incondicional -- Win32
`INFINITE` hardcodeado, sin forma de que el codigo NativeSharp pida un timeout y observe si
expiro. `Semaphore.Wait(int)`/`Event.Wait(int)` (y tambien `Monitor.Wait(int)`, Stage 54.4) son la
primera vez que eso es posible: `-1` significa esperar indefinidamente (reinterpretado como Win32
`INFINITE` al cruzar la frontera de 32 bits), cualquier valor `>= 0` es un timeout real en
milisegundos. Devuelve `true` si se adquirio/señalo antes de que expirara, `false` si el timeout
expiro sin exito -- exactamente el tipo de corner case que la regla de rigor de Stage 54 exige
verificar con un timeout que expira DE VERDAD, no solo el camino feliz.

## Un runtime hook COMPARTIDO -- la primera vez en este proyecto

`Semaphore.Wait` y `Event.Wait` lowerean AMBOS al MISMO wrapper de LLVM,
`__nativesharp_wait_for_handle_timeout`, porque la operacion Win32 subyacente es byte-por-byte
identica para ambos: `WaitForSingleObject(handle, ms)` colapsado a `bool` via `== WAIT_OBJECT_0`
(un timeout `WAIT_TIMEOUT` o cualquier otro fallo `WAIT_FAILED` colapsan igual a `false`). Es la
primera vez que dos valores DISTINTOS de `MethodCoreOperationKind` comparten un wrapper de runtime
en la historia de este compilador -- verificado explicitamente en el test dedicado contando que el
wrapper aparece llamado desde AMBOS call sites (no solo uno).

## Semaphore: `Release()` siempre libera UN nivel

Win32's `ReleaseSemaphore` acepta un conteo variable (`lReleaseCount`). Esta ronda expone SOLO el
caso de "un nivel a la vez" (`ReleaseSemaphore(handle, 1, NULL)`) -- el caso de uso dominante (un
productor libera un slot a la vez), y un `Release(int count)` es un superset estrictamente
aditivo que puede agregarse despues sin tocar nada de lo ya construido.

## Event: UNA clase unificada, no `ManualResetEvent`/`AutoResetEvent` separadas

El C# real (`System.Threading`) expone dos clases distintas heredando de `EventWaitHandle`. Esta
ronda las unifica en `Event` con un parametro `manualReset` explicito en el constructor -- mas
cerca de como Win32 mismo lo modela (`CreateEventA` recibe `bManualReset` como un simple `BOOL`, no
dos funciones de creacion distintas), y evita duplicar las cinco core operations enteras en dos
clases casi identicas. La diferencia real de comportamiento:

- **manualReset=true**: una vez señalado via `Set()`, se queda señalado HASTA `Reset()` explicito
  -- CUALQUIER cantidad de hilos que llamen `Wait()` mientras esta señalado retornan de inmediato,
  todos a la vez. Util para "avisar una vez, que se enteren todos".
- **manualReset=false**: Win32 lo vuelve a no-señalado AUTOMATICAMENTE en el instante en que UN
  SOLO hilo esperando es liberado -- cada `Set()` libera exactamente un hilo. Util para "avisar
  una vez, que se entere solo uno". `Reset()` sobre un evento auto-reset es un no-op inofensivo
  (Win32 lo permite explicitamente).

## Cobertura de pruebas (extra rigor de Stage 54)

El ejemplo empaquetado (`examples/project_stage54_5`) cubre, en 12 secciones numeradas:
`Semaphore(1,1)` basico, un `Wait(timeout)` que expira DE VERDAD, un bucle de agotamiento modesto
(200 semaforos creados/destruidos), **10 hilos compitiendo por `Semaphore(3,3)`** con un limite de
concurrencia real verificado con un contador atomico "en vuelo" (`AtomicInt32`, Stage 54.2) y un
contador de violaciones que debe terminar en 0, `Event` manual-reset transmitiendo a 5 hilos sin
coordinacion previa, `Event` auto-reset consumido por exactamente un hilo con re-armado automatico
verificado por un segundo `Wait(timeout)` que debe expirar, y un `Reset()` explicito. Verificado 5
veces bajo Wine en cada uno de O0/O2 (8/8 corridas limpias durante el desarrollo).

`tests/unit/Stage54_5SyncPrimitivesTests.cpp` agrega, ademas de la cobertura estructural (incluido
un conteo explicito de que el wrapper compartido aparece exactamente 2 veces en el fixture minimo)
y la ejecucion del ejemplo empaquetado, un fixture de regresion dedicado con MAS contencion que el
ejemplo (16 hilos sobre `Semaphore(4,4)`, vs. 10 sobre `Semaphore(3,3)`) especificamente para
maximizar la probabilidad de detectar una regresion en el limite de concurrencia real.

## Limitaciones documentadas de esta ronda (deliberadas, no descuidos)

- No se expone `Semaphore.Release(int count)` -- ver la seccion correspondiente arriba.
- El limite de concurrencia de `Semaphore` se verifica con un contador atomico "en vuelo" mas un
  busy-loop deliberado (sin `Sleep()`) para ensanchar la ventana de una violacion hipotetica -- el
  mismo espiritu de deteccion-de-carrera que Mutex/Monitor ya usan, no una prueba matematicamente
  exhaustiva de que la propiedad se cumple en TODO orden de scheduling posible (imposible de
  garantizar en cualquier entorno real).
- No hay forma de forzar deterministicamente, bajo Wine, que un evento auto-reset con MULTIPLES
  esperando simultaneamente libere a un hilo especifico -- Win32 no lo define, y por eso el test
  de auto-reset usa un solo hilo esperando (suficiente para probar "exactamente uno, y se
  resetea", sin necesitar asumir cual).

## Que sigue

Con Stage 54.4 y 54.5 cerradas, el modulo `NativeSharp.Core.Threading` cubre: `Thread` (54.1),
`AtomicInt32`/`AtomicInt64` (54.2), `Mutex` (54.3), `Monitor` (54.4), `Semaphore`/`Event` (54.5) --
la superficie de sincronizacion de bajo nivel que Win32 expone via KERNEL32 esta esencialmente
completa para los patrones de concurrencia mas comunes. Candidatos futuros (fuera de alcance de
esta ronda, ver `06_candidatos_0_3_0.md`): thread pools, `Interlocked` de 64 bits con mas
operaciones, primitivas de sincronizacion de proceso-cruzado (named mutex/semaphore/event).
