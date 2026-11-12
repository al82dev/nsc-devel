# Reproducción y análisis del fallback SEH

La revisión de trabajo es `development-fixes-limitaciones`; este registro no
certifica un build limpio. Se usó el compilador aislado
`A:/nsc-fixes-evidencia/build/bin/nsc.exe`, que informó el build hash
`b2a62146822e8e54`, y el runtime y bibliotecas de importación configurados en
`A:/nsc-fixes-evidencia/build`. Los temporales, cachés de usuario y resultados
se dirigieron a `A:/nsc-fixes-evidencia`.

Desde `A:/nsc-devel-fixes`, el comando
`A:/nsc-fixes-evidencia/async-finally-20261111/replacement-no-version-20261112.cmd`
ejecutó `tests/integration/async_finally_exception_replacement.py`; el runner
compartido invocó compilador y ejecutables con cwd del repositorio. El proyecto
temporal no seleccionó una versión de lenguaje. Se mantuvieron los fixtures
originales. El resultado completo está en el `.log` de igual nombre:

* `sync_preserve`, `sync_nested_preserve`, `sync_replace` y
  `sync_call_replace`: salida 42 en O0 y O2.
* `sync_inner_catch`: fallo LLVM `Cleanup funclets for the SEH personality
  cannot contain exceptional actions` en O0 y O2.
* `async_ready_replace` y `async_suspended_replace`: fallo de lowering
  `semantic invariant forbids structured exit across an active cleanup
  funclet` en O0 y O2.

El comando `A:/nsc-fixes-evidencia/async-finally-20261111/catch-owner-recheck-20261112.cmd`
ejecutó el control compartido `async_catch_owner_continuation.py`: sus diez
casos devolvieron 42 en O0 y O2. El log correspondiente documenta los veinte
resultados. Estas pruebas no cubren la propiedad ante una excepción extranjera
ni resuelven los fallos de reemplazo.

## Causa acotada y límites

En `OopLlvmImplementation::emitExceptionalCleanupChain`, el cuerpo de cleanup
se emite dentro del `cleanuppad` nativo; el catch filtrado de NativeSharp y el
fallback para excepciones extranjeras son rutas distintas. El documento
`agente3-async-evidencia.md` registra el ensayo que omitió el fallback: el
caso síncrono pasó, pero esa omisión elimina una limpieza necesaria y no es una
corrección. La rutina de LLVM
[`calculateSEHStateNumbers`](https://llvm.org/doxygen/WinEHPrepare_8cpp_source.html)
rechaza una acción excepcional anidada que usa el `cleanuppad` como padre;
ese código de referencia público no acredita por sí solo la revisión del LLVM
utilizado para esta compilación. La
[documentación de excepciones de LLVM](https://llvm.org/docs/ExceptionHandling.html)
describe los funclets y la diferencia con un modelo general de relanzamiento.

El ensayo nativo previo `eh-replace-investigation/finally-outline-nested.ll`
demostró para un ejemplo aislado que un helper invocado desde un cleanup puede
capturar internamente otra excepción y permitir que la excepción externa
continúe; no demuestra el lowering de NativeSharp ni las salidas suspendibles.
El registro `shared-async-drop.md` conserva un ensayo distinto donde una
excepción que escapa del helper mostró diferencias con optimización al inlinear
el llamador. No se infiere de ello una regla general de `noinline`.

Para un candidato síncrono acotado al caso real se deben transferir al helper
las direcciones efectivamente utilizadas, incluido el slot del catch interno.
El lowering también debe aislar y restaurar las estructuras de emisión que
pueden contener bloques o valores LLVM de la función anterior, en particular
`exceptionDispatchBlocks_`, `cleanupChainCache_`, `cleanupRegions_`,
`localStorage_`, handlers, funclets y estado de depuración. Hay que justificar
el manejo de parámetros, flags de destrucción, capturas y receptor para todo
cuerpo admitido por la guarda del candidato. La excepción interna puede no ser
capturada y escapar del helper; una solución debe conservar también esa ruta y
su comportamiento en O0/O2. El problema async de reanudación y dominancia
requiere un tratamiento separado: no se ha demostrado que el helper síncrono
lo resuelva.

**Estado:** tres reproducciones abiertas; sin parche funcional integrado.
Siguiente acción: definir y comprobar una guarda MIR y un protocolo de estado
que cubran los cuerpos admitidos, seguido de controles de excepción extranjera,
propagación, locales y optimización antes de integrar código. No suprimir el
fallback ni convertir el experimento de LLVM aislado en prueba del compilador.

## Guarda y protocolo examinados

`dump-sync-catch-mir-20261112.cmd`, bajo la misma carpeta de evidencia, volcó
el MIR original sin seleccionar una versión de lenguaje; el resultado está en
`sync-catch-mir/mir.log`. El cuerpo de `finally` de la reproducción consta de
un `TryRegion`, un `Throw` dirigido a esa región, construcción de la excepción,
un `CatchRegion` con local 0 y una escritura a campo estático que lee un campo
de ese local. No usa parámetros ni `this` directamente.

Una guarda conservadora podría recorrer iterativamente el árbol, reconocer los
identificadores de regiones y locales declarados dentro de él y rechazar
referencias a ámbitos exteriores, salidas estructuradas y suspensión. Para un
cuerpo que sólo use locales de catch internos, el helper podría reservar esos
slots en su propio entry. Esa guarda sería sólo una condición de elegibilidad,
no una prueba de corrección ni una solución general; las acciones implícitas
de construcción, destrucción y errores también deben preservarse. El emisor
debe aislar y recuperar todas las estructuras que se modifican durante
`lowerStatement`, incluyendo las cachés de bloques, propietarios de excepción,
ámbitos de depuración, localizadores y estado de emisión. La lista completa y
las invariantes de restauración no están verificadas.

El caso donde una nueva excepción sale del helper permanece abierto incluso con
esa guarda, porque una operación implícita puede fallar. El ensayo nativo
preexistente observó que marcar solamente el llamador como `noinline` producía
el resultado esperado en O0/O2 en ese fixture, mientras que la variante con
inlining del llamador no lo hacía; no prueba que `noinline` sea una solución
general. Además, `OopLlvmDeclarations.cpp` aplica `alwaysinline` cuando la
política explícita es `Required`, de modo que imponer `noinline` al llamador
podría contradecir un contrato vigente. No se integró la guarda específica ni
se alteró la política de inlining.

Como control adicional, `check-alwaysinline-seh.cmd` generó una copia del
fixture nativo anterior con `alwaysinline` en `probe` y `noinline` en el helper.
`clang` y `lld-link`, resueltos por nombre, produjeron los ejecutables en A;
el directorio de trabajo de los procesos fue el repositorio. Tanto O0 como O2
agotaron el límite de diez segundos de ejecución y fueron terminados por el
ejecutor. Comandos, advertencia de triple de destino y resultados figuran en
`A:/nsc-fixes-evidencia/async-finally-20261111/check-alwaysinline-seh.log`.
Un control posterior ejecutó los binarios preexistentes
`finally-outline-escape-noinline-O0.exe` y `-O2.exe` con el mismo cwd y entorno
actuales: ambos también agotaron diez segundos, aunque su log histórico registró
42. El resultado actual está en `check-native-control.log`. Por lo tanto, los
timeouts nuevos no distinguen los cambios de inlining y no permiten concluir
que la variante alwaysinline falle por esa causa. Está pendiente explicar la
diferencia de ejecución del control.
Una nueva ejecución del control histórico O0 con límite de 45 segundos terminó
con salida 0 en lugar de 42 (`check-native-control-o0-long.log`); ampliar el
plazo no restableció el resultado esperado. No se conoce aún la causa de la
variación y no se atribuye a la estrategia de generación.

Un segundo ensayo conservó `probe alwaysinline` pero trasladó la región dueña
del cleanup a una función separada `noinline`; también terminó por timeout en
O0/O2 (`check-stable-seh-frame.log`). Debido al fallo del control, este
resultado tampoco permite evaluar la alternativa. En ambos ensayos el límite
terminó los procesos; los resultados sólo describen estos fixtures nativos y no
el comportamiento de una implementación de NativeSharp.

Para comprobar la terminación se generó otra copia del control original: sus
dos salidas de `main` llaman explícitamente `ExitProcess` con el código de la
prueba. El control `noinline` devolvió 42 en O0/O2
(`check-native-explicit-exit.log`). Esto no identifica por sí solo la causa
exacta de la variación anterior, pero elimina la dependencia de retornar desde
el entry PE directo.

Las mismas salidas explícitas se incorporaron a copias de las dos variantes
(`check-seh-variants-explicit-exit.log`):

* `probe alwaysinline` con helper `noinline`: 42 en O0 y `0xE0000001` en O2.
* Wrapper `probe alwaysinline` que llama a un frame SEH separado `noinline`,
  el cual llama al helper `noinline`: 42 en O0 y O2.

El control requiere observar la segunda excepción y la traza 12. Esta segunda
forma ofrece una hipótesis para conservar el contrato de inlining del wrapper
y una frontera física SEH en este fixture. Es exclusivamente un experimento
LLVM nativo: no prueba la emisión de NativeSharp, el transporte de parámetros,
locales, ownership, salidas estructuradas, depuración, suspensión ni el caso
en que la excepción externa continúa intacta. No se aplicó en el compilador.

Una segunda variante del mecanismo usa una excepción exterior distinta, captura
la excepción interior y exige que el manejador exterior reciba la original y
que el entero compartido registre la secuencia 123. Con wrapper `alwaysinline`,
frame SEH `noinline`, helper `noinline` y `ExitProcess` explícito devolvió 42 en
O0/O2 (`check-frame-preserve-foreign.log`). Esta evidencia nativa complementa
la de sustitución; no demuestra el tratamiento de objetos ni que una corrutina
pueda suspender durante el desenrollado SEH.

## Inventario para estudiar el frame SEH separado

Este inventario procede de `ProjectOopMirFunction.h`, `ProjectOopMirNode.h`,
`ProjectOopMirNonlocalExit.h`, `OopLlvmCoroutineState.h`,
`OopLlvmFunctionDefinition.cpp`, `OopLlvmControlFlow.cpp`,
`OopLlvmStructuredExits.cpp` y `OopLlvmImplementation.h`. Identifica requisitos
de una hipótesis; no es una interfaz implementada ni prueba que sea suficiente:

* **Entradas y almacenamiento:** parámetros y su modo de paso, receptor,
  direcciones de locales referenciados en la región, capturas y celdas, y
  punteros de valores de retorno indirecto. Debe mantenerse la identidad de
  las ubicaciones, no copiar un valor cuando el cuerpo puede observar o
  modificar el almacenamiento original. Las referencias que no cruzan la
  frontera pueden reservarse localmente únicamente tras demostrarlo.
* **Vida y propiedad:** flags de destrucción de parámetros y locales,
  almacenamiento de objetos promovidos, propietarios de capturas, y slots
  de excepciones de catch y finally. El orden, la transferencia y la limpieza
  ante fallo no pueden duplicarse entre el frame nuevo y el llamador.
* **Continuaciones:** el MIR registra `Return`, `Break`, `Continue`, `Throw`,
  `Rethrow`, `Panic` y `CleanupContinuation`, junto con regiones de limpieza
  y destinos de ámbito/excepción. Un salto LLVM no puede cruzar funciones;
  una extracción necesita codificar la continuación y el valor de retorno o
  de `break`, y reanudar el destino correcto sin ejecutar dos veces las
  limpiezas. `returnStorage_` y `sretStorage_` participan en ese contrato.
* **Excepciones:** conservar catch/cleanup anidados, dueño del error
  NativeSharp, la excepción extranjera y el caso en que otra excepción la
  reemplaza. Los dos fixtures nativos sólo prueban sendas concretas y un entero
  compartido; no prueban objetos, identidad completa ni toda propagación.
* **Suspensión:** `OopLlvmCoroutineState` conserva `self`, `frame`, `future`,
  estado e IDs de reanudación, slots de catch/finally, bloques de cancelación
  y valores de replay. Un frame SEH ordinario en la pila no proporciona por sí
  mismo una continuación suspendible; el cruce entre suspensión y desenrollado
  externo sigue sin diseño o evidencia suficiente.
* **Estado del emisor:** aislar inserción LLVM y ubicación de depuración,
  función y MIR actuales, maps de almacenamiento/celdas/flags, pilas de
  loops, handlers, funclets, catch y cleanup, `exceptionDispatchBlocks_`,
  `cleanupChainCache_` y contadores. Identificar qué cachés son por función
  y cuáles pertenecen al módulo antes de vaciar o restaurar cualquiera.
* **Contratos públicos:** preservar la ABI, la política de inlining que recibe
  el consumidor, especializaciones genéricas y ubicaciones de depuración.
  El wrapper inline y el frame interno cumplen una forma puntual del ensayo,
  pero no acreditan todos esos contratos.

Antes de llevar la hipótesis al compilador se necesita una partición MIR con
entradas y salidas por identidad y pruebas de variantes: local mutable exterior,
parámetros `ref`/`out`, captura, destructor con efectos, nested catch/finally,
excepción original y reemplazo, retorno y break/continue, inlining, genéricos
y depuración. Reanudación, cancelación y abandono requieren además un protocolo
para el estado suspendido; no se consideran resueltos por el frame síncrono.

## Infraestructura existente y propuesta para comprobar

`emitClosureContextDestroyFunctionBody` en
`OopLlvmClosuresDelegatesEvents.cpp` crea un builder distinto y guarda/restaura
la función actual, el MIR, el contador, las pilas de cleanup/handlers/funclets
y el estado de celdas que usa ese destructor. Crea además un subprograma de
depuración sintético cuando procede. Es un antecedente para una función auxiliar
específica: no guarda todos los maps de locales, destinos, banderas o la
corrutina, ni puede utilizarse sin más para bajar una región arbitraria.

`prepareFunctionDefinitionState` reinicia estado por función;
`prepareFunctionCleanupState` llama a `collectLocalAllocas`, registra las
regiones y reserva flags y retorno. `collectLocalAllocas` reconoce catch,
using, declaraciones y temporales, y `collectLocalStorage` también puede crear
celdas o almacenamiento estático. Estas rutinas ayudan a enumerar efectos,
pero invocarlas directamente sobre una subregión podría duplicar identidades
o ejecutar inicializaciones no deseadas. No existe aquí evidencia de un
snapshot integral y transaccional reutilizable para extraer cualquier cuerpo.

Propuesta **no implementada**, primero para una región síncrona cuya partición
sea demostrable:

1. Recorrer el MIR y construir una descripción determinista de locales,
   parámetros, receptor, celdas, flags y salidas que cruzan la frontera,
   separados por clase e identificador. Mantener referencias al almacenamiento
   original para preservar aliasing; reservar en el frame nuevo sólo locales
   cuyo ámbito y todas sus referencias sean internos. Conservar explícitamente
   propiedad y flags compartidos cuando existan.
2. Emitir la región protegida en una función interna con su personalidad SEH,
   y el cuerpo excepcional en otra función cuando necesite catches internos.
   El llamador necesita un `invoke` con los destinos y limpiezas exteriores
   correctos. Una frontera interna no debe duplicar la limpieza que el emisor
   ya planificó en `cleanupRegionOrdinals`.
3. Usar builder independiente y restauración garantizada también en rutas de
   error, aislando las pilas/maps relativos a función e inicializando en el
   helper sólo las regiones y slots que le pertenecen. Compartir únicamente
   estado global de módulo cuya identidad deba ser común. Un subprograma y
   scopes correctos son necesarios con información de depuración.
4. Representar cada salida que atraviesa la partición mediante una
   continuación explícita y su valor, sin convertir una excepción del sistema
   en un error NativeSharp ni cambiar su registro. No copiar bloques LLVM de
   otra función como destinos. Las rutas sin cruce deben preservarse.
5. Rechazar la partición experimental si no se puede probar el conjunto de
   entradas/salidas, el cleanup o la preservación de depuración; conservar el
   fallo existente no equivale a corregirlo. Ninguna regla para async se deduce
   de este esquema síncrono.

`InliningPolicy.h` y `OopLlvmDeclarations.cpp` llevan `forceinline` a
`Required` y `alwaysinline`; `NS-INLINE-001` exige expandir las llamadas de
destino estático también en O0, preservar efectos y scopes inline o diagnosticar
si no puede hacerse. El fixture expande el wrapper original, pero conserva una
llamada al frame extraído: no se ha demostrado si esa transformación respeta la
expansión del cuerpo exigida por el contrato ni sus scopes de depuración. No se
considera preservado el contrato sólo porque LLVM acepte los atributos.

La inspección de dependencias de la extracción encontró además que
`collectLocalStorage` puede crear celdas o registrar almacenamiento estático
en `staticLocalStorage_`, que después consume `defineStaticLocalDestructors`.
Duplicar o revertir indiscriminadamente ese registro podría alterar identidad o
destrucción; un snapshot de mapas locales no basta para aislar efectos del
módulo. La rutina existente de helper de capturas utiliza su propio builder y
un subconjunto explícito de estado, pero no demuestra restauración completa al
aplicarla a todos los nodos MIR.

Como referencia de validación, `cmake/VerifyNativeSharpInlineDebugO0.cmake`
comprueba símbolos de inline, frames, locales y líneas en el PDB. No se ejecutó:
contiene `file(REMOVE_RECURSE ...)` y necesita una salida aislada segura. Una
prueba futura del helper debe comprobar también que su frame real siga visible;
no se considera validado por encontrar únicamente la marca `S_INLINESITE`.

### Identidad de static locales: propuesta y control

Para un posible frame auxiliar, la propuesta es inventariar los `staticLocal`
que participan en la partición y conservar la identidad preexistente del
registro `staticLocalStorage_` para el módulo. El mapa `localStorage_` del
helper apuntaría al mismo global registrado para el ordinal correspondiente;
el helper no debe crear otro storage, guard de inicialización ni registro de
destructor. Los mapas de bloques, locals automáticos, flags y handlers siguen
siendo relativos a cada función y requieren aislamiento/restauración propios.
Esto es una propuesta, no una modificación ni prueba de que el ordinal sea una
identidad suficiente en todas las especializaciones o unidades de compilación.

`check-static-frame.cmd` preparó dos fuentes nuevas de diagnóstico sin alterar
los ejemplos originales: ambas ejecutan dos veces un `finally` con `static int`,
`static raii string` y un guard automático con destructor observable. El control
sin catch anidado devolvió 42 en O0/O2, exigiendo el contador estático 12 y
dos destrucciones del guard local. La variante con catch anidado se rechazó al
compilar en O0/O2 con el error SEH conocido. En ambos volcados LLVM se observó
un global y estado para cada static, y una entrada de `llvm.global_dtors` para
el string; la existencia de esa entrada no demuestra su ejecución al cerrar el
proceso. Comandos, salidas, fuentes y volcados se conservan en
`A:/nsc-fixes-evidencia/async-finally-20261111/check-static-frame.log` y
`static-frame-probe/`. El compilador informó build hash `a03f8a80837d6b09`.

Un intento de observar el destructor estático mediante `Console.WriteLine` sin
referencia al SDK fue rechazado porque `Console` no resolvió; no llegó a
ejecutarse. Otro control de diagnóstico declaró una clase mínima
`NativeSharp.Core.Console` para activar el intrínseco y cambió el static a
`raii Guard`: retornó 42 en O0/O2 y produjo dos marcadores, correspondientes a
los dos destructores locales observados antes del retorno; no apareció un
tercer marcador atribuible al static
(`check-static-destructor-intrinsic.log`). Ese control no usó el SDK binario y
no distingue falta de ejecución, orden de terminación o pérdida de salida; no
se atribuye a linker, runtime ni compilador sin evidencia adicional.

Antes de integrar una extracción, la prueba debe verificar que las copias
normal/excepcional observan la misma variable y una sola inicialización, que
el destructor automático mantiene su orden y que sólo existe un registro
estático. La ejecución del destructor de módulo requiere un observador externo
fiable y una configuración de producto cuya terminación esté caracterizada;
continúa pendiente y no debe inferirse del IR.

Un experimento LLVM independiente (`check-frame-static-alias.cmd`) usa un
wrapper `alwaysinline`, un frame SEH y helper `noinline`, un global común y una
entrada `llvm.global_dtors`. La ruta normal y dos rutas de excepción comparten
el global: el resultado 42 exige tres visitas, una sola inicialización y la
traza 123 en cada ruta. Pasó O0/O2; el comando y salida están en
`check-frame-static-alias.log`, y la fuente y ejecutables en
`frame-static-alias/`. El guard es secuencial, por lo que no demuestra
inicialización concurrente.

Los volcados `optimized-O0.ll` y `optimized-O2.ll` muestran que el caller ya
no contiene una llamada directa al wrapper `probe`, pero sí un `invoke` al
frame interno `seh_frame`. Es evidencia de esa forma LLVM, no confirma el
contrato `forceinline` del lenguaje, los scopes, el transporte de estado ni la
generación de NativeSharp. El prototipo termina con `ExitProcess`, y no observa
la ejecución del destructor de módulo; no cambia la incertidumbre anterior.

Una variante posterior duplica el cuerpo en dos helpers LLVM diferentes, uno
para la vía normal y otro para la excepcional; ambos acceden al mismo static y
el guard común. Conserva las tres visitas y una sola inicialización como
condición de 42; pasó en O0/O2 (`check-frame-static-two-helpers.log`). Esta
prueba muestra esa configuración manual, no que el emisor pueda construirla de
forma segura ni que el guard sea concurrente. El proceso también termina con
`ExitProcess`; la ejecución del destructor global permanece sin demostrar.

## Reproducciones antes de modificar el emisor

`tests/integration/async_finally_frame_regression.py` reutiliza sin modificar
los casos originales `sync_preserve` y `sync_inner_catch`, y añade una variante
en la que el catch modifica un parámetro y otra con la misma operación dentro
de un método `forceinline`. El control `sync_preserve_original` devolvió 42
en O0 y O2. Las tres variantes con catch interno fueron rechazadas en ambos
niveles con `Cleanup funclets for the SEH personality cannot contain exceptional
actions`. Comando y evidencia:
`A:/nsc-fixes-evidencia/async-finally-20261111/frame-regression-20261112.cmd`
y `.log`; compilador incremental con build hash `a03f8a80837d6b09`, cwd del
repositorio y TEMP exclusivo `A:/nsc-fixes-evidencia/temp-async-eh-frame`.
Los seis fallos son reproducciones vigentes para ese ejecutable, no evidencia
de corrección. La comprobación adicional del IR y los scopes de `forceinline`
sigue pendiente una vez que el programa pueda compilar.

El ejecutor incorpora una comprobación que sólo corre tras pasar los casos
funcionales: identifica exactamente un `Work` con política `forceinline` en
MIR, exige una llamada directa al símbolo en el IR sin optimizar y comprueba
que el cuerpo textual de `Main$poll` en los IR optimizados O0/O2 no incluya esa
llamada directa en la forma detectada por la expresión regular; exige también
localizar el mismo caller en el IR crudo y observar allí la llamada. Una fuente
control sin finally pasó en ambos niveles (`check-forceinline-ir-caller-control.log`).
La comprobación del caso con finally permanece **no ejecutada** por los fallos
de compilación. La expresión no demuestra ausencia de llamadas indirectas,
alias, formas multilínea ni cuerpos auxiliares; tampoco inspecciona scopes de
depuración ni demuestra por sí sola la expansión completa del cuerpo.

La repetición `frame-regression-ir-20261112.cmd` registró el SHA-256 del
compilador antes y después: ambos fueron
`7df4b193b39ac3f176b829b87184eaa38360c35345c1fb7ee1e08318557a73fd`.
Los logs de hash y resultados están junto al comando. La prueba se inicia con
`python -B`, cwd del repositorio y TEMP/TMP configurados por el comando; el
ejecutor compartido no impone por sí solo esas rutas a otras invocaciones.

El nuevo runner devuelve 77 fuera de Windows. Se comprobó la rama de omisión
con `os.name` simulado (`check-nonwindows-skip.log`); no es una ejecución real
en otra plataforma. `check-effective-temp.log` confirma que Python seleccionó
`A:/nsc-fixes-evidencia/temp-async-eh-frame` bajo el entorno de campaña. El
directorio temporal no está fijado en el test portable: lo configura su
lanzador para esta campaña.

Después de incorporar la omisión, `frame-regression-skip-20261112.cmd` repitió
la prueba en Windows: el control original devolvió 42 en O0/O2 y los tres
fixtures de catch interno fallaron en compilación en ambos niveles con el
mismo diagnóstico SEH. El paso de IR del caso fallido continúa sin ejecutarse.
El TEMP efectivo está en `frame-regression-skip-temp-20261112.log` y los hashes
antes y después coinciden en el SHA-256 indicado arriba; la salida completa
está en `frame-regression-skip-20261112.log`.

## Diseño candidato de partición síncrona

Esta sección describe una hipótesis de implementación; **no está integrada ni
certificada**. El punto de entrada candidato es el lowering de un `TryRegion`
con `finally`, no la eliminación del fallback en
`emitExceptionalCleanupChain`. La extracción del cuerpo protegido completo a
un frame SEH interno proporciona la frontera física que los fixtures LLVM
ensayaron. El cleanup con catch interno se emite en otra función; las rutas
normal y excepcional deben compartir las mismas identidades de storage. El
llamador invoca el frame mediante la ruta de unwind ya vigente.

### Guarda conservadora del experimento

* Exigir `!coroutine_.active`: no alterar ni reescribir `poll`, reanudaciones,
  generadores, cancelación o abandono. Si la guarda no se cumple se mantiene
  el lowering existente; esto deja abiertos los casos async.
* Construir antes de emitir un inventario MIR de la región y comprobar que
  destinos, capturas, parámetros, locales, flags y rutas de cleanup se pueden
  clasificar como internos o externos. La primera variante experimental no
  debe admitir `return`, `break`, `continue`, `yield` ni `await` que crucen la
  frontera; tampoco referencias o transfers cuyo destino no pueda mapearse.
  Un `throw` que abandona la región es una salida necesaria y debe propagarse
  mediante SEH hacia el llamador, no convertirse en un entero ni tragarse.
* No usar un patrón de sintaxis idéntico al fixture como prueba general de
  elegibilidad. Una guarda debe razonar sobre identidades y efectos del MIR;
  si falta información, el caso conserva el comportamiento anterior y se
  registra pendiente, no se presenta como corregido.

### Contexto y restauración

Antes de entrar al helper, guardar el builder, función/MIR y posición actuales;
identidad y scopes de depuración; contador y nombres de bloques; receptor,
entorno y override de construcción; mapas de parámetros, locales, celdas,
objetos promovidos y flags; almacenamiento de retorno/sret; pilas de loops,
catch, handlers, funclets y cleanup; índices de despacho y caché de cadenas;
estado de expresión/overflow que pueda estar activo. Cada miembro debe
clasificarse y justificarse contra sus usos reales antes de implementarlo: esta
lista no demuestra por sí sola que el snapshot sea completo.

Utilizar un builder y pilas/mapas relativos al helper; registrar únicamente
regiones internas y transportar las direcciones y flags exteriores que la
partición demuestre necesarias. `staticLocalStorage_` es compartido a nivel de
módulo y debe conservar su identidad y registro único, no copiarse como si
fuera un local de la nueva función. Una restauración transaccional debe
ejecutarse también ante fallos de lowering, sin descartar declaraciones
globales válidas o dejar referencias a bloques de la función equivocada. Los
subprogramas sintéticos y scopes de depuración deben representar el frame
real; `ScopedDebugLocation` por sí solo no guarda el estado completo.

### Salidas y limpieza

La emisión debe repartir cada ordinal de cleanup entre el frame y el llamador:
el frame ejecuta sus acciones internas una vez, y el `invoke` exterior deja al
llamador sólo las acciones que le corresponden. Es necesario conservar el
orden de destrucción, el dueño de la excepción anterior y las acciones ante
una excepción nueva o extranjera; no se permite sustituir la excepción del
sistema por una copia. `lowerTryRegion` ya utiliza una forma SEH especial para
evitar limpiezas exteriores prematuras: la partición debe demostrar que no
reintroduce ese problema ni duplica catch/finally. No se ha definido aún un
algoritmo completo y verificado para todas las rutas de salida.

El MIR ya proporciona por salida `cleanupRegionOrdinals`,
`exceptionTargetRegionOrdinal`, `targetScopeOrdinal` y el tipo de
continuación; `ProjectOopMirCleanupRegion` vincula cada ordinal con su acción,
tipo y ownership. En el backend, `activeCleanupRegions_` conserva regiones en
orden exterior a interior, mientras `buildCurrentUnwindDestination` forma la
secuencia inversa hasta la profundidad retenida por el handler. Además,
`lowerExceptionalTransfer` puede insertar cleanups temporales activos en
lowering para tuplas y construcción parcial que no se deben ignorar al leer
únicamente la lista de la salida MIR.

`buildCurrentUnwindDestination` también consulta
`staticInitializationFailureUnwindBlock_` y puede construir o reutilizar
`captureCellExceptionalCleanupBlock_`. Son destinos LLVM ligados a la función
de origen: un helper no puede conservarlos como bloques propios. La partición
necesita aislar esos valores y conservar las obligaciones mediante la ruta del
llamador, o excluir esas situaciones hasta contar con evidencia suficiente.
Una guarda experimental podría declinar la extracción si
`staticInitializationFailureUnwindBlock_` está activo o si existen celdas
propias/`captureCellExceptionalCleanupBlock_` aún no modeladas, conservando el
lowering previo. Esas situaciones permanecerían pendientes; declinarlas no es
una corrección ni demuestra liberación de las celdas.

El fixture independiente `check-frame-capture-cell.py` usa una lambda con
captura por referencia desde el finally. El control devolvió 42 en O0/O2; la
variante con catch anidado falló al compilar en ambos niveles con el diagnóstico
SEH conocido. Los dos volcados contienen `eh.capture.cells`. El acceso correcto
al valor y la presencia del bloque no demuestran por sí solos la liberación
física de la celda. Evidencia: `check-frame-capture-cell.log` y
`frame-capture-cell/` bajo `A:/nsc-fixes-evidencia/async-finally-20261111`.

Una partición candidata debe verificar que, para cada salida admitida, la
secuencia real sea un segmento interno seguido por uno externo, ambos en el
orden original y sin ordinales duplicados. El frame se encarga del segmento
interno; la ruta del `invoke` exterior conserva únicamente lo que corresponde
hasta el handler de destino. Si una captura ocurre dentro del frame, no deben
ejecutarse las regiones exteriores retenidas. Si el destino es exterior, no
puede usarse un `LLVMBasicBlockRef` de otra función: el unwind debe llegar a
la continuación del llamador con la identidad de la excepción conservada.
Este reparto es una obligación de diseño, no está implementado ni probado; en
particular, los cleanups temporales y handlers anidados pueden invalidar la
hipótesis de segmentos contiguos.

`emitRaiiDestroy` consulta el storage por identidad y usa una guarda que lo
limpia antes de invocar destrucción. Las dos funciones no pueden mantener
copias independientes de ese estado: una excepción desde un destructor o una
segunda entrada de cleanup podría liberar dos veces. También hay que preservar
los flags de drop de structs y la propiedad de la excepción reemplazada. El
tratamiento de un destructor que lanza durante otro desenrollado debe verificarse
contra la ruta concreta y sus controles existentes; no se deduce un resultado
general a partir de la guarda de un tipo distinto.

El fixture independiente `check-frame-cleanup-order.py` exige en el control
destructores interior/finally/exterior en orden 123 y devolvió 42 en O0/O2. La
variante con catch anidado exige además que las excepciones interior y exterior
se destruyan en orden 65, pero falla en compilación con el diagnóstico SEH en
ambos niveles; no se observó su traza en ejecución. Evidencia:
`check-frame-cleanup-order.log` y `frame-cleanup-order/` en la misma zona.
El compilador de ambas corridas informó build hash `b91ecfaf473fc363`; su
SHA-256 antes y después fue
`5cc75d23c719b89d5caad0eb62d3011bbb4fe4d2b7e5e11d44fe89ee5878e0cc`.

En el MIR de la variante anidada, el throw exterior lleva el recorrido
`#2 -> #1 -> #0` (owner interior, finally y owner exterior). El owner creado
dentro del finally es `#3` y no aparece en esa lista porque todavía no estaba
activo; el throw interno tiene como destino el handler interno `#1`. Este
fixture ilustra por qué una partición no puede limitarse a dividir el vector
del throw original: debe actualizar las acciones activas al entrar al finally.
En ese mismo volcado los registros de los owners muestran `ownership=none`,
mientras los nodos de declaración conservan `object-ownership=owned` y
`storage-ownership=owned`. La clasificación no puede inferir destrucción y
liberación a partir sólo del campo `ownership` del registro de cleanup: debe
conservar la acción y sus campos de storage, además del slot o flag efectivo.

La traza final 123 no basta para detectar un destructor adelantado dentro del
catch: una destrucción en ese punto podría conservar el mismo orden final. La
variante `check-frame-cleanup-checkpoint.py` exige que, al entrar al catch
interno, la traza sea todavía 1; después exige 123 y destrucción de excepciones
65. Se preparó como control adicional y falló al compilar en O0/O2 con el
diagnóstico SEH conocido (`check-frame-cleanup-checkpoint.log` y
`frame-cleanup-checkpoint/*`). No se alcanzó el checkpoint en ejecución.

Para una partición futura se proponen estas comprobaciones independientes:

1. **Antes de activar un owner:** fallo del inicializador de clase o de un
   valor con destructor personalizado, sin ejecutar destrucción de un objeto
   que no terminó de construirse; conservar el cleanup de lo que ya existía.
2. **Inicialización parcial:** `initializeAggregateLocalDeclaration` activa
   temporalmente la región de un agregado con drop y sin destructor custom
   mientras se llenan campos ya puestos en cero. Ante un fallo, deben
   destruirse sólo owners instalados; una regla de activar todos los cleanups
   únicamente al final no preserva esta ruta.
3. **Catch retenido:** el checkpoint anterior impide destruir `#3` dentro del
   catch interno y exige destruirlo al salir del finally, seguido por el owner
   exterior. El slot/flag se comparte con la ruta de excepción.
4. **Salida hacia el llamador:** exigir una vez cada acción del segmento
   interior y una vez cada acción exterior realmente abandonada, en orden;
   los handlers exteriores retenidos no deben ejecutar cleanup prematuro.
5. **Reemplazo y fallo del destructor:** identificar el dueño de la excepción
   previa, propagar la nueva según el contrato aplicable y verificar los
   guardas que se limpian antes de destruir. Las pruebas previas de trap para
   structs no autorizan suponer el mismo resultado en todos los tipos.
6. **Alias y estado de módulo:** las copias normal/excepcional deben acceder a
   los mismos slots, celdas y globals static que el código original; comprobar
   identidad y registro único, sin inferir liberación física ni ejecución de
   destructores globales a partir del IR.

Estos son criterios para diseñar pruebas, no una implementación ni una
demostración completa. Las salidas `return`/`break`/`continue`, las
continuaciones suspendibles y la excepción extranjera siguen requiriendo su
propio protocolo antes de ampliar la guarda.

`check-frame-partial-init.cmd` ejecutó controles nuevos de tupla, agregado y
constructor fallido, junto con variantes de cada uno dentro de finally/catch.
Los tres controles devolvieron 42 en O0/O2: comprueban dentro del catch los
órdenes 21, 21 y 1, respectivamente; el último detecta además una destrucción
indebida del objeto cuyo constructor no completó. Las tres variantes dentro
del finally fallaron al compilar en ambos niveles con el diagnóstico SEH y no
alcanzaron el checkpoint. Los logs y fuentes están en
`A:/nsc-fixes-evidencia/async-finally-20261111/check-frame-partial-init.log` y
`frame-partial-init/`. El compilador informó build hash `b91ecfaf473fc363`;
su SHA-256 consultado después fue
`5cc75d23c719b89d5caad0eb62d3011bbb4fe4d2b7e5e11d44fe89ee5878e0cc`;
no se registró un hash inmediatamente antes de esta corrida.

Los volcados MIR muestran que las sintaxis de tupla y agregado usadas aquí
convergen en `aggregate-initialize`; son dos reproducciones de superficie, no
prueba de dos causas independientes. En la variante dentro de finally, la ruta
del throw exterior sólo contiene `#0` (finally), y el local de agregado tiene
cleanup `#1` dentro de ese finally. El reparto debe incorporar la acción y el
estado parcial activados durante la inicialización, que no figuran en la ruta
original. No se ha demostrado todavía el comportamiento de estas variantes bajo
una implementación nueva del frame.

### `forceinline` y gate previo

El contrato exige expandir cada llamada estáticamente resuelta y conservar
efectos, ABI y scopes; el prototipo elimina la llamada directa al wrapper pero
mantiene una llamada al frame interno. No está demostrado que esta extracción
sea compatible con el contrato completo de `forceinline`, por lo que no puede
activarse para `Required` basándose sólo en ese prototipo. Excluirlo de una
primera guarda experimental mantendría el defecto pendiente para esos métodos;
emitir un diagnóstico tampoco equivaldría a implementar la capacidad.

Antes de modificar el emisor se necesita concretar la partición y restauración
para el caso seleccionado, preparar controles independientes de alias y
propiedad, comprobar cada salida y sus limpiezas, probar excepción extranjera
preservada y reemplazada, e inspeccionar el caller y los scopes para
`forceinline`. La prueba estática debe verificar identidad de storage,
inicialización única y un solo registro de destructor; su ejecución al cierre
requiere evidencia externa fiable. La prueba nativa existente y la ausencia de
una llamada directa no sustituyen esos gates.

### Modelo aislado de una guarda estrecha

`A:/nsc-fixes-evidencia/async-finally-20261111/seh-guard-candidate.py` es un
modelo Python exploratorio, no código del compilador. Con un árbol construido
manualmente acepta la forma del catch interno observada en el MIR y rechaza
políticas `Required`/`Preferred`, corrutinas, depuración, handlers y cleanups
externos, celdas propias, unwind de inicialización estática, referencias
externas, parámetros, declaraciones con cleanup, throw sin handler interno y
await. Sus 14 comprobaciones pasaron (`seh-guard-candidate.log`). El modelo no
lee MIR real, no verifica todo el estado del emisor y no demuestra que sea
seguro trasladar un finally o que la guarda esté completa; está prohibido
integrarlo como regla general por esos resultados.

Un adaptador diagnóstico posterior (`seh-guard-mir-check.py`) lee los volcados
textuales MIR ya conservados: reconoce como elegible la forma del finally
original y rechaza los subárboles de owner RAII, agregado parcial y constructor
fallido (`seh-guard-mir-check.log`). El adaptador sólo examina ese subárbol y
la política textual; no puede comprobar el contexto real de bloques, handlers,
capturas o depuración del emisor. No transforma el modelo en código C++ ni
demuestra la corrección de la partición o del snapshot.

Se consideró marcar `noinline` la función fuente de política automática y
extraer sólo su cleanup. La política automática documentada delega la decisión
al modelo target-aware; añadirle `noinline` podría sustituir esa política, por
lo que esa propuesta no está justificada. Una frontera física de frame interno
podría mantener la política de la función fuente, pero los experimentos todavía
no prueban que la transformación preserve semántica, scopes o la decisión de
optimización observada. La alternativa sigue sin implementarse.

### Reemplazo por excepción extranjera: riesgo y modelo LLVM

En `emitExceptionalCleanupChain`, la copia síncrona del finally guarda la
excepción NativeSharp original en `owner` y emite `replacementDispatch` para
una nueva excepción. Por inspección, el handler que libera `owner` utiliza el
filtro NativeSharp y el destino de unwind del `catchswitch` es `next`. Por ello,
una excepción extranjera que salga del finally **podría** omitir esa liberación.
Este es un riesgo no reproducido con un programa compilado por NSC; la ruta de
emisión y el resultado real deben comprobarse antes de declararlo defecto.

El control independiente `A:/nsc-fixes-evidencia/async-finally-20261111/
check-foreign-replacement-owner.cmd` ejecuta tres modelos LLVM escritos a mano,
no generados por NSC. En O0 y O2, el modelo cuyo filtro deja escapar la
excepción extranjera sin cleanup devolvió 93, indicando que no se ejecutó el
drop simulado. Un segundo modelo con `cleanuppad` para el owner devolvió 42:
la excepción extranjera llegó al filtro exterior y el drop se ejecutó una vez.
Un tercer modelo añadió otro cleanup exterior y devolvió 42, exigiendo la
secuencia 12 de drops. Comandos, salidas y advertencia sobre el triple de destino
están en `check-foreign-replacement-owner.log`, junto a las fuentes y binarios
de la carpeta `foreign-replacement-owner/`.

El drop del modelo es una función simple sobre enteros: no prueba la ABI de
destrucción del compilador, un destructor que lanza, capturas, ownership real,
reanudación async, inline ni código producido por NSC. El modelo tampoco
autoriza insertar el cleanup experimental en el emisor. Siguiente acción:
reproducir primero el camino con IR generado por NSC, verificando si se puede
inyectar la excepción extranjera sin modificar interoperabilidad, y comprobar
el orden de destrucción, la propagación y las rutas de error.

El control complementario `check-foreign-record-identity.cmd` devolvió 42 en
O0/O2 para un filtro exterior que observó el código y argumento `0x12345678`
de una excepción extranjera, el bit noncontinuable desactivado y la traza 123
tras un catch interno. La observación del registro ocurre durante la búsqueda
del handler, antes de completar el fallback: no acredita su identidad posterior
ni el comportamiento de NSC. El entorno añadió el flag 0x80, por lo que no se
impuso que todos los bits fueran cero. La evidencia y el script permanecen en
la carpeta indicada; ninguno modifica las fuentes de LLVM ni interoperabilidad.

#### Reproducción híbrida sobre IR generado por NSC

`foreign-owner-nsc/Main.ns` define una excepción con destructor que incrementa
`Drops`, un `Work` síncrono que la lanza y un finally que llama a `Fail`. El
compilador aislado generó el IR de la fuente, sin versión seleccionada, mediante
`foreign-owner-nsc-dump.cmd`; el resultado está en `foreign-owner-nsc/llvm.log`.
Ese IR confirma que `Work` guarda el owner en `eh.finally.owner`, invoca `Fail`
y dirige el unwind extranjero de `eh.finally.replacement.switch` al llamador,
mientras el release queda detrás del filtro NativeSharp.

`check-generated-owner.cmd` tomó una copia de ese IR, reemplazó el cuerpo de
`Fail` para generar una excepción extranjera y añadió un handler exterior de
observación; mantuvo intactos `Work` y la implementación generada del
destructor. En O0 y O2, la copia sin otra modificación devolvió 90: el handler
recibió la excepción extranjera y `Drops` fue 0. La copia alternativa
inyectó además un `cleanuppad` que vacía el slot y llama al destructor por el
descriptor; devolvió 42 en ambos niveles, exigiendo `Drops == 1`. El enlace
usó el runtime y las bibliotecas de importación aislados en
`A:/nsc-fixes-evidencia/build`; los comandos, advertencias y salidas completos
están en `check-generated-owner.log`. Se conserva el IR original y ambas copias
bajo `foreign-owner-nsc/`.

Es una reproducción híbrida: la fuente NativeSharp original no produce por sí
sola la excepción extranjera, el cuerpo de `Fail` y el handler exterior se
inyectaron en IR. La segunda copia es una prueba de mecanismo, no un parche del
compilador. Una tercera copia inyecta en el destructor generado una excepción
extranjera adicional después de incrementar `Drops`; el handler exterior acepta
las dos excepciones posibles, exige que la última sea la del destructor y que
`Drops == 1`. Esta variante devolvió 42 en O0/O2. Su handler debe aceptar la
primera excepción extranjera para que se inicie el unwind; una variante que
aceptaba sólo la del destructor terminó con código `0xE0000002` antes de
ejecutar el cleanup, por lo que no demostraba su comportamiento.

Ninguna variante demuestra múltiples cleanups exteriores reales, destructor
NativeSharp que lanza, propiedad de la nueva excepción, inlining, catch anidado
o async. Antes de modificar el emisor se deben probar esas rutas y coordinar la
generación de funclets y el estado compartido del backend.

El control híbrido se amplió para exigir además que
`__nativesharp_exception_current()` devuelva null al llegar al handler exterior.
La variante sin liberación devolvió 90 (`Drops == 0`); la que libera sin
limpiar TLS devolvió 91 (`Drops == 1`, pero TLS no era null); la que limpia TLS
antes de liberar devolvió 42. Otra variante inyectó un cleanup exterior y un
`invoke` de destructor desde el cleanup del owner, y exigió su ejecución una vez
incluso cuando el destructor produjo una tercera excepción extranjera: devolvió
42. Cada resultado se obtuvo en O0 y O2 y se conserva en el log citado. El
cleanup exterior también es inyectado y no demuestra una región real del
frontend o MIR.

El archivo `foreign-owner-emitter-proposal.diff` contiene una propuesta no
aplicada: dirigir el unwind extranjero del switch a un cleanup hermano, limpiar
TLS antes de destruir el owner y continuar por `next`. El borrador requiere
revisión de estado y pruebas con código generado por el compilador modificado;
no demuestra todavía que el destructor NativeSharp que arroja otra excepción,
las regiones exteriores reales ni los contratos de inlining se conserven.

#### Validación dirigida de la implementación candidata

El cambio en `OopLlvmCleanup.cpp` dirige el unwind extranjero del switch de
reemplazo a un cleanup hermano. Éste limpia TLS, destruye una vez el owner y
continúa el unwind por `next`. El handler de reemplazo NativeSharp conserva su
ruta anterior. El build incremental se ejecutó de forma coordinada en la
carpeta aislada de la campaña. El SHA-256 del compilador usado para las pruebas
fue `a2ed817580814531c63f0a117a5e875a3eeb151362925fb7b117f2b6da201675`.

`tests/integration/foreign_finally_owner.py` pasó doce combinaciones: O0/O2,
con y sin objeto `Guard` exterior, y tres situaciones durante la destrucción
del owner. La primera retorna normalmente; la segunda produce una excepción
extranjera inyectada en su IR; la tercera ejecuta un destructor generado desde
fuente NativeSharp, que provoca `Exception(3)` mediante un inicializador. Los
controles exigen la excepción esperada, TLS vacío y destrucción una vez; en los
casos con `Guard`, exigen además que éste se destruya una vez después del owner.
El handler del último caso comprueba el Id de la excepción NativeSharp, la
destruye y exige dos drops en total. En todos se obtiene IR nuevo de NSC, se
sustituye el cuerpo de `Fail` para producir una excepción extranjera y se añade
un handler SEH observador; el cleanup generado por NSC se mantiene intacto.
Comandos y salidas están en `A:/nsc-fixes-evidencia/async-finally-20261111/
foreign-finally-owner-regression.log`.

Los controles existentes `sync_preserve`, `sync_nested_preserve`, `sync_replace`
y `sync_call_replace` también devolvieron 42 en O0/O2, según
`check-native-replacement-control.log` en la misma carpeta. La excepción
extranjera es una inyección de prueba en IR y no una ejecución de fuente pura.
Estos controles no demuestran todas las combinaciones de inlining o catch
anidado, no resuelven `sync_inner_catch` y no demuestran que una corrutina pueda
suspender dentro de un funclet SEH.
