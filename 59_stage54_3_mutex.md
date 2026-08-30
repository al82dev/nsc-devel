# Stage 54.3 "Mutex" -- y dos bugs preexistentes de todo el compilador, no de esta ronda

## Por que un mutex de KERNEL, no una CRITICAL_SECTION

El desglose tentativo de Stage 54 sigue proponiendo "Mutex/CriticalSection" para esta subronda,
sin haber elegido cual de las dos todavia. La eleccion se resolvio rapido, y por la misma razon
estructural que goberno el primer obstaculo de Stage 54.2 (ver `58_stage54_2_atomics.md`): la
tuberia OOP moderna de este compilador (todas las stages desde 41, sin excepcion) no baja los
operadores `&`/`*` en absoluto -- es un limite deliberado, no una laguna.

Un `CRITICAL_SECTION` de Win32 es una estructura de 40 bytes que vive INLINE (normalmente como
campo de una clase), y su API completa (`InitializeCriticalSection`/`EnterCriticalSection`/
`LeaveCriticalSection`/`DeleteCriticalSection`) recibe la DIRECCION de esa estructura -- exactamente
el mismo `&algo` que ya bloqueo el plan original de Stage 54.2. Habria exigido, como minimo, que el
compilador supiera exponer la direccion de un campo struct-like de tamaño fijo hacia una API
publica de NativeSharp -- un mecanismo nuevo, no trivial, y con el mismo tipo de riesgo de pivot de
diseño a mitad de camino que ya paso en la ronda anterior.

Un mutex de KERNEL de Win32, en cambio, tiene una API completamente distinta: `CreateMutexA`
devuelve un `HANDLE` opaco; `WaitForSingleObject`/`ReleaseMutex`/`CloseHandle` lo reciben COMO
VALOR, nunca por direccion. Es exactamente el mismo patron que Stage 54.1 (`Thread`) ya estableció
y que este compilador ya sabe representar sin ningun mecanismo nuevo: un `HANDLE` de Win32
round-tripped como `nint` entre llamadas NativeSharp separadas (`CreateMutexA` en el constructor,
`WaitForSingleObject`/`ReleaseMutex` en `Lock()`/`Unlock()`, `CloseHandle` en el destructor). Cero
mecanismo de compilador nuevo requerido para la caracteristica en si -- toda la novedad de esta
ronda, como se documenta abajo, resultó estar en un lugar completamente distinto e inesperado.

```csharp
public class Mutex
{
    public Mutex();
    public void Lock();
    public void Unlock();
    ~Mutex();  // cierra el HANDLE -- destruccion via "raii" o al final del scope
}
```

### Reentrancia: una propiedad REAL de Win32, no una simplificacion

Un mutex de kernel de Win32 es reentrante: el mismo hilo puede llamar `Lock()` varias veces sin
bloquearse contra si mismo (Win32 mantiene un contador de recursion interno), y cada `Lock()`
necesita su propio `Unlock()` para liberar el mutex de verdad. Esto no es una decision de diseño de
NativeSharp -- es como Win32 implementa `WaitForSingleObject` sobre un HANDLE de mutex desde
siempre, y este diseño simplemente lo hereda intacto, sin ninguna capa de conteo propia encima.

### `Unlock()` sobre un mutex nunca adquirido: silenciosamente inofensivo, por diseño de Win32

`ReleaseMutex` sobre un HANDLE que el hilo llamante nunca adquirio devuelve `FALSE` (con
`GetLastError() == ERROR_NOT_OWNER`) -- esta implementacion descarta deliberadamente ese valor de
retorno, dejando que esa llamada sea un no-op seguro en vez de propagar un error. La Seccion 3 del
ejemplo empaquetado ejercita exactamente este corner case.

## Las cuatro core operations: identicas en forma a Thread, con una novedad estructural

`MutexCreate`/`MutexLock`/`MutexUnlock`/`MutexClose` se reconocen, bajan a HIR/MIR/LLVM, y llaman a
su hook de runtime exacto -- el mismo mecanismo de 4 capas que cada core operation anterior de este
proyecto. La unica novedad real: **`MutexCreate` es la PRIMERA core operation de todo el proyecto
sin ningun operando** -- ni receptor, ni argumentos de AST. Cada operacion anterior (desde
`ConsoleWriteLine` hasta `ThreadCreate`) tenia al menos un operando. El bucket compartido de
"operaciones de String" en `ProjectOopHirModel.cpp`, que agrupa las core operations por numero de
operandos esperado, solo tenia ramas para 1U/2U/3U -- esta ronda agrega la primera rama `0U`,
documentando explicitamente que el vector `operands` llega vacio y que el chequeo existente
`operands.size() != expected` ya exige 0 correctamente sin ningun cambio adicional.

Los cuatro simbolos de runtime resultantes: `__nativesharp_mutex_create` (`i64()`, SIN
parametros), `__nativesharp_mutex_lock`/`_unlock`/`_close` (`void(i64)`, patron dummy-`i1` para el
valor de retorno descartado, igual que `ThreadJoin`/`ThreadDetach`). Solo dos imports nuevos de
Win32: `CreateMutexA` y `ReleaseMutex` -- `WaitForSingleObject` se reusa integro de Stage 54.1 y
`CloseHandle` de Stage 53.3b, ninguno de los dos se vuelve a declarar.

## El descubrimiento de esta ronda: un bug de concurrencia preexistente, compilador-wide

Esto es, con diferencia, lo mas importante que produjo esta subronda -- mas que la propia
caracteristica Mutex. La regla de rigor de Stage 54 (correr cada ejemplo empaquetado VARIAS veces
bajo Wine, no una sola) hizo exactamente lo que estaba pensada para hacer: expuso un bug real que
ninguna ronda anterior habia disparado nunca.

### La señal: crashes intermitentes, no deterministicos

Corriendo el ejemplo empaquetado de esta ronda repetidamente bajo Wine (la Seccion 6 lanza 4 hilos
que tocan, todos por primera vez y en simultaneo, los campos estaticos de `SharedState` -- incluido
`SharedState.Lock1`, un `Mutex`), algunas corridas terminaban con exit code 135 y, en el log de
Wine, el diagnostico interno `err:virtual:virtual_setup_exception nested exception on signal stack`
-- una señal clasica de corrupcion de memoria/estado, apareciendo de forma intermitente (no en cada
corrida), consistente con una condicion de carrera real y no con una causa unica deterministica.

### Descartando primero la explicacion mas facil: ¿es Wine, otra vez?

Este proyecto ya tiene un precedente de bug de Wine confirmado (`13_bug_raii_trycatch_confirmado_
bug_de_wine.md`), asi que la primera hipotesis disciplinada fue exactamente esa: ¿es este otro
artefacto de la sandbox, no un bug real de NativeSharp? Se descarto con el mismo nivel de rigor que
aquel caso exigio, pero en la direccion contraria -- esta vez la conclusion fue que SI es un bug
real del compilador:

Se escribio un programa de referencia en C puro (`/tmp/mutex_ref/ref.c`), replicando exactamente el
mismo patron (`CreateMutexA`/`WaitForSingleObject`/`ReleaseMutex`, 2 hilos x 2500 iteraciones sobre
un contador compartido), compilado con el cross-compiler REAL `x86_64-w64-mingw32-gcc` (ya
disponible en esta sandbox) -- no con NativeSharp. Ese binario de referencia corrio **30/30 veces
limpio** bajo el MISMO Wine donde el equivalente compilado con NativeSharp fallaba
aproximadamente la mitad de las veces. Esto aisla el problema del lado de NativeSharp de forma
concluyente: el mismo hardware, el mismo Wine, el mismo patron de Win32 -- solo el compilador que
genero el codigo cambia entre "siempre limpio" y "falla ~50% de las veces".

### Aislamiento sistematico: reduciendo el programa hasta la causa exacta

Con el bug confirmado como real, una secuencia de programas minimos, cada uno mas chico que el
anterior, aislo la variable exacta:

1. Solo agotamiento de mutex (crear/destruir en bucle, sin hilos): limpio.
2. 4 hilos con concurrencia real: fallaba 7-25% de las corridas.
3. 2 hilos (menos hilos, MAS contencion sobre el mismo momento critico): fallaba ~50% de las
   corridas -- una tasa de fallo MAYOR con menos hilos descarta la teoria de "es simplemente
   volumen de llamadas", y apunta a un momento especifico de alta contencion.
4. Un solo hilo, 20000 lock/unlock sin contencion: limpio (descarta "volumen de llamadas a
   `WaitForSingleObject` por si solo" como causa).
5. Se removio la escritura al contador compartido, dejando solo `Lock()`/`Unlock()`: seguia
   fallando (descarta "el acceso al campo compartido" como causa -- el bug no esta en el contador).
6. Volcado directo de IR LLVM (`--dump-project-oop-llvm`) del constructor estatico ("cctor") de la
   clase que declara el `Mutex` como campo estatico: ahi aparecio la causa real.

### La causa real: el guard de inicializacion estatica ("cctor") no tenia NINGUNA sincronizacion

`ProjectOopLlvmLowerer.cpp`'s `defineTypeInitializers` -- la funcion que genera el cuerpo de
`__nsc_cctor_N()`, el constructor estatico perezoso que cada clase con campos estaticos no-const
gana automaticamente, y que CUALQUIER acceso a un campo estatico de esa clase dispara antes de leer
o escribir el campo -- usaba un global `i8` de estado PLANO, no atomico, con una secuencia ordinaria
de `load`-comparar-`branch`-`store`: si `estado == 0`, marcar `estado = 1`, correr los
inicializadores de campo en orden de declaracion, marcar `estado = 2`.

El defecto es un TOCTOU (time-of-check-to-time-of-use) clasico: dos hilos que tocan por primera vez
los miembros estaticos de la MISMA clase (exactamente lo que la Seccion 6 de esta ronda hace, y
ninguna ronda anterior habia hecho nunca -- ninguna clase estatica compartida tocada
concurrentemente desde el arranque de un grupo de hilos) pueden AMBOS observar `estado == 0` y
AMBOS ejecutar el cuerpo del inicializador -- incluyendo llamar `CreateMutexA` dos veces y escribir,
sin ninguna proteccion, sobre los mismos globales de campo/estado. Importante: esto NO es un
problema de visibilidad/orden de memoria de x86-64 (los `load`/`store` alineados individuales ya son
atomicos y ordenados en TSO x86-64) -- es una falta de EXCLUSION MUTUA sobre una seccion critica de
varias instrucciones.

**Este bug es completamente ajeno a `Mutex` conceptualmente.** Es un defecto preexistente,
fundacional, de la maquinaria de inicializacion perezosa de campos estaticos de TODO el compilador
-- probablemente presente desde que esa maquinaria existe (muy anterior a Stage 54), y capaz de
afectar a CUALQUIER programa NativeSharp multi-hilo que use campos estaticos (es decir,
practicamente todos, ahora que Stage 54.1+ hace posible el multi-threading). Simplemente ningun
ejemplo de ninguna ronda anterior habia disparado el camino exacto que lo revela: dos o mas hilos
tocando por primera vez los estaticos de la misma clase, desde el arranque.

## La primera correccion: compare-and-swap atomico real

`defineTypeInitializers` se reescribio para usar una compare-and-swap atomica real
(`LLVMBuildAtomicCmpXchg`, ya disponible desde la extension de `LlvmCApi.h` de Stage 54.2) para la
transicion 0->1, mas cargas/escrituras atomicas (`LLVMSetOrdering`, funcion NUEVA agregada a
`LlvmCApi.h` en esta ronda) para el chequeo de camino rapido y el bucle de espera. Estructura del
guard resultante:

- **`entry`**: carga atomica de `estado`; si ya es `2` (listo), salta directo a `done` sin ningun
  costo adicional -- el camino rapido para el 99.9% de los accesos posteriores al primer touch.
- **`casAttempt`**: CAS atomica `0 -> 1`, orden `seq_cst` en ambas direcciones (exito y fallo). El
  hilo que GANA la CAS (unico posible ganador, por construccion de CAS) va a `initialize`; el que
  pierde va a `recursionCheck` (ver la segunda correccion, abajo).
- **`spinWait`**: espera activa (`busy-spin`) sobre una carga atomica de `estado`, hasta que valga
  `2` -- usada solo por hilos genuinamente distintos que perdieron la carrera de la CAS.
- **`initialize`**: corre los inicializadores de campo en orden de declaracion (sin cambios en esta
  parte), despues publica `estado = 2` atomicamente.
- **`done`**: `ret void`.

Se eligio deliberadamente un busy-spin en vez de cualquier primitiva de espera del sistema
operativo, para mantener el guard AUTOCONTENIDO -- una preocupacion de "bootstrapping" real: el
guard de una clase cualquiera no puede arriesgarse a esperar sobre una primitiva como `Mutex` que
podria no haber terminado de construirse todavia (¿y si la propia clase `Mutex` tuviera un campo
estatico con inicializacion perezosa? El guard de `Mutex` no puede depender de si mismo).

Verificacion de esta primera correccion: el isolamiento #3 de arriba (2 hilos x 2500, ~50% de
fallo antes del fix) paso **0/40 corridas fallidas** despues del fix, bajo Wine.

## El segundo bug: un self-deadlock, introducido por la PROPIA correccion del primero

Antes de dar por cerrado el fix, se probo proactivamente un patron que ninguna prueba anterior de
este proyecto habia ejercitado: un inicializador de campo estatico que lee OTRO campo estatico de
SU PROPIO tipo.

```csharp
class Foo
{
    public static int A = 5;
    public static int B = Foo.A + 1;
}
```

La razon de que esto sea peligroso: `ensureTypeInitialized` (la funcion que decide cuando emitir
una llamada a `__nsc_cctor_N()`) la emite INCONDICIONALMENTE en CADA acceso a un campo estatico --
sin ningun chequeo de "¿ya estoy adentro del cctor de este mismo tipo?". Evaluar el inicializador
de `B`, mientras el cuerpo del cctor de `Foo` TODAVIA esta corriendo (para completar la
inicializacion de `B` mismo), necesita leer el campo estatico `A` -- lo cual dispara OTRA llamada a
`__nsc_cctor_Foo()`, RECURSIVAMENTE, desde el MISMO hilo y el MISMO call stack, mientras el estado
ya vale `1` ("en progreso").

Bajo el fix de la primera correccion (solo CAS, sin ningun chequeo de reentrancia), esa llamada
recursiva pierde la CAS (el estado ya no es `0`) y cae en `spinWait` -- esperando a que el estado
llegue a `2`. Pero el estado solo puede llegar a `2` cuando esta MISMA llamada externa termine, lo
cual nunca va a pasar mientras siga atascada esperandose a si misma: un **self-deadlock infinito**.

Se confirmo experimentalmente, no solo por razonamiento: compilado y corrido bajo Wine con un
timeout de 15 segundos, el programa se COLGO (`timeout` devolvio exit code 124). El codigo VIEJO
(antes de cualquier fix) no tenia este problema -- su chequeo plano no-atomico (`estado == 0`)
simplemente devolvia `false` en la llamada recursiva (el estado ya era `1`), saltando directo a
`done` sin volver a correr nada -- apoyandose implicitamente (y correctamente, por pura suerte de
orden de declaracion) en que el inicializador de `A` corre antes que el de `B` en el bucle
`mirModel_.fieldInitializers()`.

## La segunda correccion: deteccion de reentrancia por hilo dueño

Se agrego un global `i32` NUEVO por tipo (`__nsc_cctor_owner_<ordinal>`, inicializado en cero -- un
ID de hilo real de Windows nunca es 0, asi que 0 es un centinela seguro de "todavia nadie") mas un
hook de runtime interno NUEVO (`__nativesharp_get_current_thread_id`, un pass-through trivial sobre
`GetCurrentThreadId()` de Win32 -- NO es una core operation visible desde NativeSharp, solo
maquinaria interna del compilador).

Flujo del guard revisado:

- **`casAttempt`** calcula `currentTid` (via una llamada a `GetCurrentThreadId`) ANTES de intentar
  la CAS -- colocado deliberadamente aca, y no en `recursionCheck` (donde se coloco en un primer
  borrador), porque este bloque DOMINA tanto a `initialize` como a `recursionCheck` en el grafo de
  control de LLVM, un requisito de SSA valido: `initialize` se alcanza DIRECTO desde `casAttempt` en
  el camino de "gane la CAS", sin pasar por `recursionCheck` -- un valor calculado ahi no
  dominaria ese uso. Este error de dominancia SSA se detecto durante el diseño, antes de compilar,
  y se corrigio moviendo el calculo al bloque correcto.
- El camino GANADOR (`initialize`) guarda `currentTid` en el global "owner" ANTES de correr
  cualquier inicializador de campo -- para que una auto-llamada recursiva pueda verlo.
- El camino PERDEDOR pasa ahora por un bloque NUEVO, `recursionCheck`: carga el owner global (carga
  atomica), lo compara contra `currentTid`. Si son IGUALES (auto-recursion del mismo hilo) -> salta
  directo a `done` (replicando el comportamiento correcto-por-suerte del codigo viejo para este caso
  especifico). Si son DISTINTOS (carrera genuina entre hilos distintos) -> va a `spinWait`, como
  antes.

Verificacion de esta segunda correccion: el mismo programa de auto-referencia que colgaba (exit
124) termino limpio (exit 0, sin timeout) despues del fix; `--verify-project-oop-llvm` confirmo IR
valido ("139 lowered callables").

## Verificacion final, con ambas correcciones aplicadas

- El isolamiento de 2 hilos (el que fallaba ~50% de las veces antes de la primera correccion):
  **0/40 fallos** bajo Wine, despues de ambas correcciones.
- El programa de auto-referencia estatica (el que colgaba antes de la segunda correccion):
  termina limpio, exit 0, sin timeout.
- El ejemplo empaquetado completo de esta ronda (`examples/project_stage54_3`), a O0 y O2, 40
  corridas cada uno bajo Wine: **80/80 limpias**, cero crashes.
- **Suite completa del proyecto, ANTES de escribir el test dedicado de esta ronda**: 220 tests,
  1 solo fallo NUEVO (`nsc.stage37.static-initialization`) -- investigado de inmediato (ver mas
  abajo), mas el fallo preexistente ya conocido (`nsc.stage52-4.owned-cleanup-runtime`).
- **Suite completa, con el test de Stage 37 corregido**: 220/221 tests -- unico fallo el mismo
  preexistente de siempre. Con el test dedicado de esta ronda agregado y registrado: **221/221 - 1
  = 220 pasan, 1 fallo preexistente conocido**, incluyendo `nsc.stage54-3.mutex` en 35.56 segundos
  (176 checks internos).

### La unica regresion aparente resulto ser una aserción desactualizada, no un bug

El rewrite del guard de cctor rompio, de forma esperada y correcta, una aserción del test de Stage
37 (`Stage37StaticInitializationTests.cpp`) que buscaba textualmente `"store i8 1, ptr
@__nsc_cctor_state_"` en el IR emitido -- el store PLANO no-atomico de la version VIEJA (insegura)
del guard, que ya no existe: la transicion 0->1 ahora es un `cmpxchg` atomico, no un `store`. Esto
NO es una regresion de comportamiento -- es un test que verificaba un detalle de implementacion
(la forma EXACTA de la instruccion LLVM) que la correccion de esta ronda cambio deliberadamente, a
proposito, porque la version anterior era insegura. Se corrigio la aserción para buscar
`"cmpxchg ptr @__nsc_cctor_state_"` (el CAS nuevo) antes de `"store atomic i8 2, ptr
@__nsc_cctor_state_"` (la publicacion de "listo", ahora explicitamente atomica) -- preservando
exactamente la propiedad de orden que el test original queria verificar (el tipo se reclama para
inicializacion antes de publicarse como listo), solo actualizando CÓMO se ve esa reclamacion en el
IR generado. Confirmado con `--dump-project-oop-llvm` sobre un programa minimo antes de editar el
test, para no adivinar la sintaxis exacta del `cmpxchg` de LLVM.

## Limitaciones documentadas de esta ronda (deliberadas, no descuidos)

**Destruir un `Mutex` mientras otro hilo esta esperando sobre el (`WaitForSingleObject`) no esta
protegido especialmente.** Es el mismo comportamiento que Win32 documenta para `CloseHandle` sobre
un HANDLE en uso: el HANDLE se invalida, y cualquier espera en curso sobre el generalmente falla o
devuelve un resultado indefinido. Esta limitacion es identica en espiritu a la que Stage 54.1 ya
documento para `Thread` (destruir un `Thread` en ejecucion), y sigue sin resolverse por la misma
razon: resolverla bien exigiria un mecanismo de referencia-counting sobre el HANDLE que esta
subronda no se propuso construir.

**El busy-spin del guard de cctor no cede el procesador.** Bajo contencion extrema (muchos hilos
esperando la inicializacion de la misma clase al mismo tiempo) esto podria desperdiciar ciclos de
CPU en vez de ceder el scheduler -- una optimizacion futura razonable seria usar
`SwitchToThread`/`Sleep(0)` en el bucle, pero se prefirio la version mas simple posible para esta
correccion (que ya era, de por si, un cambio no planeado e invasivo) y no complicarla mas de lo
necesario para resolver el bug real.

## Que sigue

Con Stage 54.3 cerrado, NativeSharp tiene mutex de kernel verificados de extremo a extremo, Y una
correccion de concurrencia fundacional que beneficia a CUALQUIER programa NativeSharp multi-hilo
con campos estaticos -- no solo a los que usan `Mutex`. El desglose tentativo sigue siendo Stage
54.4 (variables de condicion) y 54.5 (otras primitivas si hace falta) -- una propuesta, no un
compromiso; cada subronda arranca solo con señal explicita de Alberto. Esta ronda deja ademas una
leccion concreta para el resto del proyecto: la regla de rigor de Stage 54 (correr cada ejemplo
concurrente varias veces, no una sola) no fue una formalidad -- encontro un bug real, fundacional,
que ninguna de las 53 rondas anteriores habia disparado nunca, precisamente porque ninguna habia
puesto a varios hilos a competir por el primer touch de los estaticos de una misma clase.
