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
