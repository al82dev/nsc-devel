# Stage 54.1 "Threads" -- hilos de sistema operativo con RAII y auto-Join()

## Por que esta es la etapa mas peligrosa de todo el proyecto hasta ahora

Cada etapa anterior de NativeSharp, desde Stage 1 hasta el cierre de Stage 53, comparte una
propiedad tranquilizadora: un programa NativeSharp de UN SOLO HILO se ejecuta de forma
completamente determinista. Dos corridas del mismo binario, con la misma entrada, hacen
exactamente las mismas operaciones en exactamente el mismo orden. Los bugs que este compilador
tuvo que perseguir hasta ahora (el bug RAII/try-catch de Stage 52.4-cierre, por ejemplo) eran
dificiles de encontrar, pero una vez encontrados eran reproducibles: correr el mismo binario diez
veces daba el mismo resultado diez veces.

Stage 54 rompe esa propiedad a proposito. Un programa con MAS DE UN HILO ya no tiene un unico
orden posible de ejecucion -- tiene tantos ordenes posibles como formas de intercalar las
instrucciones de cada hilo pueda haber, y CUAL de esos ordenes ocurre en una corrida particular
depende del scheduler del sistema operativo, no del programa. Un bug de concurrencia puede
esconderse detras de miles de corridas exitosas y aparecer recien en la corrida 1001, en la
maquina de otra persona, con otra carga de CPU. Por eso Alberto establecio, antes de que se
escribiera una sola linea de codigo de esta ronda, una regla de rigor de testing especifica para
Stage 54 (ver `claude/05_metodo_de_trabajo_y_archivo_historico.md`, seccion "Regla de rigor de
testing: Stage 54"): no recortar la matriz de tests, correr los tests de concurrencia varias
veces (no una), verificar resultados EXACTOS en los ejemplos empaquetados (no solo "no crashea"),
nunca usar `Sleep()` como sustituto de sincronizacion real, y documentar explicitamente que no se
puede verificar deterministicamente en este sandbox Linux/Wine. Esta ronda sigue esa regla de
punta a punta -- la seccion "Verificacion" mas abajo detalla exactamente como.

Hay una segunda razon, mas especifica de NativeSharp, por la que threading es peligroso aca de
una forma en la que NO lo es en C#: **NativeSharp no tiene garbage collector**. Toda la gestion
de memoria de este lenguaje es RAII/ownership determinista (ver
`claude/04_restricciones_arquitectonicas.md`) -- un objeto `owned`/RAII se destruye en el
INSTANTE exacto en que su scope termina, sin importar si alguien mas todavia lo esta usando. En
C# real, si un hilo en segundo plano todavia tiene una referencia a un objeto, el GC lo mantiene
vivo pase lo que pase en el hilo principal -- el "objeto compartido entre hilos que uno destruye
mientras el otro lo usa" simplemente no puede pasar, el GC lo previene por construccion. En
NativeSharp, si el hilo principal termina el scope de un objeto que un hilo en segundo plano
todavia esta usando, ese objeto se destruye ahi mismo -- el hilo en segundo plano queda con una
referencia a memoria ya liberada. Esto se discutio explicitamente con Alberto (ver la seccion
"La discusion de diseño con Alberto" mas abajo) antes de escribir una sola linea de codigo.

## Que se agrego

Un namespace nuevo, `NativeSharp.Core.Threading`, con dos tipos:

```csharp
public delegate void ThreadStart();

public class Thread
{
    public Thread(ThreadStart start);   // arranca el hilo DE INMEDIATO -- no hay Start() separado
    public void Join();                 // bloquea hasta que el hilo termine, despues libera el HANDLE
    public void Detach();                // libera el HANDLE sin esperar -- el hilo sigue corriendo solo
    ~Thread();                          // auto-Join() si Join()/Detach() todavia no corrieron
}
```

Uso tipico:

```csharp
ThreadStart worker = SomeClass.SomeStaticMethod;
raii Thread t = new Thread(worker);
// ... el hilo principal puede seguir haciendo otra cosa mientras "t" corre ...
t.Join(); // espera a que termine antes de seguir
```

O, dejando que RAII haga el Join() automaticamente:

```csharp
{
    raii Thread t = new Thread(worker);
    // sin Join() -- el destructor de "t", al final de este bloque, lo hace por nosotros
}
// para cuando llegamos aca, "t" YA TERMINO de correr
```

## La discusion de diseño con Alberto

El punto de partida fue una pregunta de seguridad real: ¿que tipo de delegate deberia poder ser
el `ThreadStart` de un `Thread`? Se presentaron dos opciones via `AskUserQuestion`:

- **Opcion A**: restringir `Thread` a delegates static, sin captura -- la opcion mas "segura" en
  el sentido de que el compilador rechaza de entrada cualquier delegate que pudiera referenciar
  un objeto del hilo principal.
- **Opcion B**: reusar la maquinaria de delegates completa que existe desde Stage 42/43 (static,
  de instancia, lambda), documentando el riesgo de tiempos de vida entre hilos como una
  limitacion conocida.

Alberto respondio con una pregunta, no con una eleccion: **"pero que no se supone que el
developer debe de crear algo para que eso no pase?"** -- señalando, correctamente, que en un
lenguaje de sistemas sin garbage collector, la responsabilidad del developer sobre tiempos de
vida entre hilos es lo normal y esperado (exactamente la misma responsabilidad que tiene
cualquier programa C++ que usa `std::thread`).

La respuesta no fue simplemente "tenes razon, sin restriccion" -- fue explicar la diferencia
ESTRUCTURAL real frente a C# (la ausencia de GC descrita arriba) y proponer una mitigacion
concreta, de costo casi cero, reusando maquinaria que el compilador YA tiene desde Stage 49: que
`Thread` sea el mismo una clase RAII, cuyo **destructor haga auto-`Join()`** si el hilo todavia
no fue unido -- exactamente el mismo diseño que C++20 introdujo con `std::jthread` (a diferencia
de `std::thread` de C++11, cuyo destructor llama `std::terminate()` si el hilo sigue "joinable";
NativeSharp eligio el comportamiento MAS seguro de los dos: auto-join silencioso, nunca terminar
el proceso).

Por que esto ayuda de verdad, no es solo cosmetico: RAII en NativeSharp YA GARANTIZA, desde Stage
49, que los objetos de un mismo scope se destruyen en orden INVERSO al de su declaracion. Si el
codigo NativeSharp declara los objetos que un hilo va a usar PRIMERO, y el `Thread` que los usa
DESPUES, en el mismo scope, entonces el fin de ese scope destruye PRIMERO el `Thread` (bloqueando
en su auto-`Join()` hasta que el hilo en segundo plano realmente termine) y SOLO DESPUES destruye
los objetos que ese hilo pudo haber estado usando -- el hilo en segundo plano ya no existe para
cuando esos objetos se liberan. Esto cierra "gratis" la version MAS COMUN de este error (uso
estrictamente anidado de un `Thread` dentro de un scope), sin agregar ninguna restriccion nueva
al compilador ni al lenguaje. Alberto confirmo esta propuesta con **"ok"**.

Lo que esta mitigacion NO resuelve (documentado explicitamente, no un descuido): un `Thread`
guardado en un campo de larga vida, pasado a otra funcion que lo guarda en una estructura de
datos global, o cuyo scope de declaracion no respeta el orden "objetos primero, Thread despues"
-- esos casos siguen dependiendo pura y exclusivamente de la disciplina del developer, sin ninguna
red de seguridad, exactamente igual que en C++. Y el analisis de escape de lambdas de Stage 50.7
(`collectLambdaCaptures`), aunque SI previene una categoria entera de errores (una lambda no
puede capturar un owner RAII, un local prestado, un parametro prestado ni un parametro owned),
sigue sin poder detectar el caso de un delegate ligado a un metodo de INSTANCIA cuyo receptor es
destruido por el hilo principal mientras el hilo en segundo plano todavia lo usa -- ese analisis
es un sistema de patrones sintacticos puntuales, no un analisis interprocedural real de tiempos
de vida.

## Representacion fisica: como el trampolin de runtime reusa el layout de delegate que ya existe

Cada delegate en NativeSharp, desde Stage 42/43, es un valor de 16 bytes alocado en el heap (una
"celda"), con este layout fijo (`allocateDelegateCell` en `ProjectOopLlvmLowerer.cpp`):

```
offset 0:  ptr code       -- la funcion real a llamar
offset 8:  ptr receiver   -- null para un delegate "static"; el "this" de una instancia,
                             o el ambiente de closure de una lambda, para un delegate
                             "de instancia/closure" (indistinguibles a este nivel)
```

Cada invocacion de delegate NativeSharp (`emitDelegateCall`, tambien en `ProjectOopLlvmLowerer.cpp`)
hace exactamente dos loads (`code`, `receiver`) y despues una rama: si `receiver != null`, llama
`code(receiver, ...args)`; si no, llama `code(...args)`. Esa es la UNICA logica de despacho de
delegates que existe en todo este compilador -- Stage 54.1 no inventa una segunda.

El problema que Stage 54.1 tuvo que resolver: `CreateThread` de Win32 no sabe nada de delegates
NativeSharp. Su firma es `HANDLE CreateThread(..., LPTHREAD_START_ROUTINE lpStartAddress, LPVOID
lpParameter, ...)` -- espera una funcion de la forma `DWORD WINAPI ThreadProc(LPVOID)`, no la
forma "code/receiver" de un delegate NativeSharp. La solucion: un **trampolin** interno,
`__nativesharp_thread_trampoline` (construido en `WindowsRuntimeBuilder.cpp`, funcion
`buildThreadTrampolineFunction`), con la firma exacta que Win32 exige (`i32(ptr)`), cuyo CUERPO
replica letra por letra la misma logica de `emitDelegateCall`: recibe la celda del delegate como
su unico parametro (`lpParameter`), hace los mismos dos loads, hace la misma rama
static-vs-instancia, y siempre devuelve `0` (Win32 exige un `DWORD` de retorno; `ThreadStart` es
`void`, asi que no hay ningun valor real que reportar).

Este trampolin es **interno**: nunca se exporta, nunca se importa, nunca es invocable desde
codigo NativeSharp. `CreateThread` lo recibe como un PUNTERO A FUNCION (`lpStartAddress`), horneado
directamente en el IR de LLVM por `buildThreadCreate` en el momento de construir la llamada --
Win32 lo alcanza por VALOR de puntero, no por busqueda de simbolo por nombre. Por eso ninguna de
las cinco ubicaciones de `kernel32Symbols` necesita una entrada para el (esas listas son
exclusivamente para funciones Win32 que este runtime IMPORTA, y el trampolin no es una de esas
-- ver la seccion siguiente para los dos simbolos que SI se agregaron).

## Las tres core operations y el runtime C++

`NativeSharp.Core.Threading.Thread` define tres helpers PRIVADOS y ESTATICOS -- a diferencia de
CADA core operation anterior de este proyecto (`Console.WriteLine`, `NativeClock.*`,
`Environment.*`, `File.*`, `Directory.*`, `Path.GetFullPath`, todas `public`, pensadas para que
codigo de usuario las llame directamente), estas tres son privadas: solo el propio constructor/
`Join()`/`Detach()` de `Thread` las llama. El mecanismo de reconocimiento del compilador
(`MethodCoreOperationKind`, ver `ProjectMethodBodyModel.cpp`) no distingue accesibilidad -- solo
clase + nombre + firma exactos -- asi que una operacion nucleo privada funciona identico a una
publica.

```csharp
private static nint ThreadCreate(ThreadStart start);  // -> __nativesharp_thread_create
private static void ThreadJoin(nint handle);            // -> __nativesharp_thread_join
private static void ThreadDetach(nint handle);           // -> __nativesharp_thread_detach
```

El HANDLE de Win32 se guarda como `nint` -- el tipo entero NativeSharp del mismo ancho que un
puntero (64 bits en este target, lowerea a `i64` de LLVM llano). Esta es la PRIMERA core
operation de todo el proyecto cuyo handle de recurso debe sobrevivir ENTRE llamadas NativeSharp
separadas: cada wrapper de Win32 anterior (`File.ReadAllText`, `Directory.GetFiles`, etc.) abria
y cerraba su handle dentro de UNA sola llamada al runtime. `ThreadCreate` produce el HANDLE (en
el constructor), y una llamada POSTERIOR y separada a `ThreadJoin`/`ThreadDetach` (en
`Join()`/`Detach()`/`~Thread()`) lo consume.

Del lado de `WindowsRuntimeBuilder.cpp`, cuatro funciones nuevas:

- **`buildThreadTrampolineFunction`**: descrita arriba.
- **`buildThreadCreate`** (`i64(ptr)`): llama `CreateThread(NULL, 0, trampolin, delegateCell, 0,
  NULL)` y devuelve el HANDLE resultante convertido a `i64` via `LLVMBuildPtrToInt` DIRECTAMENTE,
  sin ninguna comparacion extra. Esto es distinto de `CreateFileA`/`FindFirstFileA` (cuyo
  sentinela de fallo es `INVALID_HANDLE_VALUE`, todos unos) -- Win32 define el fallo de
  `CreateThread` como `NULL` (todos ceros), asi que un `CreateThread` fallido ya se convierte,
  sin ningun trabajo extra, en exactamente el `nint` `0` que `ThreadJoin`/`ThreadDetach` tratan
  como "nunca fue un handle real".
- **`buildThreadJoin`** (`void(i64)`): si el handle es `0`, no-op silencioso. Si no, llama
  `WaitForSingleObject(handle, INFINITE)` y DESPUES `CloseHandle(handle)` -- ese orden importa:
  cerrar el handle antes de que el hilo termine dejaria ese numero de HANDLE libre para que una
  llamada FUTURA no relacionada (`CreateThread`, `CreateFileA`, cualquier otra) lo reutilice
  mientras el hilo original todavia podria estar corriendo.
- **`buildThreadDetach`** (`void(i64)`): mismo guard de handle nulo; si no, solo
  `CloseHandle(handle)`, SIN `WaitForSingleObject`. Esto es un hecho real de Win32, no una
  simplificacion de este compilador: Windows no tiene una syscall separada tipo "detach" de
  pthreads. Un hilo sigue corriendo despues de que su HANDLE se cierra -- el handle solo nombra
  el objeto del kernel que lleva la contabilidad de ese hilo (su estado, quien lo esta
  esperando), no es lo que mantiene su codigo ejecutandose.

Dos simbolos Win32 nuevos, agregados a `kernel32Symbols` en las CINCO ubicaciones que deben
sincronizarse cada vez que el runtime gana un import nuevo (leccion de Stage 53.5, aplicada desde
el principio en cada ronda desde entonces): `CreateThread` y `WaitForSingleObject`. `CloseHandle`
ya estaba declarado desde Stage 53.3b y se reusa directamente.

## Limitaciones documentadas de esta ronda (deliberadas, no descuidos)

**Una excepcion sin capturar que escapa un `ThreadStart` termina el proceso completo.** El
trampolin llama al delegate con una `LLVMBuildCall2` PLANA -- no la maquinaria invoke/funclet que
`buildPotentiallyThrowingCall` usa para cualquier llamada en el hilo PRINCIPAL desde Stage 45. El
desenrollado SEH de una excepcion asi no encuentra ningun manejador en la pila de ESE hilo; el
comportamiento por defecto de Windows ante una excepcion SEH sin manejar es terminar el PROCESO
COMPLETO -- el mismo comportamiento que tendria cualquier excepcion SEH sin manejar en cualquier
hilo de cualquier programa Win32/C++ comun, no un limite inventado por este compilador. Esto no
se puede ejercitar de forma segura en el ejemplo empaquetado de esta ronda (mataria al proceso de
verificacion entero), asi que queda documentado aca y en el comentario de cabecera de `Thread.ns`,
verificado por razonamiento sobre el comportamiento ya documentado de Windows, no por un test
ejecutable.

**El contenido del trabajo de un hilo `Detach()`-ado no es verificable deterministicamente.** Una
vez que `Detach()` cierra el HANDLE, este programa (o cualquier codigo NativeSharp) pierde
completamente la capacidad de saber cuando ese hilo termina -- exactamente la razon por la que
`Detach()` es la operacion peligrosa de las dos. El ejemplo empaquetado ejercita `Detach()` (que
retorna de inmediato y no crashea) pero deliberadamente NO verifica que efecto tuvo el hilo
detached: no hay forma correcta de hacerlo sin agregar una segunda primitiva de sincronizacion
(un semaforo, un evento) que Stage 54.1 no incluye todavia.

**Sin `Interlocked`/atomics todavia (Stage 54.2).** El ejemplo empaquetado evita deliberadamente
cualquier escritura compartida entre hilos: cuando varios hilos corren en simultaneo (Seccion 4
de `Main.ns`), cada uno escribe en su PROPIO campo estatico dedicado. Esto no es una limitacion
del lenguaje que se esconde -- es la eleccion correcta para esta ronda: verificar concurrencia
real sin necesitar primitivas que todavia no existen.

## Verificacion

Siguiendo al pie de la letra la regla de rigor que Alberto establecio para Stage 54:

- **Fixture minima**: confirma que las tres operaciones nucleo (`ThreadCreate`/`ThreadJoin`/
  `ThreadDetach`) lowerean a LLVM y cada una emite una llamada directa a su simbolo runtime
  exacto. El trampolin en si NO se busca por nombre en el modulo del PROGRAMA (vive en el modulo
  del RUNTIME, un modulo LLVM completamente separado, combinado recien a nivel de objeto COFF por
  el linker) -- su correctitud se verifica por ejecucion real, no por texto de IR.
- **Ejemplo empaquetado** (`examples/project_stage54_1`), compilado a traves del Driver real, a
  O0 y O2, con **9 chequeos numerados** en `Main.ns`: un hilo ligado a un metodo static; un hilo
  ligado a un metodo de INSTANCIA (con el receptor mantenido vivo en el mismo scope, respetando
  el orden de declaracion "objeto primero, Thread despues"); un hilo de LAMBDA que captura una
  variable escalar; TRES hilos corriendo en simultaneo, cada uno en su propio campo (sin carrera
  de datos posible); la verificacion CENTRAL de esta ronda -- un `Thread` que se deja caer fuera
  de scope SIN ningun `Join()` explicito, confirmando que `~Thread()` de verdad hizo el
  auto-`Join()`; idempotencia de `Join()`/`Detach()` (llamar dos veces, o `Join()` seguido de
  `Detach()`, sin colgar ni crashear); `Detach()` como primera operacion (sin `Join()` previo); y
  un control negativo -- un campo que ningun worker toca jamas, confirmando que este programa
  realmente discrimina.
- **Ejecucion REAL bajo Wine, VARIAS VECES por nivel de optimizacion** (5 corridas a O0, 5 a O2 --
  10 en total, no 1 como cada ronda de Stage 53): cada corrida exige exit code 0 y ausencia de
  crash, individualmente. Esto es exactamente la instruccion explicita de Alberto de correr los
  tests de concurrencia varias veces, no una sola.
- **Control negativo manual**: se corrio una copia del test dedicado con un chequeo
  deliberadamente roto (`FromStaticMethod != 999` en vez de `!= 111`) -- las 10 corridas de Wine
  fallaron de inmediato con el exit code esperado (1), confirmando que la matriz realmente
  discrimina antes de restaurar el archivo original.
- **66/66 chequeos del test dedicado** (`nsc_stage54_1_threads_tests`), estable en multiples
  corridas del binario de test completo (cada corrida del binario ya incluye las 10 ejecuciones
  reales bajo Wine descritas arriba).
- **Suite completa**: 218/219 -- el unico fallo (`nsc.stage52-4.owned-cleanup-runtime`) es el
  mismo hallazgo preexistente ya documentado desde Stage 53.5, no relacionado con esta ronda.

## Que sigue

Con Stage 54.1 cerrado, `NativeSharp.Core.Threading.Thread` existe y esta verificado, pero
NativeSharp todavia no tiene ninguna forma de coordinar hilos de forma mas fina que
`Join()`/`Detach()` -- ningun `Interlocked`, ningun mutex, ninguna variable de condicion. El
desglose tentativo que se discutio con Alberto antes de arrancar esta ronda (Stage 54.2 Atomics,
54.3 Mutex/CriticalSection, 54.4 variables de condicion, 54.5 otras primitivas si hiciera falta)
sigue siendo una propuesta, no un compromiso -- cada subronda de Stage 54 se arranca solo con
señal explicita de Alberto, igual que cada etapa de este proyecto desde el principio.
