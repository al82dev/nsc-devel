# Stage 54.2 "Atomics" -- AtomicInt32/AtomicInt64, sin `Interlocked` al estilo C#

## Por que esta ronda cambio de diseño DOS veces durante la implementacion

Stage 54.1 cerro con una limitacion documentada explicitamente: cuando varios hilos corren en
simultaneo, cada uno escribe en su PROPIO campo estatico dedicado, evitando a proposito cualquier
escritura compartida entre hilos -- porque NativeSharp todavia no tenia ninguna forma segura de
sincronizar un contador compartido. Stage 54.2 cierra exactamente ese hueco.

El plan de arranque, discutido antes de escribir una sola linea de codigo, era calcar
`System.Threading.Interlocked` de C# casi al pie de la letra: una clase de utilidad estatica con
diez metodos (`Increment`/`Decrement`/`Add`/`Exchange`/`CompareExchange`, cada uno en version
`int` y `long`), cada uno recibiendo la ubicacion por puntero crudo -- `Interlocked.
Increment(&contador)`, sustituyendo el `ref int` de C# por el sistema de punteros
(`int*`/`&`/`*`) que NativeSharp ya tiene desde antes de Stage 54.

Esa idea no llego a un programa de prueba completo antes de encontrar el primer obstaculo. Y el
segundo obstaculo -- independiente del primero -- aparecio recien durante la implementacion de la
capa de runtime, ya con el diseño nuevo elegido. Ambos se documentan completos abajo, porque los
dos cambiaron la forma final de esta API de una manera que vale la pena entender, no son detalles
de implementacion para ignorar.

## Primer obstaculo: la tuberia OOP moderna no baja `&` ni `*`, nunca

Un programa de prueba minimo con `Interlocked.Increment(&contador)` fallo en la compilacion con un
error muy especifico: `"Stage 34 OOP lowering keeps raw pointer address/dereference in the
procedural backend boundary"`. Rastreando ese string hasta su origen (`ProjectOopHirModel.cpp`,
caso `UnaryExpression`), la causa quedo clara: es un limite DELIBERADO y DURO, no un bug ni algo
especifico de esta ronda. El codigo dispara ese error para CUALQUIER uso de `&` o `*`, sin
importar sobre que operando esten aplicados (un local, un campo, un elemento de arreglo) --
la condicion es sobre el TOKEN del operador, no sobre el operando.

La razon de fondo: esos dos operadores existen en NativeSharp desde bastante antes de Stage 34 --
el analizador sintactico y semantico los aceptan, y hay tests que lo confirman -- pero pertenecen
a una tuberia de generacion de codigo MAS VIEJA ("procedural", pre-Stage-35), que la tuberia OOP
moderna (la que usan TODAS las stages desde 41 en adelante, incluida esta) nunca implemento. Esto
no es una laguna que Stage 54.2 podria haber llenado con un poco mas de trabajo -- es un limite
estructural de la arquitectura del compilador tal como esta hoy, y vuelve inviable, para siempre
mientras esa arquitectura no cambie, cualquier API publica que le exija a codigo NativeSharp
MODERNO escribir `&algo`.

## La solucion: metodos de instancia sobre un campo privado, no un puntero explicito

En vez de pelear contra ese limite, el diseño cambio de forma: cada operacion atomica pasa a ser
un METODO DE INSTANCIA que opera implicitamente sobre el campo privado `value_` DEL RECEPTOR
(`contador.Increment()`), nunca sobre un puntero que el codigo NativeSharp tenga que escribir. El
precedente real mas cercano no es ya `System.Threading.Interlocked` de C#, sino
`java.util.concurrent.atomic.AtomicInteger`/`AtomicLong` de Java -- un lenguaje que TAMPOCO expone
punteros/`ref` al programador para este tipo de API, y resuelve el mismo problema exactamente asi:
una clase wrapper por cada ancho, en vez de una utilidad estatica parametrizada por ubicacion.

```csharp
public class AtomicInt32
{
    public AtomicInt32(int initialValue);
    public int Value { get; }         // lectura NO atomica, ver "Limitaciones" mas abajo
    public int Increment();            // devuelve el valor NUEVO
    public int Decrement();            // devuelve el valor NUEVO
    public int Add(int value);         // devuelve el valor NUEVO
    public int Exchange(int value);    // devuelve el valor VIEJO
    public int CompareExchange(int value, int comparand);  // devuelve el valor VIEJO, siempre
}
// AtomicInt64: identica, con "long" en vez de "int".
```

El contrato de valores de retorno es identico al `Interlocked` real de C#: `Increment`/
`Decrement`/`Add` devuelven el valor NUEVO (despues de la operacion); `Exchange` devuelve el valor
VIEJO (el que estaba antes del intercambio); `CompareExchange` devuelve el valor VIEJO SIEMPRE,
coincida o no con `comparand` -- el llamante compara el valor devuelto contra su propio
`comparand` para saber si el intercambio realmente ocurrio (`if (contador.CompareExchange(nuevo,
viejo) == viejo) { /* si cambio */ }`).

### Como el compilador arma la direccion del campo sin el operador `&`

El truco que hace viable este diseño: el operador `&` esta bloqueado para codigo NativeSharp
FUENTE, pero el propio compilador (el C++ de `ProjectMethodBodyModel.cpp`/`ProjectOopHirModel.cpp`)
puede seguir construyendo una direccion de campo libremente -- es exactamente lo que YA hace,
puertas adentro, cada vez que baja una asignacion a un campo (`this.value_ = x;` necesita la
DIRECCION de `value_` para poder escribir ahi, no su valor). Ese mecanismo interno
(`ProjectOopHirNodeKind::FieldAddress`, con su contraparte en `ProjectOopMirModel.cpp` y
`ProjectOopLlvmLowerer.cpp`) no tiene relacion ninguna con el operador `&` de NativeSharp -- es un
nodo de HIR/MIR interno que cualquier parte del compilador puede construir a mano, sin pasar por
el analizador sintactico ni por la regla que bloquea `&`/`*`.

El reconocimiento de las diez operaciones (`ProjectMethodBodyModel.cpp`) resuelve, UNA SOLA VEZ,
el ordinal del campo `value_` dentro de la clase declarante (buscando entre los miembros de esa
clase el que sea `Field` y se llame `value_`). La bajada a HIR (`ProjectOopHirModel.cpp`) arma un
nodo `FieldAddress` SINTETICO usando ese ordinal y el receptor de la llamada (`contador` en
`contador.Increment()`, obtenido via `MethodCallBinding::receiverSyntax`) como su unico hijo.

Ni la conversion HIR->MIR ni la bajada a LLVM necesitaron NINGUN codigo extra para esta pieza: a
esas dos etapas, el nodo sintetico es indistinguible de un acceso a campo real -- ambas ya tenian
un caso generico para `FieldAddress` desde que existe, y lo reusan sin saber que este particular
nodo fue construido a mano en vez de venir de un `this.campo = x;` real escrito por el usuario.
Este es el mismo patron de reuso de maquinaria existente que ya goberno el diseño de Stage 54.1
(el trampolin de hilos reusa el layout de delegate existente en vez de inventar uno nuevo).

## Segundo obstaculo: `Interlocked*64` no son exports reales de kernel32.dll en x64

Con el diseño de wrapper-classes ya elegido, la capa de runtime (`WindowsRuntimeBuilder.cpp`)
seguia planeada como una envoltura fina sobre las funciones REALES de Win32:
`InterlockedIncrement`/`InterlockedExchangeAdd`/`InterlockedExchange`/
`InterlockedCompareExchange` para 32 bits, y sus variantes `*64` para 64 bits -- el mismo patron
de "declarar el import de kernel32.dll y llamarlo" que goberno cada core operation de Stage 53.

Investigando la firma exacta de las variantes de 64 bits (`WebSearch`/`WebFetch` contra fuentes
tecnicas primarias -- un reporte de bug de mingw-w64 en SourceForge, y directamente el header
`mingw-w64/mingw-w64-headers/include/psdk_inc/intrin-impl.h`) aparecio el segundo obstaculo,
completamente independiente del primero: las variantes de 32 bits (`InterlockedIncrement`, etc.)
SI son exports reales, historicos, de kernel32.dll -- confirmado por `__imp_InterlockedIncrement`
presente en `libkernel32.a`. Pero las variantes de 64 bits (`_InterlockedIncrement64`, etc.) estan
definidas en esos headers como llamadas INLINE directas a intrinsecos del compilador GCC
(`__sync_*`), SIN ningun export de DLL de respaldo. La razon de fondo: x64 tuvo instrucciones
atomicas de 64 bits nativas desde el primer dia de la arquitectura -- nunca hizo falta un export
de DLL "de antes de que existieran los intrinsecos del compilador", como si hizo falta para 32
bits en la era de x86 de 32 bits.

## La solucion: instrucciones atomicas reales de LLVM, para AMBOS anchos

En vez de envolver un export de DLL que solo existe para 32 bits, las diez operaciones -- 32 y 64
bits por igual -- bajan a instrucciones atomicas REALES de hardware x86-64, emitidas directamente
por LLVM: `LLVMBuildAtomicRMW` (con `LLVMAtomicRMWBinOpAdd` para Increment/Decrement/Add,
`LLVMAtomicRMWBinOpXchg` para Exchange) y `LLVMBuildAtomicCmpXchg` (para CompareExchange), ambas
con `LLVMAtomicOrderingSequentiallyConsistent` -- la barrera de memoria mas fuerte que LLVM ofrece,
la misma garantia de orden que el `Interlocked` real de Win32 documenta para sus propias
funciones. Estas instrucciones lowerean, en el backend de x86-64 de LLVM, a `lock xadd`/`lock
xchg`/`lock cmpxchg` -- las MISMAS instrucciones de maquina que un `InterlockedIncrement` real de
kernel32.dll termina ejecutando puertas adentro.

Esto es, si algo, MAS fiel al comportamiento real de Windows que el plan original: codigo real
compilado con MSVC tambien evita la llamada a kernel32.dll en la practica -- el compilador de
Microsoft reconoce `_InterlockedIncrement`/etc. como intrinsecos del compilador desde hace
decadas, e inlinea la instruccion `lock xadd` directamente, sin pasar por ninguna DLL. NativeSharp
termina haciendo, por una razon distinta (la ausencia de export de 64 bits), lo mismo que el
compilador de produccion de Windows ya hace por razones de rendimiento.

Consecuencia directa, notable porque es la PRIMERA vez que pasa desde que empezo la disciplina de
sincronizar `kernel32Symbols`: esta ronda **no agrego ningun import nuevo de Win32**. Las cinco
ubicaciones que normalmente hay que sincronizar cada vez que el runtime gana un import
(`WindowsExecutableBuilder.cpp`, `WindowsLibraryBuilder.cpp`, y las tres copias locales en los
tests de host-integration) se quedan exactamente como estaban al cierre de Stage 54.1.

### `LlvmCApi.h`: valores verificados contra los headers reales de LLVM, no adivinados

`LlvmCApi.h` es el subconjunto de la API C de LLVM que este proyecto mantiene a mano (declaraciones
`extern "C"` contra `libLLVM-20.so`, sin el header completo de LLVM). Necesitaba dos enums nuevos
(`LLVMAtomicRMWBinOp`, `LLVMAtomicOrdering`) y dos funciones nuevas
(`LLVMBuildAtomicRMW`/`LLVMBuildAtomicCmpXchg` -- notese "Atomic" en el nombre real de la segunda,
no `LLVMBuildCmpXchg`). En vez de adivinar los valores numericos de los enums, se instalo
`llvm-20-dev` via `apt-get` para poder leer el header REAL, `/usr/include/llvm-c-20/llvm-c/Core.h`,
y copiar los valores exactos -- incluido el hueco deliberado entre `Monotonic = 2` y `Acquire = 4`
que el LLVM real tambien tiene (el valor 3 esta reservado, sin nombre, en el enum original). Solo
se declararon los miembros que este compilador realmente emite (`Xchg`/`Add` del binop; los siete
niveles de `LLVMAtomicOrdering` completos, aunque solo `SequentiallyConsistent` se usa hoy, para
que el enum sea auto-descriptivo si una ronda futura necesita un nivel mas relajado).

### `WindowsRuntimeBuilder.cpp`: cinco funciones constructoras, compartidas por ancho

Cuatro funciones nuevas, cada una parametrizada por `bitWidth` (32 o 64) para servir ambos anchos
sin duplicar logica -- el mismo patron de "helper compartido parametrizado" que Stage 53.6 uso para
`wantDirectories`:

- **`buildInterlockedIncrementOrDecrement`** (parametro extra `isIncrement`): `delta` es `1` o
  `-1` segun el caso; `LLVMBuildAtomicRMW` con `Add` calcula el valor VIEJO atomicamente; el valor
  NUEVO (lo que esta funcion devuelve, siguiendo el contrato de Increment/Decrement) se calcula
  DESPUES, en C++ normal, sumando `delta` al valor viejo devuelto por la instruccion atomica --
  sin ninguna segunda operacion atomica, porque ese calculo ya no necesita ser atomico: la unica
  parte que de verdad tenia que ser indivisible (leer-y-sumar) ya paso adentro de la instruccion de
  hardware.
- **`buildInterlockedAdd`**: mismo patron, `delta` es el argumento explicito del llamante en vez
  de una constante `1`/`-1`.
- **`buildInterlockedExchange`**: `LLVMBuildAtomicRMW` con `Xchg`; el valor VIEJO que la
  instruccion devuelve directamente ES el valor de retorno de la funcion -- sin ningun calculo
  extra, a diferencia de Increment/Decrement/Add.
- **`buildInterlockedCompareExchange`**: `LLVMBuildAtomicCmpXchg` devuelve un agregado LLVM `{T,
  i1}` (el valor viejo, y si coincidio o no); esta funcion extrae solo el primer campo con
  `LLVMBuildExtractValue` -- el segundo campo (el booleano de "coincidio") no hace falta, porque el
  contrato de esta API (identico al `Interlocked.CompareExchange` real de C#) es que el LLAMANTE
  compare el valor devuelto contra su propio `comparand`, no que la funcion se lo diga
  directamente. Nota de firma: el orden de argumentos de LLVM es `(ptr, compare, new)`, mientras
  que el orden de Win32/C# es `(location, value, comparand)` -- `value`/`comparand` estan
  intercambiados de posicion entre ambas convenciones, y esta funcion arma la llamada a LLVM con
  el orden correcto para no invertir silenciosamente el contrato.

Los diez simbolos de runtime resultantes: `__nativesharp_interlocked_increment_int32`,
`_decrement_int32`, `_increment_int64`, `_decrement_int64`, `_add_int32`, `_add_int64`,
`_exchange_int32`, `_exchange_int64`, `_compare_exchange_int32`, `_compare_exchange_int64`.

## Las diez core operations: capas HIR/MIR/LLVM

`ProjectMethodBodyModel.h` gana un campo nuevo en `MethodCoreBinding`:
`std::optional<std::size_t> implicitFieldMemberOrdinal` -- el ordinal del campo `value_` resuelto
durante el reconocimiento, que la bajada a HIR necesita para armar el nodo `FieldAddress`
sintetico descrito arriba. Se agrego al FINAL de la struct (posicion segura para todos los
sitios de inicializacion posicional preexistentes -- solo generan warnings inofensivos de
`-Wmissing-field-initializers`, ningun error).

`ProjectOopHirModel.cpp` gana un bloque de reconocimiento DEDICADO, deliberadamente SEPARADO del
bloque compartido de "operaciones de String/etc." que agrupa la mayoria de las core operations
anteriores por numero de operandos esperados -- las operaciones atomicas necesitan un manejo
especial (armar el nodo `FieldAddress` a partir del receptor, en vez de simplemente lowerear los
hijos sintacticos de la llamada) que no encaja en ese bucket generico. El bloque valida que el
resultado no se pida en contexto de direccion (`addressContext`), que `implicitFieldMemberOrdinal`
este presente, y que la llamada tenga un receptor resuelto -- despues arma el nodo `FieldAddress`
y, sobre el, uno de los diez nodos `CoreInterlockedIncrementInt32`...`CoreInterlockedCompareExchangeInt64`.

`ProjectOopMirModel.h`/`.cpp` y `ProjectOopLlvmLowerer.cpp` NO necesitaron ningun cambio para
soportar el nodo `FieldAddress` sintetico -- ya tenian, desde antes de esta ronda, un caso generico
para `FieldAddress`/`FieldValue` que no distingue si el nodo vino de un `this.campo = x` real
escrito por el usuario o fue construido a mano por otra parte del compilador. Los diez casos de
lowering en `ProjectOopLlvmLowerer.cpp` simplemente llaman `lowerExpression` sobre el primer hijo
(la direccion) para obtener el puntero, y ese `lowerExpression` dispatcha automaticamente al caso
`FieldAddress` existente.

## Limitaciones documentadas de esta ronda (deliberadas, no descuidos)

**`Value` es una lectura NO atomica.** Una carga de 4/8 bytes alineados en x86-64 es, de por si,
atomica a nivel de hardware (nunca se puede leer un valor "partido a la mitad"), pero esta
propiedad no emite ninguna instruccion con prefijo `lock` ni ninguna barrera de memoria/fence -- a
diferencia de `AtomicInteger.get()` de Java (que es `volatile`, con semantica de barrera real).
Para Stage 54.2, esto es una lectura de "mejor esfuerzo" del valor actual, suficiente para
diagnostico/logging o para el propio uso interno de los tests de esta ronda; codigo que necesite
el valor mas reciente con garantias de orden entre hilos deberia usar `Exchange`/`CompareExchange`
con el mismo valor como ambos argumentos, en vez de depender de `Value` en una decision
sincronizada.

**Ninguna de las dos clases tiene RAII.** No envuelven ningun recurso de Win32 que liberar -- solo
cuatro/ocho bytes de memoria ordinaria del objeto -- mismo espiritu que cualquier clase de datos
comun de esta biblioteca.

**Sin mutex/CriticalSection/variables de condicion todavia.** `AtomicInt32`/`AtomicInt64` resuelven
UN caso especifico (un contador entero compartido), no coordinacion general entre hilos. El
desglose tentativo de Stage 54 sigue proponiendo Mutex/CriticalSection para 54.3 y variables de
condicion para 54.4 -- sin compromiso todavia, cada subronda arranca solo con señal explicita de
Alberto.

## Verificacion

Siguiendo la misma regla de rigor establecida para Stage 54 (sin recortar la matriz, correr los
tests de concurrencia varias veces, verificar resultados EXACTOS, nunca `Sleep()` como sustituto de
sincronizacion, documentar que no se puede verificar deterministicamente en este sandbox):

- **Fixture minima** (`testAtomicCoreOperationsLowerToRuntimeCalls`): confirma que las diez
  operaciones lowerean a LLVM y cada una emite una llamada directa a su simbolo runtime exacto,
  mas la presencia de `"field.addr"` en el IR (confirmando que el nodo `FieldAddress` sintetico
  realmente se construyo).
- **Fixture del modulo de runtime** (`testRuntimeHooksEmitRealAtomicInstructions`, un tipo de
  fixture nuevo sin precedente en Stage 54.1): construye el modulo de runtime DIRECTAMENTE via
  `nativesharp::runtime::WindowsRuntimeBuilder::buildSupport`, lo verifica con LLVM (`verify()`), y
  confirma en su texto de IR la presencia de `"atomicrmw add"`, `"atomicrmw xchg"`, `"cmpxchg"`,
  `"seq_cst"` -- MAS un chequeo NEGATIVO confirmando que NINGUNO de
  `@InterlockedIncrement`/`@InterlockedExchangeAdd`/`@InterlockedExchange`/
  `@InterlockedCompareExchange` aparece en ningun lado del modulo, probando que el segundo pivot de
  diseño de esta ronda (instrucciones atomicas reales, no wrapping de kernel32.dll) se implemento
  de verdad, no solo se documento.
- **Ejemplo empaquetado** (`examples/project_stage54_2`), compilado a traves del Driver real, a O0
  y O2, con **27 chequeos numerados** en `Main.ns`: Secciones 1-2 verifican el contrato de valor de
  retorno EXACTO de las diez operaciones (`AtomicInt32` y `AtomicInt64`), incluyendo
  explicitamente el camino en que `CompareExchange` NO coincide (no alcanza con probar solo el
  camino feliz); Secciones 3-4 son la prueba REAL de atomicidad -- 4 hilos de sistema operativo,
  arrancados todos ANTES de que el programa llame al primer `Join()` (concurrencia genuina, no
  hilos secuenciales disfrazados), incrementando un MISMO `AtomicInt32`/`AtomicInt64` compartido
  2500 veces cada uno; si `Increment()` no fuera realmente atomico, alguna escritura se perderia
  (una condicion de carrera clasica de "leer-sumar-escribir" sin proteccion) y el total final seria
  MENOR que 10000 -- Seccion 5 es un control negativo (un campo que ningun codigo toca jamas, sigue
  en 0).
- **Ejecucion REAL bajo Wine, varias veces por nivel de optimizacion** (5 corridas a O0, 5 a O2 --
  10 en total): cada corrida exige exit code 0 y ausencia de crash, individualmente.
- **Control negativo manual**: se cambio el `10000` esperado de la Seccion 3 a `999999` en una
  copia temporal de `Main.ns` -- fallo de inmediato con exit code 25 (el codigo exacto de esa
  seccion) en todas las corridas, confirmando que la matriz realmente discrimina antes de
  restaurar el archivo original (confirmado via `diff`).
- **81/81 chequeos del test dedicado** (`nsc_stage54_2_atomics_tests`), estable en multiples
  corridas del binario completo (cada corrida ya incluye las 10 ejecuciones reales bajo Wine
  descritas arriba).
- **Suite completa**: 219/220 -- el unico fallo (`nsc.stage52-4.owned-cleanup-runtime`) es el mismo
  hallazgo preexistente, documentado desde Stage 53.5, no relacionado con esta ronda.

## Que sigue

Con Stage 54.2 cerrado, NativeSharp tiene contadores atomicos verificados de extremo a extremo,
pero sigue sin ninguna forma de proteger una seccion critica mas general (varios campos, o codigo
arbitrario, que deba ejecutarse sin interrupcion de otro hilo). El desglose tentativo sigue siendo
Stage 54.3 (Mutex/CriticalSection), 54.4 (variables de condicion), 54.5 (otras primitivas si hace
falta) -- una propuesta, no un compromiso. Esta ronda dejo ademas una leccion concreta para esas
subrondas futuras: antes de asumir que una API puede calcar directamente su equivalente de C#, vale
la pena confirmar temprano si esa API necesitaria el operador `&`/`*` (bloqueado en la tuberia OOP
moderna) o algun export de Win32 que podria no existir en la forma esperada -- los dos obstaculos
que esta ronda encontro DURANTE la implementacion, no antes.
