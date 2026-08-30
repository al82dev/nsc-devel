# Stage 54.4 "Condition Variables" -- Monitor, el primer core operation que necesita DOS direcciones de campo propio

## Por que Monitor existe: lo que Mutex (Stage 54.3) no puede hacer por si solo

Un mutex de kernel (`Mutex`, Stage 54.3) resuelve exclusion mutua: "solo un hilo a la vez toca
este estado compartido". Pero exclusion mutua sola no resuelve un problema distinto y muy comun:
"este hilo necesita ESPERAR hasta que una condicion se vuelva verdadera, liberando el lock
mientras espera, y volviendo a tomarlo automaticamente al despertar". Sin eso, el unico patron
disponible con solo un Mutex es **polling** -- Lock(); revisar la condicion; Unlock(); repetir en
un bucle -- que desperdicia CPU y tiene latencia proporcional al intervalo de polling.

Una condition variable resuelve exactamente eso: `Wait()` libera el lock atomicamente, duerme el
hilo (sin consumir CPU) hasta que otro hilo llame `Pulse()`/`PulseAll()`, y vuelve a tomar el lock
antes de retornar. Ningun lenguaje/runtime serio de sincronizacion (POSIX pthreads, C++
`std::condition_variable`, Java's `Object.wait/notify`, .NET's `System.Threading.Monitor`) se las
arregla sin esto -- de ahi el nombre `Monitor`, tomado directamente de la clase de .NET real que
NativeSharp busca parecerse.

## Por que SRWLOCK y no CRITICAL_SECTION

Win32 tiene DOS familias de locks livianos: `CRITICAL_SECTION` (40 bytes en x64, reentrante,
existe desde Windows NT) y `SRWLOCK` (8 bytes en x64, NO reentrante, desde Windows Vista). La
API de condition variables de Win32 tiene una mitad para cada una --
`SleepConditionVariableCS`/`SleepConditionVariableSRW` -- funcionalmente equivalentes para este
caso de uso. La eleccion aca fue puramente mecanica: **SRWLOCK cabe en el mismo ancho que un
campo `nint`** (8 bytes), el mismo mecanismo de "un solo campo primitivo por valor" que
`AtomicInt32`/`AtomicInt64` (Stage 54.2) ya establecieron. `CRITICAL_SECTION` (40 bytes) no cabe
en ningun campo que este compilador sepa representar sin introducir un tipo nuevo de "buffer
inline de tamaño fijo" -- fuera de alcance de esta ronda.

La consecuencia practica: `Enter()`/`Exit()` de `Monitor` **no son reentrantes**. Llamar `Enter()`
dos veces desde el mismo hilo sin un `Exit()` de por medio es un self-deadlock real de Win32 -- a
diferencia del mutex de kernel de Stage 54.3, que si es reentrante. Documentado en el header de
`Monitor.ns`, no verificable de forma segura en el ejemplo empaquetado (colgaria el proceso).

## Por que no hace falta un constructor que llame a "Init"

`SRWLOCK_INIT`/`CONDITION_VARIABLE_INIT` son, segun la documentacion oficial de Win32, `{0}` -- el
patron de bits todo-cero es un estado valido "desbloqueado"/"vacio" para ambos (la propia STL de
MSVC implementa `std::mutex`/`std::condition_variable` exactamente asi). Este compilador ya
cero-inicializa el storage completo de cualquier objeto nuevo antes de correr su constructor
(`ProjectObjectLayoutModel::zeroInitializesStorage`), asi que `lock_`/`cv_` llegan validos sin que
`Monitor` necesite hacer nada explicito. Simplificacion real, no un atajo -- confirmada leyendo el
codigo del compilador antes de asumirlo, no por analogia con Stage 54.2.

## El mecanismo nuevo: DOS direcciones de campo propio en una sola llamada

Stage 54.2 (Atomics) establecio como el compilador construye una direccion de campo sintetica sin
el operador `&` (que esta tuberia OOP nunca baja): un nodo `FieldAddress` sintetico, hecho a mano
por el compilador mismo, reusando la maquinaria de offset-de-campo/GEP que el acceso a campos
ordinario ya usa. Cada core operation de Atomics necesitaba UNA sola direccion asi.

`Monitor.Wait()` necesita DOS -- la de `cv_` (para saber que condition variable dormir) Y la de
`lock_` (para que `SleepConditionVariableSRW` sepa que lock liberar/reacquirir) -- **en la misma
llamada a la funcion Win32**. La solucion, deliberadamente NO implementada bajando el receptor dos
veces (para no arriesgar evaluarlo dos veces si algun dia el receptor es una expresion no trivial
como `GetMonitor().Wait(...)`):

- `MethodCoreBinding` gana un noveno campo, `secondaryImplicitFieldMemberOrdinal` (junto al ya
  existente `implicitFieldMemberOrdinal`), reservado exclusivamente para `MonitorWait`.
- El HIR lowerer (`ProjectOopHirModel.cpp`) baja el receptor **una sola vez** (no envuelto en
  `FieldAddress`, a diferencia de Enter/Exit/Pulse/PulseAll) y construye un nodo `CoreMonitorWait`
  con `memberOrdinal` = ordinal de `cv_` y un nuevo campo `secondaryMemberOrdinal` = ordinal de
  `lock_`.
- El MIR lowerer resuelve AMBOS ordinales a offsets reales (`fieldLayout()`, llamado dos veces) y
  los guarda en `immediate`/`secondaryImmediate` (un nuevo campo `uint64_t` gemelo del ya
  existente `immediate`).
- El LLVM lowerer calcula DOS GEPs desde el MISMO puntero de receptor (ya null-checkeado una sola
  vez via `checkedReceiver()`) y pasa ambos punteros + el timeout a
  `__nativesharp_monitor_wait`.

Esta extension (`secondaryMemberOrdinal`/`secondaryImmediate`) fue segura de agregar sin tocar
ningun call site existente porque `ProjectOopHirNode`/`ProjectOopMirNode` se construyen via
asignacion de campos (`makeNode()`/`lowerNode()`), no via aggregate-init posicional -- a diferencia
de `MethodCoreBinding`, que si requirio parchar sus 16 call sites existentes (ver la addenda del
manifiesto de Stage 54.3 para el bug real que ese patron causo, y la disciplina que esta ronda
aplico preventivamente).

## Timeout real: -1 significa INFINITE, cualquier valor >= 0 es real

`Wait(int millisecondsTimeout)`: `-1` se reinterpreta como Win32 `INFINITE` (`0xFFFFFFFF` de 32
bits) al cruzar la frontera; cualquier valor `>= 0` es un timeout real en milisegundos. Devuelve
`true` si fue despertado (por `Pulse()`/`PulseAll()`, o de forma ESPURIA -- Win32 lo permite
explicitamente, igual que POSIX/C++), `false` SOLO si el timeout expiro. El contrato exacto de
pthreads/C++: **siempre re-chequear la condicion en un bucle `while`**, nunca asumir que un
retorno `true` significa que la condicion especifica que se esperaba ya es verdadera.

## Cobertura de pruebas (extra rigor de Stage 54)

El ejemplo empaquetado (`examples/project_stage54_4`) cubre, en 7 secciones numeradas: exclusion
mutua real con 4 hilos sobre un contador NO-atomico (10000 exactos), un `Wait(timeout)` que
expira DE VERDAD sin que nadie lo despierte, `Pulse()`/`PulseAll()` como no-ops seguros, un
despertar cruzado-de-hilo real con un solo `Wait(-1)`/`Pulse()`, una **barrera de 6 hilos** que
prueba que `PulseAll()` despierta a TODOS los hilos bloqueados (no solo a uno -- la propiedad mas
importante que esta ronda introduce), y una cola productor/consumidor real de 4+4 hilos con
backpressure en ambos sentidos. Verificado 5 veces bajo Wine en cada uno de O0/O2 (8/8 corridas
limpias durante el desarrollo de esta ronda, sin ninguna falla intermitente observada).

`tests/unit/Stage54_4MonitorTests.cpp` agrega, ademas de la cobertura estructural (lowering a
LLVM, imports/wrappers del runtime module) y la ejecucion del ejemplo empaquetado, DOS fixtures de
regresion dedicados e independientes del ejemplo: uno que prueba especificamente que `PulseAll()`
transmite a los 8 hilos de una barrera dedicada (8 corridas, envuelto en el `timeout 20` que Stage
54.3 establecio -- si `PulseAll()` alguna vez regresara a comportarse como `Pulse()`, este fixture
falla RUIDOSAMENTE en vez de trabar la suite), y otro que prueba especificamente que
`Wait(timeout)` expira de verdad tres veces seguidas (10ms/50ms/200ms) sin que nadie lo despierte.

## Limitaciones documentadas de esta ronda (deliberadas, no descuidos)

- La no-reentrancia de `Enter()` (self-deadlock si se llama dos veces desde el mismo hilo) esta
  documentada pero NO verificada en ningun test -- hacerlo colgaria el proceso de prueba
  deliberadamente, el mismo tipo de limitacion que `Thread.ns` ya documenta para excepciones
  no capturadas en un hilo secundario.
- Cual hilo especifico despierta con `Pulse()` (cuando hay mas de uno esperando) no esta definido
  por Win32 y este compilador no lo controla -- ningun test asume un orden.
- Los despertares espurios (permitidos por Win32/POSIX/C++ por igual) no se pueden forzar de forma
  determinista bajo Wine para probarlos directamente -- el contrato "siempre re-chequear en un
  bucle" es lo que los hace inofensivos independientemente de si ocurren o no en este entorno.

## Que sigue

Stage 54.5 "Otras primitivas de sincronizacion" (Semaphore, Event) -- ver
`61_stage54_5_semaphore_event.md`. Ambas primitivas son HANDLE-based de punta a punta, sin
necesitar ningun mecanismo de compilador nuevo (a diferencia de Monitor), ya que Win32 mismo las
modela como objetos de KERNEL con un HANDLE opaco, igual que Mutex.
