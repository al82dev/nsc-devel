# Arc y WeakArc: transferencia condicionada entre hilos

En `28d197dcc` se repitieron las comprobaciones de restricciones y capturas
de los punteros públicos. `Arc<Safe>` y `WeakArc<Safe>` se rechazan como
Send y Sync, y también como capturas de un ejecutor de hilos. Esto mantiene
cerrada una capacidad requerida, no demuestra un problema en los contadores.

La nueva regresión `tests/integration/shared_arc_thread_constraints.py`
comprueba los dos traits para Safe, Raw, SendOnly, las cuatro familias y
cargas incompatibles. Añade tipos homónimos `NativeSharp.Core.Arc` y `WeakArc`
en un proyecto llamado `NativeSharpCore`, con punteros crudos: no deben recibir
trato privilegiado por el nombre. Tanto desde fuentes como con el SDK binario
aislado se observaron cuatro fallos esperados de la funcionalidad pendiente
(`Arc<Safe>` y `WeakArc<Safe>` para ambos traits) y 22 controles correctos.
Las salidas están en `arc-thread-constraints-source-homonyms.log` y
`arc-thread-constraints-binary-homonyms.log`. La primera versión del fixture
desde fuentes no declaraba los traits y solo obtuvo `unknown type name`; se
conserva ese resultado en `arc-thread-constraints-current.log` y no se cuenta
como evidencia de Send/Sync. La versión corregida declara los traits locales;
la variante binaria usa los de Core.

`tests/integration/shared_arc_thread_capture.py` verifica la ruta separada de
capturas de `TaskScope` para las familias locales, atómicas, cargas Raw y
SendOnly. Desde fuentes y con SDK binario, los dos casos atómicos seguros se
rechazan indebidamente y los otros seis se rechazan como se esperaba. Registros:
`arc-thread-capture-source.log` y `arc-thread-capture-binary.log` en
`A:/nsc-fixes-evidencia`. Es una comprobación semántica: el executor del fixture
no ejecuta las tareas ni acredita concurrencia real.

Una excepción basada únicamente en el nombre sería insegura. El índice
`specializationForTypeKey` busca por nombre terminal; antes de una excepción
debe verificarse la identidad de la declaración, su procedencia y el argumento,
y comprobar Send y Sync de la carga en ambas rutas. También deben probarse
alias, transferencias y ejecución concurrente real. El compilador y SDK
utilizados proceden de builds aislados con cambios concurrentes sin registrar;
no certifican un árbol limpio. Las pruebas quedan fallando y aún no están
registradas en CTest. No se ha habilitado la transferencia ni se ha ejecutado
la batería completa.

La colisión no se limita al nombre terminal. En la prueba aislada
`A:/nsc-fixes-evidencia/arc-identity-counterfeit.py`, un proyecto productor
llamado `NativeSharpCore` declara `NativeSharp.Core.Arc<T>` con un campo
`int*`; otro proyecto, `Probe`, declara `Safe` y referencia al productor.
`--dump-project-generics` y `--check-project-method-bodies` terminaron con
salida 0. La especialización falsa y la del Core probado producen ambas la
misma clave `NativeSharpCore::NativeSharp.Core.Arc`1<type:Probe::Safe>` y el
mismo nombre `Arc__g1__Safe__h02d56f2d93bca6a9`. Las salidas respectivas
están en `arc-identity-counterfeit/dump-project-generics.log` y
`public-pointers-project-send-probe/dump-project-generics.log`, bajo la carpeta
de evidencia. Por tanto, ni la clave canónica ni el nombre del módulo bastan
por sí solos para reconocer una implementación segura. La prueba no ejecuta
una carrera ni demuestra que los campos de procedencia disponibles en otras
etapas sean suficientes; esa comprobación sigue pendiente.

La regresión dirigida incorpora ahora un productor y un consumidor separados
con esa misma forma, para `Arc` y `WeakArc` y para ambos traits. Las cuatro
imitaciones se rechazaron correctamente tanto en la ejecución desde fuentes
como en la ejecución con el SDK binario; cada ejecución conserva cuatro
fallos por el rechazo indebido de las familias atómicas reales y 26 controles
correctos. Evidencia: `arc-thread-counterfeit-source.log` y
`arc-thread-counterfeit-binary.log`. Ambos procesos terminan con código 1
porque la funcionalidad sigue pendiente.

La comprobación se amplió a una biblioteca precompilada falsa. El productor
en `arc-identity-binary/producer` contiene las dos estructuras con punteros
crudos; `--emit-project-oop-lib` generó la biblioteca y metadata en A. La
primera ejecución se rechazó porque el proyecto no declaraba `OutputType`;
se corrigió la configuración del fixture a `Library` y la emisión terminó con
código 0. El volcado del consumidor, `arc-identity-binary/dump.log`, muestra
para `Arc<Safe>` y `WeakArc<Safe>` las mismas claves y nombres especializados
que `public-pointers-project-send-probe/dump-project-generics.log`. Ser
precompilado tampoco basta para acreditar procedencia segura.

La regresión construye ahora esa biblioteca de control y comprueba ambos traits
para ambos tipos. En `arc-binary-counterfeit-source.log` y
`arc-binary-counterfeit-sdk.log` los cuatro casos precompilados se rechazaron
como se esperaba, con 30 controles correctos en total por ejecución. Los
cuatro rechazos incorrectos de `Arc<Safe>` y `WeakArc<Safe>` permanecen; por
ello las dos ejecuciones terminan con código 1. Los resultados no demuestran
que la clasificación por identidad deba implementarse ni que el uso
concurrente de las familias sea seguro.

## Contrato explícito investigado

El parser ya reconoce `unsafe impl` y guarda el indicador en el nodo. El
modelo `language::evaluateThreadSafety` contempla declaraciones inseguras con
una prueba revisada, pero la búsqueda de `unsafeSendImpl` en `src` e `include`
solo encontró la evaluación y la definición del dato: la comprobación de
constraints no establece ese dato a partir del impl. La prueba aislada
`arc-unsafe-contract-probe` confirma que `unsafe impl Send for Raw {}` pasa
sintaxis y aun así `Raw`, con puntero crudo, no satisface `Send`. Esto no
demuestra la semántica de todos los traits inseguros ni acredita una prueba
de seguridad de `Raw`.

En esa misma prueba, `unsafe impl<T> Send for Wrapper<T>` sobre una estructura
se rechaza durante la materialización, con y sin condiciones. Para una clase
genérica, la variante sin condiciones llega a la comprobación de `Send` y se
rechaza por el puntero; la variante con `where T : Send, Sync` se rechaza
antes, porque las condiciones de impl deben coincidir exactamente con las del
tipo. Las salidas están en `arc-unsafe-contract-probe/` y
`arc-unsafe-class-probe/`, bajo la carpeta de evidencia.

Una alternativa a confiar en la identidad del módulo sería una declaración
explícita e insegura del productor, condicionada por las propiedades de `T`.
Eso requiere definir y verificar cómo se representa la condición separada de
la del tipo, cómo se preserva en las especializaciones y la metadata, cómo se
comprueba en constraints y capturas, y qué significa la prueba revisada. Una
declaración de ese tipo traslada responsabilidad al autor del código inseguro;
no autentica por sí misma una biblioteca ni demuestra que `Arc` esté auditado.
No se ha habilitado esta alternativa ni modificado el comportamiento general
de `unsafe impl`.

Como comprobación acotada de publicación, `unsafe-impl-binary/build.cmd`
emitió una biblioteca mínima con una clase genérica y un `unsafe impl` sin
condiciones, sin caché de artefactos. La búsqueda de texto en su `.nsmeta`
encontró `unsafe impl<T> UnsafeProducer::Send for Wrapper<T> { }`. Esto prueba
que esa pieza textual está en el artefacto de ese fixture; aún no demuestra que
el indicador sobreviva en el modelo semántico del consumidor ni cubre una
implementación de estructura o una condición distinta de la del tipo.

La regresión `tests/integration/conditional_unsafe_thread_traits.py` conserva
una reproducción propuesta de ese contrato: un `Box<T>` con puntero crudo
declara `unsafe impl` de Send y Sync solo si `T` satisface ambos traits. Espera
aceptar `Box<Safe>` y rechazar cargas `Raw`, `SendOnly` y un contenedor sin
impl explícito, tanto desde fuentes como con una biblioteca. El compilador
posterior a `31476e234` rechaza los dos impl antes de evaluar cada caso porque
sus restricciones no coinciden con las del tipo. El productor binario se
rechaza por la misma razón. Registro:
`A:/nsc-fixes-evidencia/conditional-unsafe-thread-before.log`. Las ocho
comprobaciones desde fuentes y la construcción binaria fallan la prueba;
no se cuentan los casos negativos como controles aprobados porque no llegaron
a evaluar el tipo solicitado. Esta es una propuesta de semántica, no una
característica habilitada ni una garantía de seguridad de `Arc`.

El siguiente paso para esta alternativa es conservar la condición separada
del tipo sin tratarla como una implementación incondicional. La comprobación
debe asociar el contrato con la especialización por identidad de declaración,
evaluar Send y Sync de sus argumentos y aplicarlo tanto a constraints como a
capturas. El texto `unsafe` expresa responsabilidad del autor, pero por sí solo
no demuestra que los contadores, la vida del módulo o los préstamos de `Arc`
sean correctos. Se requiere esa revisión y ejecución concurrente antes de
exponer el contrato en las familias públicas.

En el árbol de trabajo hay un prototipo sin commit: conserva una forma
restringida de `unsafe impl` condicionado como extensión de la plantilla,
sin añadir el trait como base incondicional. El colector conserva también la
referencia al trait de esa extensión para publicación. No se ha implementado
su evaluación semántica: el productor binario del fixture ya compila, pero
`Box<Safe>` sigue rechazándose para Send y Sync desde fuentes y binario. Los
12 controles de cargas incompatibles o sin impl se rechazan con el diagnóstico
esperado; la prueba sigue terminando con código 1 por cuatro positivos
pendientes (`conditional-unsafe-thread-prototype.log`). La regresión existente
de impl genérico pasó (`conditional-unsafe-thread-controls.log`). La matriz
binaria de Arc conserva sus 30 controles correctos y sus cuatro positivos
fallidos (`conditional-unsafe-thread-arc-controls.log`). Los registros están
en `A:/nsc-fixes-evidencia`.

El prototipo reconoce nombres terminales solo para conservar esa forma de
sintaxis; **no concede Send ni Sync por esos nombres**. Es imprescindible
resolver la identidad real del trait y de la especialización al implementar
la evaluación, cubrir duplicados y formas no admitidas y revisar toda la
publicación antes de considerar el cambio terminado. Las modificaciones y
la prueba nueva siguen sin commit y no habilitan las familias públicas.

El ensayo posterior conecta la ruta de constraints con una consulta que parte
del nodo exacto de la especialización y exige identidades resueltas para el
trait implementado y los dos traits de la condición. La comprobación recorre
la carga bajo ambas propiedades y falla de forma conservadora si no puede
resolver un tipo. Una primera matriz pasó para carga simple desde fuentes y
binario (`conditional-unsafe-thread-identity.log`), pero la ampliación
`Box<Box<Safe>>` falló (`conditional-unsafe-thread-nesting.log`).

El volcado `conditional-unsafe-nested-identity/dump.log` muestra que ese
argumento se conserva inicialmente como escritura con origen. La traza
temporal `conditional-unsafe-nested-identity/trace.log` mostró que el nodo
del argumento no tenía enlace directo de nombres; se retiró la traza del
código y el ensayo busca ahora su clave de especialización y nodo exactos. La
matriz `conditional-unsafe-thread-expanded.log` pasó 48 comprobaciones
semánticas: 24 desde fuentes y 24 desde biblioteca, con escalares, wrappers
anidados, campos que contienen wrappers, carga con referencia a un tipo que
no es Sync y traits homónimos. Las pruebas existentes de campos anidados y
`impl` genérico también pasaron. El primer intento de
`send_mutable_reference.py` omitió el argumento del SDK y terminó con
`IndexError`; al pasarlo explícitamente, los 18 controles pasaron
(`conditional-unsafe-thread-existing-mut-fixed.log`). Todos los registros
están bajo `A:/nsc-fixes-evidencia`.

Esto sigue siendo un prototipo sin commit. Falta conectar y verificar la ruta
de capturas, revisar ciclos y cachés, evaluar la superficie de los propios
punteros y demostrar concurrencia y vida útil. La prueba semántica no demuestra
que las implementaciones declaradas unsafe sean seguras en ejecución. Las
familias públicas todavía no tienen este contrato.

El ensayo de capturas ahora consulta las identidades completas de los traits
de Core y recorre de forma iterativa ambas propiedades de la carga, con estados
visitados separados para Send y Sync. Solo utiliza ese recorrido para un tipo
que declare el contrato condicionado; los demás siguen la ruta anterior.
`conditional-capture-expanded-source.log` y
`conditional-capture-expanded-binary.log` registran diez controles cada uno,
incluidos ciclos de clases, punteros crudos, referencias a cargas que no son
Sync, tipos sin impl y traits homónimos. El executor del fixture no ejecuta
tareas: estos resultados son semánticos. Pasaron también los controles
existentes de herencia y la matriz de restricciones
(`conditional-capture-inherited.log` y `conditional-capture-constraints.log`).

Para probar las familias sin modificar todavía el SDK, los fixtures admiten
`--prototype-contract`: copian sus fuentes a temporales en A y agregan allí los
dos impl de Arc y WeakArc. Las restricciones pasaron con cargas simples y
anidadas (`arc-conditional-nested-constraints.log`). La primera prueba de
capturas anidadas rechazó indebidamente `Arc<Arc<Safe>>` y
`Arc<WeakArc<Safe>>` (`arc-conditional-nested-capture.log`). El volcado en
`arc-conditional-nested-inspect/` muestra varias especializaciones parecidas
con orígenes distintos; la búsqueda se acotó al rango de instanciación de la
especialización. La repetición `arc-capture-origin-check.log` pasó las doce
comprobaciones: cuatro aceptaciones y ocho rechazos. También volvió a pasar la
matriz genérica (`arc-capture-origin-contract.log`).

Estos son ensayos sin commit. El contrato se añadió solo a las copias de
prueba, no a las fuentes del SDK; aún faltan un Core binario con el contrato,
validación de cachés y ejecución concurrente y de vida útil. No se acredita
seguridad completa de Arc por la aceptación semántica.

## Core binario y ejecución dirigida

Se emitió `arc-conditional-sdk/NativeSharpCore.lib` y su metadata desde la
copia `arc-conditional-nested-inspect/Core`, que contiene los impl de prueba.
La primera emisión encontró la caché de compilación ausente y procesó 71
fuentes (`arc-conditional-sdk-build.log`). Una segunda emisión con la misma
caché informó `compilation-bundle-cache: hit`
(`arc-conditional-sdk-warm.log`); la salida también conserva estadísticas de
los artefactos de la emisión y no se interpreta como prueba de aciertos en
todas las capas de caché. Las matrices de restricciones y capturas pasaron
antes y después de recuperar el bundle: registros
`arc-conditional-sdk-{constraints,capture}.log` y
`arc-conditional-sdk-warm-{constraints,capture}.log`.

La regresión nueva `tests/integration/shared_arc_thread_runtime.py` enlaza ese
Core binario, comprueba `Send` y `Sync` de Arc y WeakArc y ejecuta dos hilos
nativos. Los hilos esperan una barrera atómica y cada uno realiza 1000 rondas
de clone, downgrade y upgrade con lectura del payload. La primera fase
comprueba conteos tras `Join`; en la segunda, el owner principal se libera y
los hilos liberan sus referencias, tras lo cual se observa expiración del débil
y una sola destrucción adicional. Compilación y ejecución dieron 42 en O0/O2
(`arc-conditional-runtime-traits.log`); los resultados anteriores de cada
ampliación se conservan en `arc-conditional-runtime.log` y
`arc-conditional-runtime-last-strong.log`. Los registros están en
`A:/nsc-fixes-evidencia`.

La barrera garantiza que ambos hilos se inicien antes de trabajar; no garantiza
un orden concreto ni cubre todas las intercalaciones. La prueba usa `Thread`
y un receptor de instancia cuya vida se conserva hasta después de `Join`;
no ejecuta un `IThreadExecutor` real ni demuestra todos los casos de
cancelación, alias, carga o descarga de módulos. El compilador y el Core de
prueba provienen de un árbol con cambios locales concurrentes. El contrato
sigue solo en la copia temporal y el prototipo permanece sin commit.

## Solo lectura, caché y carga estática externa

Con el Core binario de prueba, `shared_arc_conditional_readonly.py` comprobó
que los mismos tipos `Arc<Payload>` satisfacen Send y Sync, pero se rechazan
la escritura directa, el alias mutable de `Get`, el alias de `Borrow().Get`,
el almacenamiento no prestado del arreglo, la llamada al método mutador y el
escape a un campo. También se rechazó publicar `this` desde constructor y
destructor en O0/O2, y la lectura válida ejecutó 42 en ambos niveles. Los
diagnósticos están en `arc-conditional-readonly-diagnostics.log`; el rechazo
del arreglo sucede al intentar guardar el préstamo en un local no calificado,
antes de llegar a la escritura. No se considera prueba exhaustiva de alias.

Con el compilador aislado SHA-256
`40566c10872f48dd5c12909c68983b1f972493d3639ec65997e3c190502d41bd` y Core
binario SHA-256 `f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`,
`arc-readonly-current.cmd` pasó la selección dirigida ampliada. La escritura a
través de una expresión condicional o una conversión se rechazó por destino no
asignable; las lecturas equivalentes ejecutaron 42 en O0/O2. El log conserva
los diagnósticos y los controles restantes. Esta evidencia cubre Arc y esas
expresiones concretas; no demuestra todos los alias ni WeakArc.
El ejecutor registra ahora cada comando, código y salida; la selección se
repitió correctamente y el log permite identificar el compilador, el proyecto
de prueba y cada ejecución. No se cambió el comportamiento del compilador.

`shared_arc_conditional_cache.py` usó una caché nueva en A. La compilación
segura fría ejecutó 42, la repetición recuperó el bundle y ejecutó 42; cambiar
la fuente a `Arc<Raw>` rechazó el constraint sin hit. Al restaurar la fuente
segura hubo hit y salida 42. Después, en copias temporales, se reemplazaron
biblioteca, metadata y sidecar por la versión anterior sin contrato: la misma
fuente segura se rechazó sin hit. Restaurar la biblioteca con contrato volvió
a dar hit y salida 42. Registro `arc-conditional-cache-dependency.log` y
comandos, hashes y resultados en `arc-conditional-cache-dependency/`.
Esto cubre esa invalidación concreta, no todas las capas ni cambios de ABI.

`shared_arc_conditional_static_payload.py` compila una biblioteca estática
separada y consume sus cargas con Arc. Una clase sin destructor, otra con
destructor vacío y otra cuyo destructor escribe un campo compilaron y
ejecutaron 42 en O0/O2. La misma forma de destructor que incrementa un contador
estático ejecutó 42 cuando la carga se definió en el consumidor, pero se
rechazó al proceder de la biblioteca en ambos niveles con `shared payload class
has no verifiable precompiled lifecycle body`. Una carga precompilada cuyo
constructor publica `this` también se rechazó en ambos niveles como control.
La regresión permanece fallando por el destructor con contador; evidencia
`arc-conditional-static-payload-local-control.log`. Los casos vacío y de campo
no observan directamente que su destructor haya corrido. La acreditación
existente del productor reconoce solo un subconjunto de cuerpos MIR; no se ha
ampliado ni se ha relajado el rechazo del consumidor. Esta prueba usa enlace
estático y no demuestra retención durante descarga dinámica de módulos.

## Coherencia de implementaciones condicionadas

La matriz `conditional_unsafe_thread_traits.py --split-impl` distribuye el tipo
y sus cuatro declaraciones condicionadas en cinco archivos, cada impl con su
propio `using Safety`; otro namespace aporta traits homónimos. El compilador
aislado aceptó y rechazó según lo esperado las 48 combinaciones de marcadores y
cargas, tanto usando una referencia a fuentes como una biblioteca producida por
separado (`A:/nsc-fixes-evidencia/conditional-thread-split.log`). La variante
original de un archivo también pasó 48 comprobaciones
(`conditional-thread-unsplit.log`). Es evidencia semántica para esa disposición
de imports, no una prueba de ejecución concurrente ni de todos los ámbitos.
Los cambios de los impl condicionados siguen sin commit.

La prueba `conditional_unsafe_trait_coherence.py` reprodujo que un impl
condicionado seguido por otro impl del mismo trait se aceptaba, mientras que
traits distintos con el mismo nombre se rechazaban, incluso cuando se
resolvían mediante imports de archivos diferentes. El prototipo ahora compara
las identidades resueltas en el ámbito de cada impl y conserva el rechazo
cuando no puede demostrar una identidad única. Una ampliación de la prueba
mostró que un único impl para `Missing.Send` se aceptaba sin resolver ese
trait; la validación ahora se ejecuta también con una sola extensión.

Después de una compilación incremental de `nsc`,
`conditional-unsafe-coherence-after.log` registra siete controles aprobados,
incluido el trait inexistente. También terminaron con código 0 las matrices
`conditional-unsafe-thread-expanded.log`,
`conditional-capture-expanded-{source,binary}.log` y
`arc-conditional-sdk-{constraints,capture}.log`. Todos están en
`A:/nsc-fixes-evidencia`; el SDK binario es el de prueba descrito arriba. El
prototipo sigue sin commit ni incorporación al SDK público: estas pruebas no
resuelven el destructor precompilado ni acreditan vida útil de módulos.

Se añadió un octavo control que declara `Missing.Sync` como condición del
impl. Antes del cambio, `--check-project-generics` aceptaba esa declaración;
la validación ahora exige también una identidad única para cada condición del
impl diferido. Tras recompilar incrementalmente, los ocho controles pasaron,
incluida la comprobación del diagnóstico de identidad para los dos nombres
inexistentes. Las matrices de restricciones y capturas antes citadas volvieron
a pasar con fuentes y con el SDK de prueba. Esto comprueba resolución de esas
referencias, no la corrección de todas las implementaciones inseguras.

Dos controles posteriores reprodujeron aceptación de una clase llamada
`Send` como impl y de una clase llamada `Sync` como condición. La identidad
resuelta no bastaba para demostrar que la declaración fuera una interfaz: el
catálogo de predeclaraciones no conservaba su nodo. El prototipo conserva ese
nodo y, únicamente para el impl condicionado diferido, exige que la identidad
del trait y las identidades de las condiciones correspondan a interfaces.
No se requiere que el nombre del módulo sea privilegiado ni se decide la
seguridad de una implementación solo por ser `unsafe`.

La compilación incremental completó 154 acciones; por la dependencia de una
cabecera del frontend incluyó recompilar `CHeaderGenerator.cpp` dentro de ese
build aislado, sin modificar sus fuentes ni usar builds de interoperabilidad.
Los diez controles de coherencia pasaron con los diagnósticos correspondientes;
las matrices `conditional-unsafe-thread-expanded`,
`conditional-capture-expanded-{source,binary}` y
`arc-conditional-sdk-{constraints,capture}` volvieron a terminar con código 0.
Los logs están bajo `A:/nsc-fixes-evidencia`. El SDK de prueba y las demás
limitaciones descritas siguen vigentes; los archivos continúan sin commit.

Tras integrar los cambios de la laptop y la corrección de parámetros de
restricciones en `030ccc705`, se repitió la prueba dirigida de concurrencia
con `arc-after-laptop-regression.cmd`. La compilación terminó con código 0 y
el ejecutable devolvió 42 tanto en O0 como en O2. La salida está en
`A:/nsc-fixes-evidencia/arc-after-laptop-regression.log`. Se reutilizaron el
compilador incremental y el Core binario de prueba, no el SDK público: la
prueba conserva las limitaciones de concurrencia y de árbol local indicadas
arriba y no demuestra vida útil ante descarga de módulos.

## Ensayo con Core de la copia de trabajo

Se incorporaron provisionalmente los impl condicionados de Send y Sync a
`lib/NativeSharp.Core/Arc.ns` y `WeakArc.ns`: ambos requieren Send y Sync de
la carga. `arc-public-sdk-build.cmd` construyó el proyecto Core de la copia
directamente en `A:/nsc-fixes-evidencia/arc-public-sdk/`; procesó 71 fuentes
con caché nueva y terminó con código 0. El SHA-256 del compilador fue
`3cdea1150ae607fa420d1f0fe40476cb310de87cfed606122d91570ab055218b`, el de
`NativeSharpCore.lib` fue
`f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0` y el de
su metadata fue
`f8e4189f7f35f457c61550b015ba001815e76ec0337973add8cc810f9aad669c`.

Las matrices de restricciones y capturas pasaron tanto desde las fuentes
reales como con ese Core binario, incluyendo cargas incompatibles, Rc/WeakRc y
tipos homónimos (`arc-public-{source,sdk}-{constraints,capture}.log`). La
prueba de solo lectura rechazó las mutaciones y escapes cubiertos y ejecutó el
control válido en O0/O2 (`arc-public-sdk-readonly.log`). Las pruebas de dos
hilos y de upgrade frente a liberación final compilaron y devolvieron 42 en
O0/O2 (`arc-public-sdk-{runtime,race}.log`); sus limitaciones de planificación
de hilos son las descritas arriba. La emisión de biblioteca informó una etapa
de cabecera automática en la carpeta aislada; no se modificaron fuentes ni
builds de interoperabilidad.

Los impl y el soporte de compilador siguen sin commit y deben considerarse
provisionales. El Core se construyó con todo el árbol local, incluido el cambio
concurrente de `DeterministicMath.ns`, por lo que no certifica una revisión
limpia. Los casos de lifecycle binario y vida de módulos descritos siguen
pendientes; estas pruebas no demuestran seguridad exhaustiva de Arc.

La regresión de caché se repitió con el Core de la copia, en una caché nueva
en A. La carga segura ejecutó 42 en frío y con bundle reutilizado; al cambiar
la fuente por una carga con puntero crudo se rechazó sin hit. Al sustituir en
copias temporales el Core por el binario anterior sin contrato, la carga segura
también se rechazó sin hit; al restaurar el Core con contrato volvió a
ejecutar 42 con hit. Comandos, hashes y resultados están en
`A:/nsc-fixes-evidencia/arc-public-cache-results/` y `arc-public-cache.log`.
Esto cubre esa invalidación concreta de bundle, no todas las capas de caché ni
todos los cambios binarios. Los cambios siguen provisionales y sin commit.

La regresión `shared_arc_conditional_static_payload.py` se repitió con este
Core (`arc-public-static-payload.log`). El productor compiló; las cargas sin
destructor, con destructor vacío o con escritura de campo ejecutaron 42 en
O0/O2, al igual que el control cuyo destructor contador está en el consumidor.
El constructor que publica su receptor se rechazó como se esperaba. El
destructor contador definido en la biblioteca siguió rechazándose en O0/O2
con `shared payload class has no verifiable precompiled lifecycle body`; por
ello la regresión termina con código 1. La limitación sigue abierta. Las
ejecuciones de los destructores vacío y de campo no observan directamente que
se hayan invocado.

La reproducción conservada en `arc-binary-lifecycle-ir/` muestra el MIR del
destructor con contador: `Store` a `StaticFieldAddress` de `int`, con `Binary`
`Plus` sobre `StaticFieldLoad` y una constante. El constructor que publica el
receptor también usa `StaticFieldAddress`, pero almacena `ThisValue` con tipo
de clase. El analizador conservador `isSimpleNonEscapingLifecycle` admite el
store a un campo escalar del propio receptor, pero no el store estático ni la
expresión aritmética; por eso no acredita el primer cuerpo. El catálogo se
consulta al construir cargas compartidas binarias y al comprobar bases, y su
productor se encuentra en un archivo compartido bajo `src/Interop`. No se
modificó esa ruta. Admitir indiscriminadamente stores estáticos sería inseguro
porque también podría permitir publicar `this`; una eventual extensión tendría
que demostrar que el valor almacenado no contiene el receptor y preservar el
rechazo del constructor de control. Además, el incremento escalar del ejemplo
no demuestra seguridad ante destrucciones simultáneas de instancias distintas.
No se ejecutó esa posible carrera ni se considera corregido el caso.

## Carga de una biblioteca dinámica NativeSharp

`arc-dynamic-payload.cmd` adaptó fuera del repositorio la regresión de carga
estática existente: emitió `PayloadProducer.dll` y consumió su biblioteca de
importación y metadata desde un ejecutable NativeSharp. La DLL se copió junto
al ejecutable en el directorio temporal de A. El productor compiló y, en O0
y O2, las cargas sin destructor, con destructor vacío y con escritura de un
campo ejecutaron con resultado 42. El control con destructor que incrementa
un contador local al consumidor también ejecutó 42; el constructor que publica
su receptor fue rechazado en ambos niveles.

El destructor con contador estático definido dentro de la DLL se rechazó al
compilar el consumidor en ambos niveles con `shared payload class has no
verifiable precompiled lifecycle body`, igual que en el ensayo estático. Por
ello la prueba completa termina con código 1. La salida está en
`A:/nsc-fixes-evidencia/arc-dynamic-payload.log`; el adaptador y su alcance
están en la misma carpeta. La prueba usa el Core binario de prototipo y un
árbol con cambios locales. No observa directamente los destructores vacío y
de campo, no fuerza la descarga de la DLL y no demuestra que sea seguro
conservar un puntero después de descargar el módulo. No se modificó el fixture
original ni se usó interoperabilidad C/C++.

Un segundo ensayo, `arc-cross-module.cmd`, cambia la frontera: una fábrica
en una DLL NativeSharp construye y devuelve `Arc<Payload>`, y el ejecutable
lo lee, clona, convierte a `WeakArc` y comprueba conteos y expiración después
de liberar el último dueño. La DLL se compiló en O0; los consumidores O0 y O2
compilaron y ejecutaron con resultado 42. Comandos, salida y las fuentes de
ambos módulos se conservan en `A:/nsc-fixes-evidencia/arc-cross-module.log` y
`A:/nsc-fixes-evidencia/arc-cross-module/`. Se usó el Core de prototipo con
los contratos condicionados, con la DLL presente junto al ejecutable. Este
ensayo verifica esa ruta de propiedad mientras el módulo permanece cargado;
no prueba descarga dinámica, concurrencia entre módulos, todas las cargas ni
un productor optimizado en O2. El compilador procede del árbol conjunto con
cambios locales, por lo que tampoco certifica una revisión limpia.

Inspección posterior de esos dos ejecutables conservados: `llvm-readobj
--coff-imports` muestra una importación PE ordinaria de
`ArcBoundaryProducer.dll` y de su fábrica en O0 y O2
(`A:/nsc-fixes-evidencia/arc-cross-module-imports.cmd` y
`arc-cross-module-imports-O0.log`, `arc-cross-module-imports-O2.log`). Los
SHA-256 de los ejecutables inspeccionados son respectivamente
`3b167d2a9abf8ed98187de9a23f66d1c1fde91a9396cf02171709c7a036bd431` y
`301ffb1bf7f54182cc32566ae7b2d345ec35839e059f2d9674b4853e0a3bb9be`.
Esto identifica una dependencia de carga del ejecutable hacia la DLL para ese
ensayo; no demuestra que el bloque de control retenga el módulo ni prueba una
descarga solicitada mientras sobrevive una referencia fuerte o débil. La
investigación de esa vida independiente permanece abierta y no se ejecutó un
ensayo que invoque código después de descargar el módulo.

Para ampliar esa frontera, `arc-cross-module-threads.cmd` reutilizó la misma
DLL y entregó a dos hilos del consumidor clones del `Arc` creado por la
fábrica de la DLL. Después de una barrera, cada hilo realizó 1000 ciclos de
clonación, degradación, recuperación y lectura; ambos soltaron sus dueños y
el consumidor comprobó expiración y conteo de la referencia débil. Los
consumidores compilaron y devolvieron 42 en O0 y O2. Fuentes y registros están
en `A:/nsc-fixes-evidencia/arc-cross-module-threads/` y
`A:/nsc-fixes-evidencia/arc-cross-module-threads.log`. La barrera no controla
todas las intercalaciones; se usó una DLL compilada en O0, el módulo siguió
cargado y el payload no contabiliza destrucciones. Se mantiene la limitación
de Core de prototipo y árbol local conjunto.

La nueva prueba `tests/integration/shared_arc_upgrade_release_race.py` inicia
dos hilos tras una barrera: uno libera el último `Arc` inicial y el otro
intenta recuperar un `WeakArc` 10000 veces. Una recuperación exitosa verifica
el valor; tras unir los hilos se comprueba expiración, una destrucción y que
una recuperación posterior queda vacía. Compiló y ejecutó con salida 42 en
O0 y O2; comandos y salidas están en
`A:/nsc-fixes-evidencia/arc-upgrade-release-race.log`. La barrera no garantiza
que se observe una recuperación exitosa ni una intercalación concreta con la
liberación, y la prueba no sustituye el análisis del algoritmo atómico. El
compilador y Core siguen siendo los del prototipo y árbol conjunto.

## Actualización de la carga binaria con contador

El rechazo histórico de los apartados anteriores se corrigió en el commit local
`b19cff442`, con un reconocimiento limitado de la forma escalar y sin acreditar
la publicación del receptor. La regresión estática y su variante `--dll` ahora
comprueban Rc y Arc en O0/O2 y observan el contador del destructor; las formas
que publican el receptor siguen rechazándose. Evidencia y límites en
`docs/fixes/metadata-lifecycle-no-escape.md` y
`A:/nsc-fixes-evidencia/arc-dynamic-lifecycle-regression.log`. La DLL se mantiene
cargada; la seguridad de descarga dinámica y del contador global concurrente no
queda demostrada. Los impl condicionados de Arc y WeakArc siguen provisionales.

## Promoción coordinada antes de liberar el dueño

La prueba `shared_arc_upgrade_release_race.py` conserva la carrera original y
añade una segunda fase coordinada: un hilo promueve el débil y mantiene el
`Arc` resultante; solo después el otro suelta el dueño original. El hilo que
promovió comprueba que el objeto sigue accesible y que el débil no expiró, y
entonces suelta el último fuerte. Tras `Join`, se comprueba una sola destrucción
de esa carga, expiración y que una promoción posterior produce un propietario
vacío. `A:/nsc-fixes-evidencia/arc-upgrade-coordinated-regression.cmd` compiló
y ejecutó ambas fases con resultado 42 en O0 y O2; los comandos y salidas están
en `arc-upgrade-coordinated-regression.log`.

El ejecutable del compilador aislado tuvo SHA-256
`9a2f389db71eaae6e87e5a00dc76380a5cb63acdbe20370d29268d4efe07b89c` y el Core
de prueba `f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`.
La sincronización demuestra ese orden concreto, no todas las intercalaciones;
la carrera original sigue sin garantizar que observe una promoción exitosa.
El compilador y Core incluyen cambios locales provisionales y esta prueba no
demuestra seguridad ante descarga de módulos ni certifica la implementación.

## Solo lectura de un objeto anidado

Con el Core binario aislado se comprobó una carga con un campo de clase `Child`.
La regresión `shared_arc_conditional_readonly.py` exige rechazar la escritura
directa del campo anidado, una llamada que lo modifica, su paso a un método que
puede modificarlo y su copia a un local sin calificador de préstamo. Los cuatro
casos se rechazaron; la lectura a través de `Borrow().Get()` devolvió 42 en O0 y
O2 (`A:/nsc-fixes-evidencia/arc-readonly-nested-regression.log`). También pasaron
los controles anteriores de la misma selección. Una exploración independiente
intentó declarar el alias de `Child` como `borrowed`, pero el compilador rechazó
esa inicialización por no ser un origen directo admitido
(`arc-readonly-nested-borrowed-probe.log`); no demuestra que se permita usar
ese alias en modo solo lectura. El resultado usa el árbol provisional conjunto
y no acredita todas las vías de alias o métodos anidados.

Actualización: la limitación de ese sondeo quedó atendida posteriormente por
`6767fc4d0` para las procedencias detalladas en
`readonly-nested-field-borrows.md`. Al repetir el caso histórico con el
compilador aislado actual, la declaración del alias se acepta y la escritura
se rechaza en `child.Data` (`arc-readonly-nested-borrowed-current.log`). Un
control independiente, sin cambiar el ejemplo anterior, compiló y leyó 42 en
O0 y O2 desde `owner.Get().Item` con el Core binario aislado
(`arc-readonly-nested-current/run.cmd`, salida 0; `build-O0.log` y
`build-O2.log`). El equivalente con Rc también devolvió 42 en O0/O2 y su
mutación se rechazó en `child.Data` (`rc-readonly-nested-current/run.cmd` y
`mutation.log`). Los controles usan los cambios locales conjuntos y el Core
ya generado; no demuestran todas las procedencias ni la seguridad completa de
las familias.
