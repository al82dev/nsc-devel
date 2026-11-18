# Diseño autorizado: punteros compartidos y AddressSanitizer

Este documento conserva el diseño y la evidencia. Los punteros compartidos
siguen pendientes; ASan cuenta con la ruta limitada descrita más abajo.

## Rc, WeakRc, Arc y WeakArc

La prueba dirigida `move_assign_distinct_blocks` transfiere un fuerte entre dos
bloques distintos para las familias local y atómica. Comprueba la destrucción
de la carga sustituida, la expiración de su débil, la conservación y upgrade
del bloque transferido, y la destrucción posterior de su carga. Ejecutó 42 en
O0/O2 con Rc/WeakRc y Arc/WeakArc; los controles de fuertes vacíos y clonación
de un débil expirado también pasaron. Evidencia:
`A:/nsc-fixes-evidencia/shared-move-assignment-lifecycle.log`. La prueba usa
fuentes del SDK en un solo hilo; no mide directamente la liberación del bloque
de control ni acredita el consumo binario o la seguridad concurrente pública.

La inspección `A:/nsc-fixes-evidencia/allocation-inspect` usa copias de las
fuentes públicas actuales y crea Rc y Arc para una clase y un struct con
constructores triviales. El IR contiene una llamada a `__nativesharp_alloc`,
en `SharedControlStorage.AllocateLayout`; las cuatro especializaciones de
fábrica llaman a su ruta de reserva y después a la construcción placement.
La construcción de clase escribe el descriptor y llama al constructor sobre
la dirección del bloque; la de struct inicializa y construye esa misma región.
El runtime Windows observado implementa `__nativesharp_alloc` con una llamada
a `HeapAlloc`. El programa terminó con 42 en O0/O2 (`results.txt` y logs de
build). Esta evidencia estática es compatible con una sola reserva del bloque
para esas cargas, pero no cuenta reservas dinámicas, no cubre constructores que
reservan otros recursos ni acredita todas las rutas excepcionales o el consumo
binario. Los volcados solicitados sin nivel y con `-O2` son idénticos; no se
afirma haber inspeccionado IR optimizado distinto ni mediciones de heap.

### Investigación de limpieza de bases durante construcción fallida

**Comprobación posterior con el árbol actual.** La reproducción histórica
`shared_nontrivial_base_throw.py` primero se detuvo durante el análisis porque
su proyecto mínimo no declaraba `Send` y `Sync`, necesarios para las
implementaciones condicionales actuales de Arc y WeakArc
(`A:/nsc-fixes-evidencia/shared-throwing-base-current-{Rc,Arc}.log`). Se
añadieron esas dos declaraciones de prueba junto a `Exception`; la rama de
consumo binario elimina las tres para utilizar las del SDK. Se conservaron los
casos y las expectativas de traza existentes.

Al repetir con el compilador aislado SHA-256
`f0948d234daf92ecb107c680974db4c5f0294a2d87b23c7e199b288b2739f2b1`,
Rc y Arc pasaron análisis y ejecutaron con salida 42 en O0 y O2, desde fuentes
(`shared-throwing-base-traits-{Rc,Arc}.log`) y contra
`A:/nsc-fixes-evidencia/pointers-sdk-refresh/NativeSharpCore.lib`
(`shared-throwing-base-binary-{Rc,Arc}.log`). Los comandos están en los
archivos `shared-throwing-base-current-check.cmd` y
`shared-throwing-base-binary-current.cmd` de esa misma carpeta de evidencia.
El SDK binario se generó previamente desde la copia aislada, con otro
compilador; sus hashes figuran en `pointers-sdk-refresh-scope.md`.

Esta comprobación posterior contradice el rechazo histórico descrito más
abajo para el fixture completo: con este árbol y estos artefactos las trazas
esperadas del fixture se alcanzaron. No identifica qué cambio posterior
eliminó aquel rechazo ni demuestra todas las formas de construcción, la
liberación física del bloque o el comportamiento de una segunda excepción
durante cleanup. El ajuste del fixture está sin commit; no se modificó la
protección del compilador en esta comprobación.

La reproducción se registró después como
`nsc.sdk.shared-nontrivial-base-throw` y se añadió `--no-artifact-cache` a sus
emisiones. Tras regenerar la configuración y compilar `nsc` incrementalmente
(`shared-throwing-base-registered-build.log`), la prueba registrada pasó para
Rc y Arc en O0/O2 (`shared-throwing-base-registered-ctest.log`). El mismo
fixture sin caché pasó contra el Core binario aislado previamente generado
(`shared-throwing-base-binary-no-cache.cmd` y `.log`). El SHA-256 del
compilador de esta repetición fue
`0d48e6a9098d9d24a33be2cc1e4a9d909854c7e92906675e32411b517609fe73`.
Estos registros están en `A:/nsc-fixes-evidencia`; el Core binario no se
regeneró durante esta repetición. La prueba registrada cubre fuentes y el
control binario se ejecuta por separado. No cambian los límites de cobertura
del párrafo anterior.

La selección `shared-constructor-regression-current.cmd` verificó el prototipo
actual en 17 casos para Rc y Arc en O0/O2: las 68 ejecuciones terminaron con 42
(`shared-constructor-regression-current.log`). Incluye transferencia y orden
de campos owned, fallo durante reasignación, bases implícitas completas e
incompletas y campos struct con drop. Esta evidencia procede de las fuentes del
SDK y del compilador incremental, y no cubre las fábricas throwing, el inicio
de vida de campos predeterminados ni todas las formas de construcción. Los
cambios de cleanup permanecen sin commit.

Cinco regresiones dirigidas combinan los punteros compartidos con tuplas:
inicialización completa, excepción al construir un elemento posterior, tupla
anidada con dos propietarios y destrucción en orden inverso, y éxito o fallo
de un `await` que se suspende después de crear el propietario. En la ruta async
exitosa se comprueba que no se recrea ni destruye prematuramente el payload.
Las 20 ejecuciones de Rc/Arc O0/O2 pasaron
(`shared-tuple-owner-async-expanded.log`). Usan SDK desde fuentes y el backend
provisional; no cubren cancelación ni el fallo separado de `await` en finally.

El control `tuple_shared_owner_cancel` avanza un iterador async hasta entregar
un valor, después de construir una tupla con Rc o Arc y completar un await que
se suspende. Comprueba que `Cancel()` no destruye inmediatamente el payload y
que el siguiente `MoveNextAsync()` lo destruye una vez. Pasó en O0/O2 para
ambas familias junto a los controles de await exitoso y fallido
(`shared-tuple-owner-cancel.log`). No cancela mientras el await hijo sigue
pendiente ni cubre abandono del iterador o el fallo de await dentro de finally.

Dos controles posteriores ejercen `Drop()` explícito del iterador: antes de
iniciarlo no construye ni destruye el payload; después del primer valor destruye
una vez el propietario contenido en la tupla. Ambos y el control de cancelación
pasaron para Rc/Arc O0/O2 (`shared-tuple-owner-drop-final.log`). No cubren un
abandono mientras el future hijo está suspendido, `Drop` repetido ni liberación
física del bloque compartido.

Para investigar abandono durante un await hijo pendiente se probó invocar
`Poll()` sobre el future obtenido de `MoveNextAsync()`: el análisis lo rechaza
porque ese tipo no admite acceso a miembros (`manual-poll-probe/inferred.log`).
Un control con el mismo future y `await` pasó el análisis
(`manual-poll-control/check.log`). El backend documenta que el código fuente
usa await para avanzar futures, pero estas comprobaciones no demuestran que no
exista ninguna otra forma de programar una suspensión parcial. No se ejecutó
código con acceso manual al frame ni se modificó el contrato de cancelación.

`normal_factory_partial_owned_order` compara el orden de limpieza excepcional
de dos campos owned de una clase `partial` con el observado al destruir
normalmente una clase equivalente. Los dos fragmentos de cada clase están en
un mismo archivo; no acredita el orden de partes distribuidas entre archivos.
El caso y el control no parcial pasaron para Rc y Arc en O0/O2
(`shared-partial-order-check.cmd` y `.log`).
El control `normal_factory_partial_files_owned_order` distribuye los campos
entre `Main.ns` y `PartialFields.ns` y compara también con la destrucción
normal de una clase equivalente. Pasó junto a los controles anteriores para
Rc y Arc en O0/O2 (`shared-partial-files-check.cmd` y `.log`); la prueba cubre
esa combinación de archivos y orden de proyecto, no todos los órdenes posibles.

La selección `shared-delegating-control.cmd` comprueba una fábrica normal con
un constructor `this(...)`: fallo dentro del constructor delegado, fallo en
el cuerpo que delega después de regresar aquel, excepción capturada localmente
y salida normal. En la primera forma comprueba que el recurso se destruye una
vez y que el objeto incompleto no ejecuta su destructor; en las demás comprueba
la vida y destrucción del recurso. Las 16 ejecuciones de Rc/Arc O0/O2 pasaron
(`shared-delegating-control-outer.log`). El caso que falla después de delegar
no declara destructor propio: no decide si corresponde ejecutarlo, ni cubre
bases, fábricas throwing, construcción ordinaria o todas las cadenas delegadas.

El MIR de `A:/nsc-fixes-evidencia/throwing-base-probe/current-mir.log`
representa el constructor derivado como una llamada directa a la base seguida
del `throw`, sin región de limpieza para la base (`cleanup-contract=0`). El LLVM
correspondiente confirma que la llamada de base retorna antes de propagarse la
excepción y que no se llama al destructor de la base en esa ruta.

Con el prototipo posterior sin commit y el compilador incremental con hash
`bc4d89b727068dcb`, las reproducciones ordinarias existentes de base explícita
y de base intermedia implícita pasaron en O0 y O2, con salida 42 en las cuatro
ejecuciones (`A:/nsc-fixes-evidencia/constructor-explicit-current.log`). La
segunda comprueba la traza 53421. Se conservó una regresión reproducible en
`tests/integration/constructor_throwing_base_cleanup.py`; la selección
`nsc.compiler.constructor-throwing-base-cleanup` pasó 1/1, con las cuatro
ejecuciones registradas en `constructor-throwing-base-ctest.log`. Asimismo, la
selección `safe` de la API pública pasó Rc y Arc en O0/O2, incluidos los casos
throwing simples (`shared-throwing-basic-current.log`). Estas verificaciones no
habilitan las fábricas throwing para bases no triviales: su protección continúa
activa y el prototipo conserva los límites de construcción parcial documentados
en este archivo.

Ensayo diagnóstico posterior: se desactivó temporalmente el rechazo de
`sharedThrowingConstruction && constructorThrows && !safePartialCleanup`, se
compiló incrementalmente `nsc` en el build aislado y se ejecutó la reproducción
`tests/integration/shared_nontrivial_base_throw.py` por separado para Rc y Arc.
Las dos familias pasaron análisis y ejecutaron 42 en O0 y O2, incluyendo las
formas preparadas de bases explícitas e implícitas, fallo en la base, excepción
capturada, construcción satisfactoria y delegación
(`A:/nsc-fixes-evidencia/throwing-guard-experiment.log`). Este resultado solo
acredita los casos del fixture: no demuestra campos de valor con vida parcial,
layouts explícitos, bases precompiladas ni segundas excepciones al limpiar.
Después se restauró la condición original, se recompiló incrementalmente `nsc`
y se confirmó que Rc y Arc vuelven a rechazarse en O0 con el diagnóstico de
protección (`throwing-guard-restored-check.log`, hash `f7706addf7b7bfb9`). No
quedó habilitado el rechazo desactivado y no se guardó un binario experimental
aparte. El siguiente paso es delimitar y demostrar qué formas puede limpiar el
constructor antes de sustituir ese rechazo por un criterio más preciso.

Un control posterior mostró por qué el ensayo positivo no basta para hacerlo:
`helper_owned` inicializa un campo `owned` llamando a un método de instancia y
después lanza desde el constructor derivado. La construcción ordinaria con el
compilador de trabajo ejecutó 20 en O0 y O2: el código del fixture representa
base destruida una vez y recurso no destruido; se esperaba 42 para ambos drops.
La base compilada por separado ejecutó 10 en ambos niveles, es decir, ninguno
de los dos drops (`A:/nsc-fixes-evidencia/constructor-helper-baseline.log`).
El selector `nsc.compiler.constructor-throwing-base-cleanup` ahora incluye la
reproducción con expectativa de éxito y falla deliberadamente en ese caso;
los casos explícito e implícito anteriores aún ejecutan 42 en ambos niveles
(`constructor-throwing-base-ctest.log`). En `prepareFields`, el prototipo
recorre stores del cuerpo MIR del constructor y no inspecciona el cuerpo del
método llamado; esto explica que no registre ese campo, pero aún debe
contrastarse con la generación LLVM antes de elegir la corrección. La prueba
ordinaria no demuestra por sí sola el comportamiento de la fábrica que sigue
protegida. No se modificó el backend en esta comprobación.

La variante `normal_factory_base_owned_helper` prueba directamente las fuentes
públicas: un método inicializa el campo owned de la base y el siguiente acceso
a una clase estática lanza antes de completar el constructor base. Rc y Arc
capturaron la excepción sin destruir el recurso: las cuatro combinaciones O0/O2
devolvieron 10 y esperaban 42 (`A:/nsc-fixes-evidencia/shared-owned-helper-run.log`).
La selección conserva la expectativa correcta y falla. `prepareFields` solo
encuentra asignaciones presentes en el MIR del constructor; la escritura en
`Initialize` está en otro cuerpo. El almacenamiento placement se limpia a cero
cuando `node->zeroInitialize` está activo, pero el constructor también puede
llamarse en otros contextos: ese dato por sí solo no permite tratar todas las
ranuras como inicializadas ni ejecutar después el destructor de una base ya
destruida. Se mantiene la protección throwing y aún falta una solución que
distinga esos estados sin duplicar drops.

El ensayo posterior registra para constructores de clase los campos de
referencia `owned` del layout, incluidos los que solo se escriben desde otro
método. El almacenamiento de las construcciones placement se pone a cero en
MIR; el plan de asignación ordinaria también indica inicialización a cero. Por
eso esos campos se registran como activos desde el comienzo y una ranura nula
no destruye ningún objeto. Los campos de valor con drop conservan su indicador
de vida anterior; los indicadores de una base implícita se desactivan al
completarla, como en el prototipo existente. El cambio está en
`src/LLVM/Lowering/OopLlvmStatements.cpp` y continúa sin commit.

Con el compilador incremental `87dc014b74d83e90`, la reproducción pública y
dos controles nuevos (excepción dentro del método y salida normal) pasaron en
Rc y Arc O0/O2. También pasaron el campo no escrito, la rama omitida, el bucle
y el caso público básico: 28 ejecuciones en total
(`A:/nsc-fixes-evidencia/shared-owned-helper-controls.log`). La selección previa
de 17 casos de reasignación, orden, transferencia y bases implícitas pasó sus
68 ejecuciones (`shared-owned-helper-regression.log`); la regresión ordinaria
pasó sus seis ejecuciones (`constructor-throwing-base-ctest.log`). Las salidas
anteriores de `shared-owned-helper-run.log` y `constructor-throwing-base-ctest.log`
fueron reemplazadas al repetirlas; los resultados previos quedan descritos en
los párrafos anteriores, pero esos dos archivos muestran la pasada posterior.
Esto no acredita campos de valor inicializados indirectamente, constructores
precompilados, layouts explícitos ni las fábricas throwing con bases no
triviales, cuyo rechazo permanece activo.

La selección `shared-owned-helper-controls.log` se amplió posteriormente con
transferencia y reemplazo del campo desde métodos. Ambos casos pasaron para Rc
y Arc en O0/O2; la selección ampliada contiene 36 ejecuciones, todas con salida
42. La repetición sustituyó el contenido anterior de ese mismo log. El control
`nsc.item319.explicit-object-destruction` pasó sus 27 comprobaciones
(`A:/nsc-fixes-evidencia/constructor-destruction-control.log`). Estos controles
no demuestran aún todas las formas de alias del campo ni el contrato de las
fábricas throwing con bases no triviales.

Una reproducción posterior cambió el constructor de un campo struct `Part`
para inicializar su campo owned llamando a `Initialize()` y fallar después.
Antes del ajuste, la selección se detuvo en Rc O0 con salida 3: se capturó la
excepción pero `Resource.Drops` seguía en cero. El registro de aquella primera
ejecución fue sustituido al repetir el comando; el resultado se observó
directamente. Se amplió el registro de campos de referencia owned al layout de
estructuras ordinarias, excluyendo expresamente uniones y offsets explícitos.
El indicador inicialmente activo para referencias nulas se limita también a
clases y estructuras sin solapamiento; los valores con drop mantienen su
indicador anterior.

Después de compilar incrementalmente el compilador con hash
`7c969cf811f592e3`, los cinco casos dirigidos de estructura —fallo después y
dentro del método, salida normal, fallo parcial y orden de cleanup— pasaron para
Rc y Arc en O0/O2: 20 ejecuciones (`struct-owned-helper-controls.log`). La
selección de nueve controles de clases volvió a pasar sus 36 ejecuciones
(`shared-owned-helper-controls.log`), ambos en `A:/nsc-fixes-evidencia`.
No se ha acreditado el tratamiento de uniones, offsets explícitos ni campos
de valor con drop inicializados indirectamente. El ajuste sigue sin commit.

También se probó `Part` como carga directa de `Rc` y `Arc`, con un campo owned
asignado desde un método. Se comprobaron excepción implícita, constructor que
declara y propaga `throws` mediante la fábrica correspondiente y salida normal.
Los fallos destruyeron el recurso una vez y no llamaron al destructor del struct
incompleto; la salida normal destruyó ambos una vez. Pasaron las 12 ejecuciones
de las dos familias en O0/O2 (`A:/nsc-fixes-evidencia/struct-payload-helper-controls.log`).
El contador demuestra llamadas a destructores, no la liberación directa del
bloque de control. No se modificó la protección de formas throwing no triviales.

Dos controles adicionales modifican por un método el campo owned de una base
ya terminada y después disparan una excepción en el constructor derivado. El
primero exige traza 21; el segundo añade un campo owned al derivado y exige
traza 321, por la destrucción del campo derivado, el destructor de base y el
campo base. Ambos pasaron para Rc y Arc en O0/O2; también pasaron en la misma
selección los controles anteriores de base implícita completa e incompleta
(`A:/nsc-fixes-evidencia/implicit-helper-controls.log`, 16 ejecuciones).
Se intentó además llamar un método de instancia desde el inicializador de un
campo de base implícita: la forma sin receptor produjo `unknown identifier` y
la forma `this.Initialize()` produjo `'this' is available only inside an
instance method`. Ninguna de las dos llegó a ejecución y no demuestra limpieza
de esa forma; los diagnósticos se observaron en la salida del ensayo, cuyo log
fue sustituido por el control válido posterior.

La distinción entre una llamada de inicializador y una llamada de instancia
ordinaria existe en HIR, pero `MirNodeMachine::lowerGroup19` convierte ambas en
`DirectInstanceCall`; conserva `baseReceiver`, que distingue la delegación a
`this` del receptor base en el caso de inicializador. Esa marca por sí sola no
demuestra que toda la base inmediata esté construida: en
`appendConstructorFieldInitializerWork`, cuando se omite el constructor de una
base implícita, el compilador llama al ancestro explícito y agrega por separado
los inicializadores de las bases omitidas. Un fallo entre ambas operaciones no
debe ejecutar el destructor de la base implícita como si estuviera completa.

Por tanto, quitar el rechazo de `invokePlacementConstructor` o activar un único
indicador al regresar la llamada no resuelve los casos de base implícita,
delegación y fallos de inicialización. La regresión en preparación
`tests/integration/shared_nontrivial_base_throw.py` conserva esos casos; no se
ha ejecutado una corrección ni se declara resuelto el problema. La siguiente
investigación debe identificar puntos de finalización de cada subobjeto y de
sus campos, y conservar la limpieza de un constructor que captura localmente
una excepción. La semántica de una segunda excepción durante limpieza sigue
separada y pendiente de decisión.

Se añadió a la regresión una base implícita que construye un campo propietario
y falla al inicializar el siguiente. El resultado esperado es destruir ese campo
y después la base ya completa, sin llamar al destructor de la base incompleta
ni al de la derivada. Los análisis semánticos de `Rc` y `Arc` pasan; la emisión
O0 sigue detenida por `shared throwing construction cannot safely clean this
payload shape`, como se registra en
`A:/nsc-fixes-evidencia/shared-implicit-base-failure-{Rc,Arc}.log`. Como la
prueba contiene varias construcciones y la emisión se detiene en la primera
forma rechazada, todavía no ejecuta ni aísla ese nuevo caso; tampoco se alcanzó
O2. Se conserva como requisito de la corrección, no como resultado aprobado.

Una revisión más precisa de `implicit-base-initializer-probe/mir.log` muestra
que la llamada selecciona el constructor de `Base` mediante `member=#1`, pero
su operando `this` figura con el tipo de `FailingImplicitMiddle`. Por tanto, el
tipo del operando por sí solo no identifica el subobjeto cuyo constructor
acaba de retornar; hay que conservar el tipo declarado por el miembro llamado
y los pasos de inicialización posteriores. La tabla MIR ofrece para cada tipo
una entrada `destroy-object`; `emitObjectDestructionSteps` usa el orden de
destrucción asociado a esa tabla y la entrada sin liberación no debe sustituirse
por el descriptor dinámico del objeto incompleto. Para bases precompiladas
existe además una ruta delegada por descriptor. Esta inspección identifica
piezas para un diseño; no demuestra que sea seguro invocarlas desde un cleanup
de construcción parcial ni resuelve campos a medio inicializar o excepciones
durante la propia limpieza.

Una reproducción aislada con construcción ordinaria, sin fábrica ni tipos
compartidos, permitió ejecutar esa misma secuencia: base explícita completa,
campo propietario de base implícita completado y fallo de inicialización
estática en el siguiente campo. Tanto la base de referencia como el compilador
de trabajo compilaron en O0/O2 y devolvieron traza 0, frente a la secuencia 421
esperada (`A:/nsc-fixes-evidencia/implicit-base-initializer-probe/results.json`).
El MIR del constructor derivado incluye la llamada al ancestro y los dos
almacenamientos, pero registra `cleanup-contract=0` y `personality=false`;
el LLVM ejecuta la llamada que falla sin un destino de limpieza. La reproducción
acredita el problema de la construcción ordinaria en esa forma, no demuestra
todavía el comportamiento de la fábrica compartida, cuya emisión se rechaza.

La fábrica normal tiene una vía distinta que sí se emite: en
`normal_factory_base_static_throw` una base con destructor termina de
construirse, y después el constructor derivado sin `throws` dispara una
excepción al inicializar una clase estática. Las reproducciones desde fuentes
de Rc y Arc compilan y ejecutan en O0 y O2; las cuatro combinaciones devuelven 3, que el ejemplo
reserva para excepción capturada, base construida una vez y destructor de base
no ejecutado. El resultado esperado es 42. O2 se comprobó por separado mediante
el selector `NSC_SHARED_POINTER_LEVELS`, porque el runner se detiene ante el
primer fallo. La prueba permanece en
`tests/integration/shared_public_pointers.py`; la salida se observó
directamente, sin log independiente. Esto demuestra una falta de destrucción
en ese caso concreto, no mide directamente la liberación del bloque ni prueba
otras formas de constructor. La condición de rechazo actual se aplica a la
fábrica que declara `throws`; no debe retirarse esa protección ni simular una
limpieza completa de la base mientras no se conozca qué subobjetos terminaron.
El control `normal_factory_base_success` construye y destruye la misma forma de
base sin excepción; ejecutó 42 para Rc y Arc en O0/O2, con una construcción y
una destrucción. Esto contrasta la salida normal, pero no identifica por sí solo
la ruta exacta que falta durante la propagación de la excepción.
El control `normal_factory_base_incomplete` hace que falle el propio
constructor de la base antes de completarse: Rc y Arc ejecutaron 42 en O0/O2,
con la excepción capturada y sin llamar al destructor de esa base incompleta.
Por tanto, destruir siempre la base desde la fábrica solucionaría de forma
incorrecta el caso anterior. Este control no cubre campos que la base pudiera
haber terminado antes de fallar, ni demuestra que tales campos se limpien.
El caso `normal_factory_base_incomplete_owned` añade un recurso owned a esa
base: verifica que se construyó antes de la excepción, que la base incompleta
no ejecuta su destructor y que la excepción se capturó. Rc y Arc devolvieron 3
en O0 y O2, que significa que el destructor del recurso no se ejecutó; el
resultado esperado es 42. Esto requiere limpiar el campo ya inicializado sin
ejecutar el destructor de la base incompleta. La prueba permanece fallida y
solo acredita esta forma concreta; no mide directamente la liberación del
bloque ni la semántica de una segunda excepción.

Ensayo provisional de base explícita completada: el backend activa una región
excepcional después de retornar la llamada al constructor base, identifica su
tipo por el miembro llamado e invoca la entrada de destrucción sin liberar su
almacenamiento. Con este ensayo, `normal_factory_base_static_throw`, el
control normal y el de base incompleta ejecutaron 42 para Rc y Arc en O0/O2;
una excepción capturada dentro del constructor también preservó la base y
terminó correctamente en ambas configuraciones. El ensayo conserva el rechazo
existente de fábricas throwing para formas no acreditadas.

No está listo para commit: `normal_factory_implicit_base_throw` espera la
traza 21 (destructor de la base intermedia y luego de la base explícita), pero
en O0 produce 1 con Rc y Arc: solo se destruye la base explícita. El runner se
detuvo en O0. La limpieza del campo owned de una base incompleta sigue abierta,
así como inicializadores envueltos, delegación, bases precompiladas y el
comportamiento de una segunda excepción en cleanup. El ensayo reutiliza
`buildNoUnwindCall` para la entrada de destrucción como el descenso existente;
eso no resuelve ni decide el contrato pendiente de doble excepción. Las salidas
se observaron directamente sin log independiente. Los cambios de backend
permanecen sin commit y no se consideran una corrección general.

La ampliación provisional registra un campo owned de la propia instancia tras
una asignación directa exitosa; al propagarse una excepción limpia la ranura
antes de destruir el recurso, para coordinarse con la limpieza placement ya
existente. Inicialmente destruyó dos campos en orden de asignación (12) y no en
orden inverso de declaración (21). Se ajustó la posición de sus regiones bajo
los locales activos: las pruebas de orden de campo y de local antes de campos,
reasignación, captura local y base incompleta pasaron para Rc y Arc en O0/O2.
La regresión existente `nsc.sdk.shared-implicit-constructor-cleanup` pasó para
clase y struct en ambas familias y niveles, sin detectar doble destrucción en
esas formas. Los resultados se observaron directamente, sin log independiente.

El caso `normal_factory_base_owned_branch`, que inicializa el campo en una
rama y después lanza, todavía falla para Rc en O0; no se alcanzaron las otras
combinaciones en esa selección. El ensayo solo registra asignaciones directas
del bloque raíz y no cubre ramas, bucles, inicializadores envueltos, campos de
valor ni delegación. No se ha demostrado que los ordinales de miembro preserven
el orden de declaración en todas las configuraciones. La base intermedia
implícita y la doble excepción siguen abiertas. Todo este código continúa
provisional, sin commit y sin sustituir los controles de seguridad existentes.

La siguiente versión provisional prepara indicadores para campos owned
asignados en sentencias del constructor y los activa después de cada asignación
observada; el cleanup consulta el indicador antes de leer la ranura. Con este
ensayo, la rama tomada pasó y también los controles de rama no tomada, excepción
antes de asignar y asignación en un bucle: Rc y Arc ejecutaron 42 en O0/O2.
Pasaron los nueve casos previos de base y orden y volvió a pasar
`nsc.sdk.shared-implicit-constructor-cleanup`. Las salidas se observaron
directamente y no se conservaron logs independientes.

Los indicadores aún no cubren asignaciones anidadas dentro de expresiones ni
sentencias no envueltas en un bloque; tampoco prueban transferencia de campos,
bases implícitas, campos de valor, inicializadores envueltos, delegación ni la
doble excepción. El orden se deriva de ordinales de miembro y sigue sin
acreditarse para todas las configuraciones. Esta versión permanece provisional y
sin commit; no se considera resuelto el contrato completo de construcción.

Una ampliación posterior añade en HIR una marca al terminar los inicializadores
de cada base implícita dentro de un constructor. El ensayo registra los campos
owned de esas bases mientras se construyen y, al completar cada base, transfiere
la limpieza a su entrada de destrucción sin liberar almacenamiento. Se invalida
la caché local de la cadena de cleanup al cambiar el tipo de la base completa.

En Rc y Arc pasaron en O0/O2 los casos dirigidos de base implícita completa e
incompleta, campo owned transferido sin doble destrucción, dos bases implícitas
y salida normal sin destrucción prematura. La selección conjunta de 17 casos
de construcción y limpieza pasó en ambas familias y niveles; el control de
salida normal se ejecutó aparte. Las salidas se observaron directamente sin
log independiente.

Esto no habilita todavía las fábricas throwing: siguen sin acreditarse las
construcciones sin constructor ejecutable, inicializadores de otras rutas,
asignaciones embebidas en expresiones, campos de valor, delegación,
precompilados y el contrato de doble excepción. Los ordinales de miembros como
orden de declaración y los efectos de las marcas sobre cachés requieren
verificación adicional. Los cambios siguen provisionales y sin commit.

Actualización del ensayo: `normal_factory_base_owned_expression` reproduce una
asignación de campo owned dentro de una condición; antes del ajuste, Rc en O0
no destruía ese recurso al fallar después el constructor. El marcador se activa
ahora tras la escritura exitosa en `lowerExpressionFrame044Store`. Con el
compilador incremental aislado, `shared-struct-pattern-check.cmd` ejecutó 19
casos seleccionados para Rc y Arc en O0 y O2: las 76 ejecuciones devolvieron
42 y CTest pasó. La prueba dirigida `constructor-field-control.cmd` pasó para
clases y estructuras, controles locales y atómicos, en ambos niveles. También
pasó `tuple-partial-regression.cmd`, incluyendo sus 14 casos en O0/O2. La salida
se observó directamente; no se conservó un log independiente. Estos resultados
acreditan solamente las formas probadas: no resuelven los demás límites
enumerados arriba ni autorizan retirar la protección de fábricas throwing.

La prueba dirigida `tuple-await-replay-test.cmd` terminó con fallo en
`await_in_unwind_finally` O0: devolvió 7 en lugar de 42; se detuvo antes de O2
para ese caso. Los ocho casos anteriores pasaron en O0/O2. La evidencia previa
`A:/nsc-fixes-evidencia/tuple-unwind-finally-probe/results.json` reproduce la
pérdida de la excepción tanto en la base como en el prototipo; por ello no se
atribuye este resultado al último ajuste sin evidencia adicional. La
continuación suspendida en finally sigue pendiente.

La prueba diagnóstica `async-finally-value-diagnostic.cmd` separa el resultado
del retorno y la ejecución del finally: en O0/O2 el finally sí registra su
marca, pero el valor recibido es 0 en lugar de 5 (salida 20). En la ruta de
excepción también se registra la marca, pero el catch no recibe la excepción
(salida 11). Los controles correspondientes con un await ya listo devuelven
42. Una repetición con el compilador y runtime de la base independiente dio
los mismos resultados (`async-finally-value-baseline.cmd` y `.log`); por tanto,
estos casos no son una regresión introducida por el prototipo actual.

En el LLVM de `async-finally-return-ir/llvm.log`, la ruta de return guarda 5 y
marca el future terminado antes de ejecutar la limpieza; al suspender, el await
sobrescribe el estado con su identificador de reanudación. La ruta que termina
esa instancia del finally retorna del poll sin volver a marcarlo terminado.
Otra instancia generada del finally continúa hacia un retorno predeterminado
de 0. Esto evidencia estados y continuaciones distintos para ese ejemplo, pero
no demuestra por sí solo el recorrido dinámico completo ni proporciona una
solución segura para excepciones, finally anidados o cleanuppads de Windows.
Se conserva la prueba fallida y no se cambia el comportamiento hasta resolver
conjuntamente esas rutas. Como control adicional, cuando el `finally` lanza
una excepción nueva después del await, el catch la recibe y el retorno anterior
no prevalece: tanto con suspensión como con el await ya listo se obtuvo 42 en
O0/O2 (`async-finally-throw-replacement.cmd` y `.log`). Ese control no cubre
una segunda excepción mientras otra excepción sigue propagándose.

`normal_factory_base_owned_transfer` mueve el campo owned a un local antes de
la excepción y observa una sola destrucción: pasó con Rc y Arc en O0/O2.
`normal_factory_base_owned_clear` libera el campo asignándole null antes de la
excepción y también observa una sola destrucción en las cuatro combinaciones;
la ejecución conjunta está en
`A:/nsc-fixes-evidencia/shared-constructor-field-transfer-clear.log`.
Los controles `normal_factory_base_owned_parameter` y
`normal_factory_base_owned_parameter_field_order` también pasaron en Rc/Arc
O0/O2: al fallar el constructor base, se destruyó su parámetro owned, y con un
campo adicional la traza fue 21 (campo primero y parámetro después). Evidencia:
`A:/nsc-fixes-evidencia/shared-constructor-parameter-order.log`. Estas pruebas
no abarcan fallos en argumentos. El caso separado
`normal_factory_base_owned_parameter_to_field` transfiere el parámetro al campo
y verifica una sola destrucción tras la excepción; pasó en Rc/Arc O0/O2
(`A:/nsc-fixes-evidencia/shared-constructor-parameter-transfer.log`).
Los controles `normal_factory_base_argument_failure` y
`normal_factory_base_argument_failure_order` fallan al evaluar un argumento
posterior del inicializador base: verifican que la base no comience y que uno
o dos argumentos propietarios ya evaluados se destruyan en orden inverso.
Pasaron con Rc/Arc O0/O2; evidencia en
`A:/nsc-fixes-evidencia/shared-constructor-argument-failure.log`. Solo cubren
esta forma de argumento y la excepción implícita de inicialización estática.

Se añadieron controles del caso en que falla la evaluación del reemplazo de
un campo `owned` ya inicializado. Con la excepción propagada, el recurso
anterior se destruyó una vez; con captura local permaneció vivo hasta la
destrucción normal. Un control con dos campos observó el orden inverso 21,
y otro colocó la asignación dentro de la condición de un `if`. Estas cuatro
formas ejecutaron 42 para Rc y Arc en O0/O2, junto a los controles anteriores
de reasignación y fallo antes del almacenamiento
(`A:/nsc-fixes-evidencia/constructor-failed-replacement-expression.log`).
El fallo se induce al inicializar una clase estática desde una función que
retorna un recurso owned; esto no demuestra otras fuentes de excepción ni
contabiliza directamente la liberación del bloque compartido.

La variante que usa una expresión condicional como valor de la asignación se
rechazó en análisis para ambas familias porque no se reconoce como expresión
de transferencia de ownership. Se conserva el ejemplo y el diagnóstico en el
mismo log; no se ejecutó ni se ha determinado aquí si el contrato del lenguaje
debe admitir esa forma. Los cambios de cleanup siguen provisionales y no se
retiró la protección de las fábricas que declaran `throws`.
`normal_factory_base_argument_completed` mostró otra ruta: después de completar
una base que recibe un argumento owned, la excepción de la derivada dejó la
traza 0 en Rc O0, en vez de 21. El MIR del proyecto aislado
`A:/nsc-fixes-evidencia/base-argument-completed-probe/Probe.nsproj` muestra que
`FullExpressionCleanup` envuelve la llamada directa a la base. El ensayo en
`lowerBlockStatement` reconoce esa forma de un solo hijo y activa la limpieza
tras retornar la envoltura. Tras compilar incrementalmente, pasaron 12 casos
dirigidos para Rc/Arc O0/O2, incluidos éxito y fallo antes, durante y después
de la base (`shared-constructor-wrapped-base-controls.log` en evidencia). El
resultado no cubre una excepción lanzada por la limpieza de un temporal después
de retornar el constructor base; la región aún se activa después de esa limpieza.
Se intentó reproducir esa situación con un temporal devuelto por una fábrica.
Rc O0 devolvió traza 1, pero el MIR de
`A:/nsc-fixes-evidencia/base-temporary-receiver-probe/Probe.nsproj` muestra que
la limpieza del temporal está dentro del argumento y ocurre antes de la llamada
base. Por tanto, esa salida no demuestra una base completada sin limpiar. Se
retiró el ensayo de activación anticipada que intentaba corregirla. El contrato
de vida de temporales hasta el final de la expresión completa y su aplicación
específica a este inicializador requieren investigación separada. Las variantes
rechazadas y sus diagnósticos se conservan en
`A:/nsc-fixes-evidencia/base-argument-completed-probe/temporary-cleanup-variants.txt`.
Tras retirar el ensayo adicional se recompiló incrementalmente y volvieron a
pasar los 12 controles dirigidos en Rc/Arc O0/O2; salida conservada en
`A:/nsc-fixes-evidencia/shared-constructor-wrapped-base-restored.log`.

El caso `normal_factory_base_value_field_failure` confirma que un campo struct
con destructor terminó de construirse antes de una excepción que abortó el
constructor base. Antes del ensayo, Rc O0 devolvió 5: excepción capturada y
una construcción, sin destrucción del campo. El MIR de
`A:/nsc-fixes-evidencia/base-value-field-probe/Probe.nsproj` contiene un `Store`
directo a ese campo `requires-drop=true`. El registro de cleanup provisional
solo reconocía referencias owned. El ensayo registra también los valores que
requieren drop y destruye su dirección mediante `emitValueDropAddress`.
Después de compilar incrementalmente pasaron 10 casos dirigidos para Rc/Arc en
O0/O2: el fallo reproducido, fallo antes del campo, salida normal, mezcla de
campos de referencia y valor, bases implícitas completas e incompletas y cuatro
controles previos. Evidencia:
`A:/nsc-fixes-evidencia/shared-constructor-value-field-controls.log`. También
pasó `nsc.sdk.shared-implicit-constructor-cleanup` en sus ocho configuraciones.
El ensayo sigue sin commit y no acredita la inicialización parcial del propio
struct, valores predeterminados sin `Store`, uniones, alias, ni segunda excepción;
la protección de fábricas throwing permanece activa.

El control `normal_factory_base_value_field_move` detectó una regresión del
ensayo: mover el campo struct y fallar después destruía dos veces; Rc O0
devolvió 5. El MIR de `base-value-move-probe/Probe.nsproj` usa
`MoveOwnedField(FieldAddress(ThisValue))`. El ensayo desactiva el indicador
excepcional después del movimiento; el control pasó en Rc/Arc O0/O2. Sin
embargo, `normal_factory_base_value_field_move_replace` fallaba en Rc O0: el
contador cambiaba durante la reasignación, antes de la excepción (salida 4;
`constructor-value-replace-before.log`). El cambio provisional en
`storeFromParts` consulta el indicador de vida ya existente para campos directos
del constructor: solo destruye el valor anterior si sigue vivo. La instrucción
original de corregir el compilador autoriza este archivo del backend; la
restricción anterior de presupuesto era autoimpuesta. El caso original y siete
controles pasaron para Rc y Arc en O0/O2 (32 ejecuciones,
`constructor-value-replace-expanded.log`). También pasaron ocho controles de
limpieza de constructores y 28 de inicialización parcial de tuplas
(`constructor-value-replace-field-controls.log` y
`constructor-value-replace-tuple-controls.log`). El cambio depende de los demás
cambios de limpieza aún sin registrar. El control adicional
`value_field_replace_in_method` verifica la sustitución y destrucción de un campo
de valor desde un método ordinario en O0/O2
(`value-field-method-check.log`); el runner lo ejecuta dos veces por familia,
pero el ejemplo usa un objeto owned ordinario, no las API de punteros. Esto
comprueba esa ruta fuera del constructor, no todas las formas de asignación.
El cambio
no acredita el inicio de vida predeterminado,
destructores que lanzan durante otra excepción ni las fábricas que propagan
excepciones. El caso
`normal_factory_base_value_default_failure` también falla en Rc O0: el campo
sin inicializador no se destruye al fallar el constructor, aunque el control
de salida normal pasó en Rc O0/O2. No se probaron Arc ni O2 para ese fallo.
Ninguno de estos resultados acredita una solución completa del ensayo.

La selección posterior ejecutó por separado el caso de fallo del campo
predeterminado con Arc en O0 y O2 y con Rc en O2: las tres ejecuciones
devolvieron 3, que en ese fixture significa excepción capturada y cero llamadas
al destructor de `Part`. Los registros son `shared-default-field-Arc-O0.log`,
`shared-default-field-Arc-O2.log` y `shared-default-field-Rc-O2.log`. El control
de construcción normal ejecutó 42 para Rc y Arc en O0/O2
(`shared-default-field-success.log`). Los logs están en
`A:/nsc-fixes-evidencia`. La expectativa de destrucción durante construcción
fallida depende todavía de la decisión consultada sobre el comienzo de vida de
campos predeterminados; no se modificó la implementación en esta tanda.

Los controles `normal_factory_base_value_partial_failure` y
`normal_factory_base_value_partial_order` prueban un campo struct cuyo propio
constructor inicializa un recurso y luego falla por inicialización estática.
En Rc/Arc O0/O2 se limpió el recurso, se omitió el destructor del struct
incompleto y, cuando había un campo owned anterior, la traza fue 21: recurso
interno primero y campo anterior después. Evidencia:
`A:/nsc-fixes-evidencia/shared-value-partial-construction.log`. Esto cubre esa
forma concreta, no todos los inicializadores parciales ni los campos de valor
predeterminados.

Para el campo struct sin inicializador, `appendInstanceFieldInitializers` en
`src/Frontend/HIR/HirFunctionLowerer.cpp` omite los campos cuya sintaxis no
tiene inicializador: no hay `Store` que active el indicador del ensayo. La
construcción normal destruye ese campo; al fallar el cuerpo, la reproducción
registró cero drops. La lectura del HIR explica la ausencia del indicador, pero
no determina en qué punto de la secuencia de campos debe comenzar su vida,
especialmente si falla un inicializador anterior. Antes de corregirlo hay que
establecer ese contrato y su interacción con bases implícitas; no se ha
modificado `HirFunctionLowerer.cpp`.
En cambio, `normal_factory_base_owned_destroy` ejecuta `destroy resource` y
después provoca una excepción: Rc O0 capturó la excepción y observó más de una
destrucción (salida 4). El control `normal_factory_base_owned_destroy_success`,
sin excepción, también observó más de una destrucción en Rc O0. Ambos ensayos
se detuvieron al fallar ese primer resultado; no se han comprobado Arc ni O2.
`lowerExpressionFrame021ObjectDestroy` destruye el objeto, pero no modifica la
ranura propietaria. El contrato `NSC-PLACEMENT-003` en
`docs/1.0.1/compiler/NativeSharp_Compiler_Contract_1_0_1.md` exige rechazar
`destroy` aplicado a un valor owned; la implementación ya tiene ese rechazo
para `operand.ownership == Owned`, pero no lo aplicó a este acceso de campo.
Las reproducciones se conservan con la expectativa de rechazo semántico, sin
volver a ejecutar código que destruye el objeto más de una vez. La corrección
del análisis semántico ya rechaza el campo owned por su declaración, tanto con
receptor implícito como explícito. Pasaron la prueba dirigida de destrucción y
las dos reproducciones para Rc y Arc; detalles y límites en
`destroy-owned-field.md`. Este hallazgo no prueba que el cleanup excepcional
fuera la causa del rechazo ausente.

La opción dirigida `NSC_SHARED_POINTER_REPEAT_CACHE=1` de la prueba pública usa
una caché de artefactos dentro del proyecto temporal, recompila cada caso y
exige `compilation-bundle-cache: hit` antes de volver a ejecutar el resultado.
Para `normal_factory_implicit_base_incomplete` y
`normal_factory_implicit_base_completed_owned`, las compilaciones iniciales y
repetidas dieron salida 42 en Rc y Arc, tanto O0 como O2; el segundo build
informó el acierto en las ocho combinaciones. La salida está en
`A:/nsc-fixes-evidencia/shared-constructor-cache-repeat.log`. Esto verifica
reutilización del bundle para esos proyectos,
no demuestra por separado el round-trip de todos los nodos HIR/MIR ni cubre
cambios de fuentes o invalidación de caché.

El contrato `NSSMARTPTR` publicado por el compilador exige clonación explícita,
ausencia de copia implícita, upgrade sin resurrección, rechazo de overflow del
contador y mantenimiento de las funciones de destrucción y liberación del
módulo productor mientras exista el bloque de control. `Rc` y `WeakRc` son
locales; `Arc` y `WeakArc` pueden cruzar hilos solo cuando el payload satisface
Send/Sync. Las familias y fábricas pertenecen a `NativeSharp.Core`, que es el
namespace usado por la reproducción, y deben respetar el contrato publicado.
El usuario aprobó admitir clases y estructuras y una fábrica adicional que
propague excepciones y libere el bloque si la construcción falla.

Cada bloque conserva contadores fuertes y débiles, origen del allocator,
destructor, liberador y procedencia del módulo. El último fuerte destruye el
payload una vez y libera la referencia débil implícita; el bloque solo se
libera al desaparecer el último débil. Upgrade incrementa el fuerte solo si
es distinto de cero, mediante compare-exchange en Arc. Las operaciones que
no puedan incrementar deben fallar antes de desbordar. Clone y Downgrade son
explícitos; un valor expirado nunca vuelve a poseer el payload.

La regresión `nsc.sdk.shared-public-counter-limits` verifica desde fuentes las
cuatro API públicas con contadores establecidos artificialmente en el máximo:
`Clone` fuerte, `Upgrade`, `Clone` débil y `Downgrade` devuelven una referencia
vacía sin incrementar el contador. Después restaura los contadores y comprueba
un payload válido y una destrucción. Pasó para Rc/WeakRc y Arc/WeakArc en
O0/O2 (`A:/nsc-fixes-evidencia/shared-public-counter-limits.log`). El fixture
accede al bloque interno en un solo hilo para preparar el límite; no equivale a
alcanzarlo mediante operaciones reales ni verifica overflow concurrente.
La prueba `nsc.sdk.shared-public-counter-race` complementa ese control para
Arc/WeakArc: establece un contador en máximo menos uno antes de iniciar dos
hilos, que compiten por `Upgrade` o `Clone` débil y conservan sus resultados
hasta que ambos terminan el intento. En 32 rondas exige un éxito y un rechazo,
valor máximo sin wrap y decremento correcto antes de restaurar el contador;
el payload se destruye una vez al finalizar cada ronda. Pasó en O0/O2
(`A:/nsc-fixes-evidencia/shared-public-counter-race.log`). El límite se prepara
artificialmente y la prueba no mide la liberación del bloque ni demuestra todos
los intercalados o Send/Sync.

Tras los cambios provisionales de metadata se repitieron cuatro pruebas
dirigidas desde fuentes: carrera de upgrade, clonación débil concurrente,
último fuerte con débil retenido y límites concurrentes de ambos contadores.
Las cuatro pasaron en O0/O2 (`shared-atomic-directed-current.log`); la última
incluye por separado los contadores fuerte y débil. Son ensayos con un número
finito de intercalados y usan el estado estático del fixture; no acreditan la
transferencia pública de Arc entre hilos ni la liberación física del bloque.

La implementación debe acreditar además la fábrica de una sola asignación
exigida por el manifiesto, el movimiento sin doble liberación, el pin del módulo
y la imposibilidad de copiar o compartir Rc de forma indebida. El SDK contiene
`ReferenceCounts` atómico y hooks en `NativeSharp.Core.Threading`, pero no
provee los cuatro tipos ni demuestra las demás garantías. No basta añadir
declaraciones vacías ni tratar esos contadores como una implementación completa.
Es necesario investigar y probar el soporte efectivo del compilador para
propiedad genérica, destrucción, restricciones Send/Sync y módulo productor.
`NativeSharp.Core.Threading.nsproj` ya referencia `NativeSharp.Core`, mientras
`Interlocked` y `ReferenceCounts` viven en Threading. Por ello Core no puede
simplemente agregar una referencia inversa para implementar Arc: primero hay
que ubicar la primitiva compartida sin crear un ciclo, manteniendo disponibles
los nombres esperados por los consumidores de Core.
Como primer paso, `SharedReferenceCounts.ns` añade a Core un estado interno
de contadores locales para Rc y otro con cargas y compare-exchange
secuencialmente consistentes para Arc. El
frontend reconoce exclusivamente las firmas internas de 64 bits para emitir
los mismos helpers atómicos usados por Threading. La prueba dirigida
`nsc.sdk.atomic-shared-counts` comprueba cero, último fuerte y débil y rechazo
de overflow para ambos; también comprueba 200 000 ciclos atómicos distribuidos
en cuatro hilos en O0/O2, detectando fallos intermedios de retención y
liberación. El análisis
del proyecto Core completo también pasó (`shared-counts-test.log` y
`shared-core-check.log` en `A:/nsc-fixes-evidencia`). Es una primitiva interna,
no demuestra todavía las familias públicas, la destrucción del payload ni
seguridad de concurrencia para todos los intercalados posibles.
La búsqueda de `validateSmartPointerOperation` solo encuentra su definición
en `src/Language/SmartPointerSemantics.cpp`; no se ha encontrado una llamada
desde el frontend. El manifiesto de reglas, por tanto, no demuestra que hoy se
rechacen copias implícitas o cruces de hilo indebidos en programas reales.
El contrato T13-F08 también describe `ModuleLifetimeRegistry`, pero la búsqueda
del símbolo en `src`, `include` y `lib` encuentra únicamente su definición en
`src/Language/CrossModuleLifetime.cpp` y su interfaz en `include`; esto no
acredita un registro de módulos conectado al runtime de los programas.
En Windows, [GetModuleHandleEx](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexa)
puede adquirir una referencia desde una dirección del módulo; la opción PIN
mantiene el módulo hasta terminar el proceso. [FreeLibrary](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-freelibrary)
puede descargarlo al llegar a cero. Estos hechos identifican piezas posibles,
pero no demuestran una solución segura para liberar el último control block
mientras todavía se ejecuta código del módulo. No se ha implementado ni
validado esa integración.

La revisión posterior del código en curso precisa esa limitación estructural:
`SharedControlStorage` reserva ocho palabras iniciales; `SharedLocalControl` y
`SharedAtomicControl` usan las dos primeras para contadores y `block[2]` para
el débil implícito. En esas rutas no se guarda una referencia al módulo ni un
thunk de liberación del módulo. En el último fuerte, las cargas de clase se
destruyen mediante `SharedStorage.DestroyObject<T>`; HIR marca destrucción
dinámica y LLVM obtiene la entrada desde el descriptor del objeto. El último
débil ejecuta `delete[] block`. Para valores, `Destroy<T>` invoca `destroy`
sobre la ranura. El registro `ModuleLifetimeRegistry` hallado pertenece al
compilador; su presencia no conecta esas operaciones a la vida de un módulo
en el programa generado. Esta inspección no constituye una prueba de descarga
real ni demuestra qué imágenes específicas contienen los thunks generados.

Se consultó si retener el módulo hasta el último débil o hasta terminar el
proceso. La primera opción mantiene la duración necesaria sin retenerlo toda
la ejecución, pero necesita un mecanismo seguro para que el código que libera
la retención no regrese a una imagen ya descargada. No se modificó el runtime,
la interoperabilidad ni el SDK al registrar esta evidencia; la decisión y la
integración permanecen pendientes.

En `A:/nsc-fixes-evidencia/smart-pointer-probe/` un prototipo descartable de
struct genérico con destructor poseyó un payload y ejecutó con salida 42; una
copia implícita fue rechazada (`smart-pointer-copy-probe/check.log`). Un
segundo prototipo local pasó clonación, expiración débil y destrucción única
en O0 (`smart-pointer-lifecycle/`). Sin embargo, un `Get()` que devuelve `T`
permite conservar una referencia después del último propietario
(`smart-pointer-escape/check.log`); cambiarlo a `borrowed T` se rechaza porque
el frontend no acredita la vida del campo (`smart-pointer-borrow/direct-check.log`).
Crear antes un préstamo local también se rechaza (`local-check.log`). Los
intentos de retornar `ref[this]` al campo también se rechazan con vida
`unknown` (`ref-this-check.log`). Los scripts en la carpeta de evidencia
reconstruyen cada variante por separado.
Una vista `borrowed struct` que contiene la referencia sí pasó el análisis,
pero también lo pasó al asignarla fuera del ámbito del último `Rc` y usarla
después (`smart-pointer-view-probe/escape-check.log`); esa comprobación no
demuestra un préstamo seguro del payload compartido.
El compilador ahora rechaza esa asignación y otras formas de escape de la vista
ligada al receptor; véase `view-receiver-lifetime.md`. La referencia extraída
con `view.Get()` también podía sobrevivir al propietario en el prototipo;
la regla posterior y sus límites se describen más abajo.
El prototipo recibe además un objeto ya asignado y crea el bloque de control
por separado; no acredita la fábrica de una sola asignación del manifiesto y
solo ensaya payloads de clase.
Una fábrica genérica ordinaria `new T()` sí compiló y ejecutó con salida 42
(`smart-pointer-generic-factory/results.json`). En cambio, una fábrica que
retorna `owned T` desde `new (storage) T()` fue rechazada en O0 y O2
(`smart-pointer-placement-factory/all-results.json`): la construcción placement
posee el objeto pero toma prestado el almacenamiento. No se debe eliminar ese
rechazo sin transportar de forma segura la distinción entre destrucción del
objeto y liberación del bloque compartido.
Una variante que retorna una referencia ordinaria al objeto placement sí
ejecutó y leyó el valor en O0 y O2; no destruyó el objeto ni transfirió su
ownership. Intentar adoptarla como `raii` fue rechazado en ambos niveles. Las
tres variantes se conservan por separado en el mismo registro.
`SharedStorage.DestroyObject<T>` es una operación interna para destruir el
objeto por su descriptor dinámico sin liberar el almacenamiento: la prueba
`nsc.sdk.shared-object-destroy` verifica O0 y O2, destructor derivado llamado a
través de una referencia base, nulo y reutilización del almacenamiento; rechaza
`string`, que carece del descriptor requerido. El llamador sigue siendo
responsable de evitar destrucciones repetidas y liberar el bloque. Esta pieza
no implementa todavía el bloque de control ni las familias públicas.
La prueba aislada `shared-layout-probe/results.json` muestra que
`sizeof(T)` en una fábrica genérica devuelve 8 en O0 y O2 para una clase con
cuatro campos `long`: mide la referencia, no el objeto. El modelo de layout
distingue explícitamente el tamaño de referencia del tamaño y alineación de
la instancia (`ClassObjectLayout`): el volcado de esa prueba informa 40 bytes
para la instancia y 8 para la referencia. La fábrica no puede calcular el espacio
del payload usando `sizeof(T)` sin reservar menos memoria de la necesaria.
Las operaciones internas `SharedStorage.InstanceSize<T>(null)` e
`InstanceAlignment<T>(null)` consultan el layout de clase especializado y
exigen un testigo literal nulo, sin evaluar expresiones con efectos; la prueba
`nsc.sdk.shared-object-destroy` comprueba tamaños 40 y 48 para base y derivada,
la alineación 8 y el
rechazo de un testigo distinto de null. El prototipo aislado
`shared-sized-lifecycle-probe/results.json` usa ambos valores para calcular
la reserva y alinear el payload y ejecuta el ciclo fuerte-débil en O0 y O2.
Todavía no verifica límites aritméticos, alineaciones mayores ni errores de
construcción y no constituye una fábrica pública.
`SharedControlStorage` traslada la reserva y construcción placement al SDK
como primitivas internas. Reserva provisionalmente ocho palabras al inicio,
comprueba el cálculo del tamaño antes de sumar y redondear, y valida que el
objeto alineado quepa en el bloque. El llamador conserva la responsabilidad
de destruir el objeto y liberar el arreglo, incluso ante una excepción; la
prueba dirigida verifica bloque nulo, bloque insuficiente, alineación de 32,
entradas inválidas, rechazo antes del límite de tamaño y el ciclo válido. No
intenta realmente reservar el máximo. El layout final del control, la limpieza
ante fallo y la API pública siguen pendientes.
`AllocateValue<T>` y `ConstructValue<T>` usan el layout de valor para una
estructura y devuelven un puntero a la construcción placement. La misma prueba
ejecuta un caso de estructura con destructor y alineación 32 en O0/O2, destruye
el valor y libera el bloque; no demuestra soporte de todos los tipos de valor.
Una prueba de constructor con `throws NativeSharp.Core.Exception` no llegó a
ejecución: la especialización de `SharedControlStorage.Construct<T>` lo
rechazó porque el método genérico no declara el efecto
(`shared-constructor-failure-probe/results.json`). Ese rechazo no demuestra
limpieza durante una excepción. Las variantes internas `CreateThrowing` y
`CreateValueThrowing` declaran el efecto y liberan el arreglo con `delete[]`
si la construcción no se transfiere. Para placement inicializado en cero con
campos owned de referencia, sin bases no triviales ni campos de valor con drop, el compilador
destruye los campos inicializados en orden inverso al fallar el constructor,
sin llamar el destructor del objeto incompleto. La prueba dirigida verifica
clases y estructuras, excepción antes y después de inicializar campos, orden,
constructores correctos y liberación en el cleanup del IR; pasó O0/O2
(`shared-throwing-tests.log`). Ahora se admite una cadena de bases locales sin
destructor, campos owned ni valores con drop: el fixture verifica dos bases
triviales y un campo owned del derivado, y comprueba que ese campo se destruye
al lanzar tanto en placement directo como mediante la fábrica. El recorrido es
iterativo y consulta los índices de layouts y miembros cuando están disponibles;
no llama el destructor del objeto incompleto. La prueba también cubre un throw
del propio constructor base, antes de inicializar el campo del derivado. Bases
con destructor, campo owned
o valor con drop se rechazan, así
como las bases precompiladas; campos propios de valor no trivial y layout
explícito también permanecen restringidos. La selección dirigida pasó 2/2 en
O0/O2 (`shared-throwing-tests.log`). El análisis estático dirigido terminó con
código 0 y nueve advertencias de padding en headers
(`shared-trivial-base-analysis.log`); no se afirma ausencia de advertencias.
La prueba `shared-local-handles-test.log` verifica además que las fábricas
internas local y atómica propagan la excepción y limpian el campo del derivado
en O0/O2.
Esto no completa la fábrica pública.
La construcción ordinaria de los ejemplos `Bad` y `BadValue` también dejó
vivos los campos en O0/O2 (`shared-throwing-normal-compare/results.json`);
esta corrección se limita a placement. La prueba histórica TL-LANG-472 no
distingue destrucción al asignar de destrucción al lanzar: una reproducción
registra dos drops antes y dos después (`constructor-drop-existing-probe/results.json`),
por lo que no demuestra por sí sola cleanup durante la excepción.
Una reproducción independiente de construcción ordinaria con base no trivial
registró que el constructor de la base terminó, que el derivado lanzó y que
ninguno de los dos destructores corrió: la base y la rama actual devuelven 101
en O0 y O2, frente a 42 esperado, con sus runtimes aislados respectivos
(`throwing-base-probe/results.json`). Esto no demuestra todas las
rutas de excepciones, pero impide tratar como trivial una base con destructor.
La limpieza correcta requiere distinguir si la construcción base terminó antes
del throw; no basta llamar su destructor incondicionalmente. Sigue pendiente.
La regresión preparada `shared_nontrivial_base_throw.py` exige ese caso para
ambas familias, distingue el fallo en la base del fallo posterior y verifica
el orden de limpieza de campos y destructores. El análisis semántico pasó para
`Rc` y `Arc`, pero la emisión O0 falla en ambas por el rechazo deliberado
(`shared-nontrivial-base-throw-Rc.log` y `shared-nontrivial-base-throw-Arc.log`);
las ejecuciones y la emisión O2 de esta regresión no se han alcanzado.
La regresión en preparación incluye ahora una excepción propagada por una
llamada y otra capturada dentro del constructor. Ambas familias pasan el
análisis semántico con esos casos incluidos, pero la emisión O0 sigue detenida
por la protección anterior; no se ejecutaron los nuevos casos ni O2
(`shared-nontrivial-base-call-Rc.log`, `shared-nontrivial-base-call-Arc.log`).
Esto obliga a comprobar tanto `buildCurrentUnwindDestination` como
`lowerExceptionalTransfer` al implementar la limpieza, además de conservar los
catch locales que permiten continuar la construcción.
En la ruta observada, `ProjectOopHirModel::appendConstructorInitializerCall`
marca el receptor de la llamada base y
`ProjectOopMirModel::MirNodeMachine::lowerGroup19` convierte la llamada en
`DirectInstanceCall`, como una llamada de instancia ordinaria. La copia general
del nodo conserva `baseReceiver` y el nombre del constructor seleccionado; el
backend no usa hoy esa marca como estado de construcción. Para bases implícitas
el HIR puede invocar un ancestro y ejecutar luego inicializadores de tipos
intermedios en el constructor derivado. Por ello, marcar como completa toda la
base inmediata en cuanto retorna esa llamada no cubre ese caso. Esta lectura
de código no implementa ni verifica aún el seguimiento de subobjetos vivos.
Una reproducción adicional con `PanicProfile=unwind-native` y constructor
estático que llama a `panic` genera la llamada al inicializador de `Derived`
antes de llamar al constructor de `Base`; el inicializador propaga su fallo
mediante `cleanupret ... unwind to caller`. Fuente, proyecto, comando e IR:
`A:/nsc-fixes-evidencia/static-init-before-base/`. Solo se inspeccionó el IR:
no se ejecutó el programa ni se acreditó cómo lo capturaría el llamador. Este
orden descarta suponer que la base terminó únicamente porque su constructor
no declara `throws`: una salida excepcional puede ocurrir antes de invocarla.
Un segundo control usó una excepción NativeSharp declarada en el constructor
estático, en vez de `panic`: la construcción ordinaria se compiló y ejecutó en
O0 y O2, y ambas ejecuciones devolvieron 42 al capturar la excepción y observar
que la base no se construyó ni se destruyó
(`A:/nsc-fixes-evidencia/static-throws-before-base/run.log`). El caso equivalente
se añadió a la regresión pendiente de las fábricas: el análisis semántico pasa
para `Rc` y `Arc`, pero la emisión O0 sigue detenida por la protección de bases
no triviales (`shared-static-before-base-Rc.log` y
`shared-static-before-base-Arc.log`). No se ejecutó todavía ese caso mediante
las fábricas ni se alcanzó su emisión O2.
Un control separado de destrucción normal con campos propietarios confirmó
el orden derivado, campo derivado, base y campo base en O0/O2
(`destruction-order-control/results.json` y `current-ir.log`). En el IR de la
reproducción que lanza, la llamada a la base termina antes del throw pero no
hay una ruta de limpieza de la base (`throwing-base-probe/current-ir.log`).
El cleanup actual de placement destruye campos al regresar al llamador; una
destrucción de la base introducida antes de esa limpieza podría invertir el
orden. Se necesita preservar tanto el estado de construcción como la secuencia,
no basta retirar el rechazo ni añadir una llamada incondicional.
El layout de cada clase contiene únicamente los campos declarados por ella;
la base ocupa un prefijo con layout propio. Esto permite localizar los campos
por subobjeto, pero no demuestra que estén vivos. La regresión preparada
incluye ahora una base intermedia implícita con campo propietario y destructor,
y espera la traza `53421` si falla la derivada. El análisis de `Rc` acepta el
programa ampliado; la emisión O0 sigue rechazada antes de poder ejecutarlo
(`shared-nontrivial-base-throw-Rc.log`). La traza aún no está verificada.
Un control separado de construcción y destrucción completas de esa misma cadena
implícita obtuvo `653421` en O0 y O2, con salida 42 en ambos casos
(`implicit-base-order/results.json`). Esto comprueba el orden normal, no la
limpieza parcial cuando falla el constructor derivado.
Un segundo proyecto aislado usa la misma jerarquía con `new Derived()` y un
constructor derivado que lanza después de crear su campo. En O0 y O2 compiló y
terminó con código 10, que identifica traza cero; se esperaba código 42 para
la traza `53421` (`implicit-base-throw/results.json`). Este control confirma
que en esa ruta ordinaria tampoco corren los destructores de las bases ni los
de los campos ya creados. No ejecuta las fábricas de punteros, cuya protección
sigue impidiendo emitir la reproducción correspondiente.
La inspección de HIR muestra que `prependConstructorWork` antepone la llamada
al constructor base y después los inicializadores de campos. En el backend,
las llamadas potencialmente excepcionales y el `throw` explícito construyen
sus rutas de unwind por funciones distintas (`buildCurrentUnwindDestination`
y `lowerExceptionalTransfer`). Una implementación debe conservar los cleanups
de scopes y los `catch` locales: solo una excepción que abandone el constructor
debe iniciar la destrucción parcial. Para la cadena de bases necesita saber qué
subobjetos terminaron; para el derivado debe destruir antes sus campos ya vivos
y omitir su propio destructor. La evidencia actual no cubre delegación
`this(...)`, todos los inicializadores, campos de valor con drop, bases
precompiladas ni todos los perfiles de panic.

Como siguiente paso de implementación, el HIR marca la llamada de
inicialización de constructor y MIR conserva `baseReceiver` y el miembro
seleccionado. Eso permite identificar una llamada base explícita, pero una
cadena con constructores implícitos puede saltar directamente a un ancestro y
ejecutar después inicializadores de las bases intermedias; todavía no hay
evidencia de que una sola bandera después de la llamada represente esa cadena.
La limpieza actual de campos propietarios vive en el llamador de placement.
Si se mueve parte del trabajo al constructor, debe retirarse o coordinarse esa
limpieza para evitar orden inverso o doble destrucción. La regresión preparada
incluye también una excepción capturada dentro del constructor y una cadena
con dos bases, además de un constructor delegado cuyo destino lanza; su
análisis pasó, pero aún no se han ejecutado. Debe ampliarse con fallos de
inicializadores antes de dar por resuelta la ruta. Este párrafo es un plan
técnico, no implementación.
Una reproducción directa de inicializador de campo que llama a un método
`throws` fue rechazada por el análisis, incluso cuando el constructor declara
`throws` (`throwing-field-initializer-probe/results.json`); no se generó ni
ejecutó ese programa. Por tanto, esa reproducción no demuestra una ruta de
unwind válida de inicializador ni autoriza considerar todas las formas de
fallo de inicialización cubiertas.
Para un payload `struct` con destructor, la construcción placement devuelve
`Payload*`. En dos pruebas separadas, `delete *slot` y `delete slot[0]`
compilaron en O0/O2, pero el contador del destructor siguió en cero y el
programa salió con 2 (`shared-value-placement-probe/results.json` y
`shared-value-delete-index-probe/results.json`). La operación explícita
`destroy slot` tenía el mismo resultado en la revisión base; ahora ejecuta
drop in situ sin liberar el almacenamiento y la misma reproducción devuelve 42
en O0/O2 (`shared-value-destroy-compare/results.json`). La regresión prueba el
destructor de la estructura, destrucción de un campo owned, puntero nulo y
reutilización del bloque; también pasó la prueba existente de item 319
(`shared-destroy-test.log`, `shared-item319-test.log`). Estos componentes
internos no declaran todavía soporte de payloads `struct` en familias públicas.
Una especialización controlada distingue una fábrica para clase de otra para
valor con el mismo nombre (`shared-factory-specialize-probe/results.json`);
sin `specialize`, el compilador rechaza esa familia
(`shared-factory-overload-probe/results.json`). Un propietario experimental
con una ranura `T*` común construye y destruye una clase y una estructura en
O0/O2 (`shared-unified-owner-probe/results.json`), pero su retorno `ref[this] T`
es inseguro como API: el compilador acepta asignar otra referencia de clase a
esa ranura (`shared-unified-owner-mutation-probe/results.json`), lo que puede
romper la propiedad del bloque. Esa variante solo se analizó, no se ejecutó.
La escritura `in[this] T` como retorno se rechaza en parsing
(`shared-unified-owner-in-probe/results.json`); esto no prueba que no exista
otra forma segura. La representación experimental no se publicó.
El lenguaje también admite `ref readonly T`: un prototipo posterior leyó una
clase y una estructura en O0/O2 y rechazó sustituir la ranura
(`shared-unified-owner-readonly-probe/results.json`,
`shared-unified-owner-readonly-mutation-probe/results.json`). No se han
acreditado todavía todas sus garantías de préstamo o concurrencia. En concreto,
una referencia de clase puede copiarse desde ese `ref readonly` y conservarse
después de destruir el owner: el análisis acepta el caso
(`shared-readonly-escape-probe/results.json`). No se ejecutó el acceso colgante.
Por decisión posterior del usuario, el resultado de clase de un método de una
vista prestada se trata conservadoramente como préstamo de la vista, tanto si
se devuelve por valor como por `ref readonly`. Un alias local `borrowed` se
admite si la vista es una variable local con propietario local comprobable; se
rechazan escapes a locales sin calificador, campos, parámetros que pueden
retener y retornos. El retorno directamente declarado `borrowed` también se
rechaza hasta demostrar su procedencia entre llamadas. Esta regla puede
rechazar métodos de vista que devuelvan referencias de vida independiente.
`nsc.compiler.borrowed-view-class-result` comprueba ambos retornos, alias y
accesos válidos en O0/O2 y los destinos de escape sin ejecutar accesos inválidos;
la selección dirigida de cinco pruebas pasó (`shared-view-class-tests.log`).
La regla aún no demuestra el contrato completo de los punteros compartidos.
La comprobación de `where T : Send` tiene otra limitación relevante para
`Rc`: en la base `Inner` con un campo `int*` se rechaza, pero `Outer` con un
campo `Inner` se acepta (`shared-send-structural-probe/results.json`). La ruta
`ProjectGenericConstraintModel::satisfiesThreadSafetyConstraint` ahora recorre
campos de tipos nombrados anidados y detecta ciclos; en la comparación rechaza
ambos. La regresión dirigida verifica Send y Sync con uno a tres niveles y
controles válidos (`nested-thread-safety-tests.log`). Esta prueba solo cubre
restricciones genéricas de esos tipos, no todos los genéricos ni
las rutas de traslado entre hilos; el executor tiene otra comprobación. No
constituye por sí sola una garantía suficiente para publicar `Rc`.
Un caso posterior reveló que un arreglo fijo de `Inner` volvía a aceptarse como
Send (`shared-send-generic-probe/results.json`, antes de la corrección). El
analizador ahora revisa el elemento de arreglos fijos anidados. La regresión
dirigida rechaza arreglos de tipos con punteros y de punteros directos, comprueba
arreglos seguros y wrappers genéricos, tanto en Send como Sync. El mismo fallo
se reprodujo con arreglos dinámicos (`shared-send-generic-probe/results.json`);
la comprobación ahora recorre también su elemento. La selección dirigida pasó
2/2 (`nested-thread-safety-tests.log`). Otras representaciones y rutas de envío
no quedan acreditadas por esos casos. La primera compilación incremental falló
por un include ausente; se agregó y la segunda compilación pasó.
La primera ejecución de esta regresión utilizó por error `C:/Windows/Temp`;
`TemporaryDirectory` eliminó su carpeta al terminar. Se repitió con TEMP, TMP,
APPDATA y LOCALAPPDATA en A; el registro corresponde a la segunda ejecución.
Una ejecución posterior de las dos regresiones falló al enlazar la prueba de
vista porque el lanzador no había configurado el runtime aislado; tras añadir
`NSC_WINDOWS_RUNTIME_LIBRARY` y `NSC_LINKER`, ambas pasaron. El fallo fue de
configuración de la prueba y no demuestra un fallo del código de préstamo.
`nsc.sdk.shared-control-storage` añade una regresión dirigida que sitúa los
contadores locales o atómicos junto al payload en la misma reserva; comprueba
la destrucción y la expiración y, para la variante atómica, 200 000 ciclos de
retención y liberación en cuatro hilos. Pasó con ejecución Windows en O0/O2
(`shared-control-test.log`). Además ejecuta 1000 carreras por nivel entre la
última liberación y una retención: comprueba que exista exactamente un último
fuerte y que la retención falle después de llegar a cero. El análisis en otros sistemas no implica que el
ejecutable se haya ejecutado. No demuestra exhaustivamente los intercalados.
Los primeros prototipos usaron `delete` con un arreglo, que no lo libera; por
ello sus salidas no prueban desasignación. La prueba del SDK y las variantes
internas actuales usan `delete[]`; en la ruta excepcional el IR contiene la
llamada de liberación. No se afirma que ASan haya medido ese heap.
Un experimento separado (`shared-heap-lifecycle-probe/results.json`) coloca
contadores y un payload de clase en un arreglo de tamaño fijo; en O0 y O2
ejecuta clonación, destrucción del último fuerte y consulta del débil expirado,
además de upgrade antes y después de expirar, con salida 42. El intento de
copia implícita se rechaza (`shared-heap-copy-probe/result.json`). Una vista
con campo y retorno `borrowed[a]` ejecuta el acceso válido en O0 y O2 y
rechaza almacenar la referencia en un local sin calificador
(`shared-heap-borrow-probe/results.json`). Esas pruebas no demuestran todas
las formas de escape. No es una fábrica productiva: el tamaño fijo puede ser
insuficiente, no comprueba alineación arbitraria, overflow, concurrencia,
excepciones ni pin del módulo; el método de acceso ordinario devuelve una
referencia sin comprobar su vida.
El mecanismo actual de `new (allocator)` tampoco demuestra la asignación
única: `installAllocatorDescriptorSidecar` reserva por separado 80 bytes para
el descriptor y el contexto del allocator. La operación MIR `ObjectDestroy`
ya distingue destrucción object-only, pero aún no existe una fábrica compartida
que la conecte con un bloque de control de vida independiente. Evidencia:
`src/LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp` y
`src/LLVM/Lowering/OopLlvmConstruction.cpp`.
Estos prototipos no son una implementación pública: las comprobaciones de
contadores y upgrade descritas arriba no demuestran la familia Arc completa,
el pin del módulo ni la seguridad general del acceso al payload.

Los componentes internos `SharedLocalControl` y `SharedAtomicControl` conectan
los contadores existentes al ciclo de vida del bloque: el último fuerte destruye
el payload de clase o estructura y suelta el débil implícito; el último débil
libera el arreglo. Sus métodos requieren un bloque vivo y referencias válidas;
no representan por sí solos handles públicos ni fijan el módulo productor.
`nsc.sdk.shared-local-control` ejecuta ambos controles con clases y estructuras,
sin resurrección, doble liberación fuerte ni overflow de retención. La prueba
`nsc.sdk.shared-atomic-control-concurrency` disputa la última liberación entre
cuatro hilos durante 64 rondas en O0 y O2 y observa una destrucción por ronda.
Ambas pasaron junto a las dos pruebas dirigidas previas de almacenamiento y
constructores (`A:/nsc-fixes-evidencia/shared-control-suite.log`, 4/4). Estas
pruebas no agotan los intercalados ni prueban el pin del módulo o la seguridad
de las futuras APIs públicas; el análisis del proyecto Core pasó por separado
(`shared-core-check.log`).

La implementación interna incluye ahora `SharedLocalHandle`, `SharedLocalWeak`,
`SharedAtomicHandle` y `SharedAtomicWeak`, con fábricas para clase y estructura
y variantes que propagan excepciones. Son estructuras move-only y utilizan la
vista interna `SharedBorrow`, cuyo acceso a una ranura nula hace `panic`.
La prueba `nsc.sdk.shared-internal-handles` comprueba clonación, upgrade,
expiración, drops, constructores que lanzan, alias prestado, copia implícita,
escape y movimiento durante el préstamo; ejecuta los casos válidos y vacíos
en O0/O2 (`shared-control-suite.log`). El caso vacío devuelve 142 en ambos
niveles, el código del `panic` con perfil abort (`LlvmTrapReason.h`); la salida
explícita se conserva en `shared-local-handles-test.log`.
El proyecto Core pasó su comprobación semántica (`shared-core-check.log`).
La superficie pública solicitada sigue sin publicarse y faltan pruebas de
concurrencia sobre los handles, vida del módulo y garantías completas del payload.
Una reproducción adicional muestra que tanto los handles locales como los
atómicos se rechazan como `Send`, incluso con payload seguro, porque ambos
contienen un puntero crudo (`shared-handle-send-probe/results.json`). El rechazo
es deseable para los locales; para los atómicos falta un mecanismo que acredite
las garantías del payload sin tratar como seguro un puntero arbitrario. La ruta
del executor es distinta de la de restricciones y también debe comprobarse.
Una prueba concurrente de los handles activó un diagnóstico espurio: el
candidato especializado para clase materializó `SharedClassCreation<BadValue>`
aunque el payload era estructura y correspondía la alternativa de valor
(`shared-handles-failure/`). La traza temporal de genéricos muestra que los
métodos candidatos llevan grupo 5, pero la especialización de tipo anidada
llega con grupo 0 (`shared-handles-failure/generics.log`). Restaurar el grupo
de sobrecarga exterior en el punto ensayado no resolvió el caso y se retiró.
Como medida interna, el contenedor de resultado se dejó sin una restricción que
no utiliza; la fábrica de clases conserva `where T : class`. La reproducción
original con el contenedor restringido se conserva y el defecto del compilador
permanece pendiente, no se considera corregido por ese ajuste.
Una prueba complementaria encontró el problema inverso: un `Box<Wrong>` que
incumple su restricción se rechaza directamente, pero se aceptó dentro de un
candidato elegido (`shared-overload-constraint-probe/results.json`, ensayo
anterior a la corrección experimental). Transportar el grupo por el recorrido
y comprobar declaraciones, construcción y arreglos corrigió esas variantes,
pero `Wrapper<Box<Wrong>>` seguía aceptándose. Se retiró todo el experimento
del compilador y se recompiló incrementalmente la versión anterior; la solución
requiere conservar las dependencias de argumentos anidados y demostrar también
que un diagnóstico válido no desaparezca por diferirse. El ejemplo original
de la campaña no se alteró.
En la revisión actual, `ProjectGenericModel::rewriteSpecializedSyntax` guarda
la identidad de la especialización y después ejecuta `clearTypeArguments()`;
una comprobación tardía que solo inspeccione el tipo externo ya no puede recorrer
directamente `Box<Wrong>` dentro de `Wrapper<Box<Wrong>>`. Una solución debe
preservar esa relación por uso, incluidos caché y metadata precompilada, y
asociar cada diagnóstico al candidato realmente seleccionado. Esto es una
dirección de investigación, no una corrección implementada.
Actualización posterior: `1cedb1269` corrigió las variantes reproducidas de
restricciones anidadas en cuerpos de sobrecargas y restituyó `where T : class`
en `SharedClassCreation`. La investigación y el experimento retirado anteriores
permanecen como historia; la corrección y sus límites se detallan en
`restricciones-en-cuerpos-genericos.md`. No se ha demostrado cobertura de todas
las rutas de caché ni de módulos genéricos precompilados.
Con el contenedor interno sin esa restricción, la regresión de handles ejecuta
4 000 clones atómicos en cuatro hilos, tanto O0 como O2, y verifica que queda
un único fuerte y que el payload se destruye una vez después de los joins. El
fixture comparte un puntero crudo al handle vivo para probar el contador; esto
no acredita que el lenguaje permita transferir un `Arc` seguro entre hilos.
La selección de cinco pruebas dirigidas pasó (`shared-control-suite.log`); el
análisis de Core también pasó. No se agotaron todos los intercalados posibles.
Los handles internos fuertes y débiles exponen ahora `WeakCount`: la prueba
verifica cero, uno y dos débiles y conserva un débil después de destruir el
payload. La variante local lee los contadores sin concurrencia; la atómica
realiza lecturas separadas que pueden observar cambios concurrentes y no se
ha demostrado como una instantánea linealizable. Esa limitación debe resolverse
o definir su contrato antes de publicar la API. Las cinco pruebas dirigidas y
el análisis semántico de Core pasaron de nuevo (`shared-control-suite.log`,
`shared-core-check.log`).
Una prueba controlada mantuvo bloqueado el destructor del último fuerte mientras
quedaba un débil explícito: `WeakCount` devolvió 2 durante el destructor y 1
después, en O0/O2 (`shared-weak-count-race-probe/results.json`). Esto confirma
que la lectura puede incluir el débil implícito transitorio. La elección entre
exponer ese conteo aproximado bajo concurrencia o exigir una observación exacta
estaba pendiente de la respuesta del usuario cuando se registró la prueba.
Decisión posterior: el usuario eligió un conteo aproximado durante operaciones
concurrentes y exacto en ausencia de ellas. La observación de 2 durante el
destructor sigue formando parte de la evidencia del comportamiento permitido;
no es una instantánea linealizable.

## AddressSanitizer

Revalidación dirigida en `215f49ca5`: la selección
`asan-current-direct-20261113.cmd` pasó seis pruebas y omitió la prueba de
crecimiento de `ReadLine` porque faltaba `NSC_TEST_CORE_BINARY`. Se ejecutó
esa prueba por separado con el Core binario aislado de
`A:/nsc-fixes-evidencia/integracion-vistas-sdk`: pasó sin omisión, con salida
42 en O0/O2 tanto en modo normal como ASan. Los comandos y resultados están en
`A:/nsc-fixes-evidencia/asan-current-direct-20261113.log` y
`asan-readline-current-20261113.log`; este último registra hashes de entradas
antes y después. Sigue vigente `detect_leaks=0` y el alcance documentado abajo;
la prueba de lectura no demuestra que la reasignación haya movido el bloque.

La reproducción original del ejemplo 97 se ejecutó posteriormente con
`--sanitize-address` y las dependencias aisladas de la campaña, en O0 y O2.
La copia de `Program.ns` es idéntica al original (`fc /b` sin diferencias).
Ambos builds terminaron con éxito, pero ambas ejecuciones terminaron con 0 y
sin diagnóstico de ASan, aunque el programa escribe `pointer[4]` después de
reservar cuatro enteros. Comandos, salidas y códigos están en
`A:/nsc-fixes-evidencia/asan-original-97-20261111/`; el compilador informó el
hash de build `957f80c6905f7c94`. Este resultado reproduce una falta de
detección del ejemplo concreto; no demuestra por sí mismo cuál de las rutas
de reserva o instrumentación la causa. El ejemplo y la instalación funcional
permanecen intactos, y el caso 97 queda abierto.

El IR no optimizado de la reproducción reserva 24 bytes mediante
`__nativesharp_alloc` y emite la escritura de cuatro bytes al desplazamiento
24 desde el inicio (`original-llvm.log`). El desensamblado y las relocaciones
del runtime enlazado muestran que esa función llama a `GetProcessHeap` y
`HeapAlloc` (`runtime-alloc-relocations.log`). Los dos ejecutables importan la
DLL de ASan y `__asan_report_store4` (`exe-O0-imports.log` y
`exe-O2-imports.log`); la presencia de una importación no demuestra por sí sola
que se haya instrumentado la instrucción concreta. Es coherente con la
limitación de heap previamente observada, pero aún no prueba cómo ASan marca
esa reserva ni permite dar por identificada toda la causa raíz.

Inspección adicional del ejecutable O0: la escritura de `42` está en
`0x140002ba6` y va precedida de una lectura del byte de sombra y una llamada
condicional al thunk de `__asan_report_store4` en `0x1400739e0`. El thunk
salta a la entrada `0x140081908`, correspondiente a esa importación. Como
el proceso terminó con 0, esa comprobación no diagnosticó la dirección en la
ejecución observada. El desensamblado completo y las importaciones están en
`exe-O0-disassembly.log` y `exe-O0-imports.log`. Esto confirma la
instrumentación de esa escritura en O0; no acredita la instrucción O2 ni
identifica todavía el mecanismo correcto para registrar el bloque ante ASan.

Como contraste se tomó una copia del IR no optimizado del ejemplo, se
redirigieron sus llamadas de reserva y liberación a `malloc` y `free`, se
añadió el atributo ASan y un `main` de prueba, y se aplicó el pase ASan con
LLVM 23.1.0. Ese ejecutable terminó con código 1 y el runtime informó
`heap-buffer-overflow` al escribir justo después de una reserva de 24 bytes
(`malloc-experiment.cmd`, `malloc-input.ll`, `malloc-asan.ll`,
`malloc-compile.log` y `malloc-run.log` en la carpeta de evidencia). Clang
avisó que reemplazaba el triple de target. El experimento cambia una copia del
IR y usa un arranque de prueba: demuestra el contraste de ese asignador para
esta reproducción, no corrige ni valida todavía la integración del driver.
El runtime también reserva directamente con `HeapAlloc` en servicios que
entregan memoria a consumidores y usa `HeapReAlloc` y `HeapFree` para la
lectura de consola. Sustituir solo `__nativesharp_alloc` o solo
`__nativesharp_free` mezclaría familias de asignación; se debe resolver la
procedencia y el ciclo de vida de todas esas rutas antes de elegir una
implementación productiva.

El control válido del mismo experimento modifica en la copia del IR el
desplazamiento de la escritura de 16 a 12 bytes desde el primer elemento,
manteniendo la reserva y liberación con `malloc`/`free`. Terminó con 0 y sin
diagnóstico (`malloc-valid-control.cmd`, `malloc-valid-input.ll` y
`malloc-valid-run.log`). Clang emitió la misma advertencia de triple. Este
control no ejercita el runtime productivo ni variantes O2 del experimento.

Se ensayaron además dos alternativas aisladas para emparejar las tres
operaciones del heap del runtime. La primera modificó una copia del IR del
runtime generado por el compilador y mantuvo `__nativesharp_start`; la segunda
aplicó `llvm-objcopy` a una copia de la biblioteca de runtime configurada y
redirigió sus referencias a un adaptador de prueba. En ambas, el ejemplo
informó `heap-buffer-overflow` y terminó con 1. La biblioteca de entrada
conservó el SHA-256 registrado antes del experimento.

Con el runtime generado experimental, un acceso válido terminó con 0; además,
una cadena devuelta por el servicio de directorio y una lectura de 340 bytes
que requiere reasignación se liberaron mediante el runtime y devolvieron 42.
Los mismos dos servicios devolvieron 42 usando la copia de la biblioteca y el
adaptador. Un último fixture modificó una copia del IR para escribir en el
último elemento después de liberar el arreglo: con esa biblioteca ASan informó
`heap-use-after-free` y terminó con 1. Los comandos, fuentes IR y logs están en
`A:/nsc-fixes-evidencia/asan-original-97-20261111/runtime-experiment-*` y
`runtime-archive-*`; las compilaciones IR de Clang registraron advertencia de
triple. Estos experimentos usan arranques o IR de prueba y no demuestran
compatibilidad con todos los flags del heap, runtimes personalizados,
bibliotecas externas ni el driver productivo. No se ha implementado todavía
la transformación en el compilador.

Existe ahora un generador provisional de adaptador en
`WindowsRuntimeBuilder::buildSanitizerHeapAdapter`, todavía desconectado del
driver. Emite rutas de `malloc`, `realloc` y `free` para el heap de proceso y
rechaza flags, heaps y argumentos de realloc no soportados. La construcción
incremental de su prueba y 19 comprobaciones pasaron. El objeto de una primera
versión detectó el desbordamiento del ejemplo en un enlace experimental y el
control de directorio más lectura con reasignación devolvió 42. Un control de
flags no soportados encontró que `ExitProcess` impedía vaciar el mensaje;
después de añadir `fflush`, la segunda versión devolvió 87 con el diagnóstico
visible (`build-heap-adapter-v2.log`, `test-heap-adapter-v2.log` y
`generated-adapter-unsupported-v2-*.log`).

Una reserva ajena realizada con `HeapAlloc` y entregada al adaptador para
liberarla terminó con 1 sin diagnóstico visible
(`generated-adapter-foreign-*.log`). Por ello se añadió una comprobación de
propiedad mediante la interfaz local `__sanitizer_get_ownership`; ese último
cambio **aún no se ha compilado ni probado**. La comprobación puede cambiar el
diagnóstico de una doble liberación y requiere controles explícitos. Tampoco
se ha resuelto aún la identificación fiable de runtimes personalizados ni la
integración del archivo de runtime en el driver. El caso continúa abierto.

La comprobación de propiedad se compiló incrementalmente y la prueba del
generador volvió a pasar 19 comprobaciones (`build-heap-adapter-v3.log` y
`test-heap-adapter-v3.log`). En enlaces experimentales, el control válido de
servicios devolvió 42, la memoria ajena fue rechazada con diagnóstico y salida
87, y el uso después de liberar produjo `heap-use-after-free`. La doble
liberación se detenía con el mismo diagnóstico genérico de memoria ajena.

Se comprobó entonces que `__asan_get_free_stack` distingue en la configuración
de prueba una reserva ajena de una reserva ASan liberada. El generador usa ese
dato para dejar que el interceptor ASan diagnostique la doble liberación. Con
la cuarta versión, compilada incrementalmente, la prueba del generador pasó
19 comprobaciones; el control válido devolvió 42, la reserva ajena devolvió
87 con diagnóstico, y ASan informó `double-free` y `heap-use-after-free` en
los respectivos procesos. Las salidas están en `build-heap-adapter-v4.log`,
`test-heap-adapter-v4.log` y `v4-*-run.log` de la carpeta de evidencia.

Con `malloc_context_size=0` también se observó `double-free`; con
`quarantine_size_mb=0`, el adaptador lo rechazó con el diagnóstico genérico y
salida 87. Por tanto no se garantiza un diagnóstico específico de doble
liberación para todas las configuraciones del runtime ASan. El adaptador sigue
sin conectarse al driver y las comprobaciones anteriores no certifican O2,
integración completa, runtimes personalizados ni la transformación de la
biblioteca en producción.

`tests/integration/asan_heap_out_of_bounds` conserva copias idénticas de la
fuente y el proyecto originales; ambas coincidencias se comprobaron con
`fc /b`. La regresión `nsc.driver-address-sanitizer-heap-example`, preparada
en `address_sanitizer_heap.py`, exige un diagnóstico ASan de desbordamiento de
heap o acceso a memoria envenenada y salida distinta de cero en O0 y O2. Su
ejecución directa de la versión inicial antes de corregir falló en las dos
configuraciones por ausencia de diagnóstico, tras compilar y
ejecutar correctamente los programas (`regression-before.cmd` y `.log` en la
carpeta de evidencia). No se ha ejecutado todavía a través de CTest ni se ha
corregido el runtime; el fixture de compilación se creó desde cero y no fue
incremental. La aceptación posterior de `use-after-poison` evita imponer un
nombre de diagnóstico único a un posible allocator propio; esa versión del
ejecutor aún no se ha repetido, y los logs sin diagnóstico tampoco cumplen su
expectativa.

La prueba dirigida `nsc.driver-address-sanitizer` se repitió con el compilador
aislado SHA-256
`8abb9d728847e1fe3e6ed949a85734bbb08bb738397d3643c3d5c453f8e53cf1`.
Pasó 1/1 sin omisión (`A:/nsc-fixes-evidencia/asan-driver-current-directed.cmd`
y `.log`): usa proyectos mínimos, Clang y su runtime existentes, comprueba la
ejecución válida y el desbordamiento de pila en O0/O2, el empaquetado de la DLL
y los rechazos de opciones y runtime incompatibles. No reconstruye Core ni el
compilador; los binarios de prueba se generan en temporales aislados de A. El
compilador contiene cambios locales y la cobertura no incluye el runtime ni las
bibliotecas precompiladas, que no se instrumentan. La modalidad ASan desactiva
la caché de artefactos por contrato del driver.

Con el compilador incremental SHA-256
`0d48e6a9098d9d24a33be2cc1e4a9d909854c7e92906675e32411b517609fe73`,
`nsc.driver-address-sanitizer` volvió a pasar sin omisión en Windows x64
(`A:/nsc-fixes-evidencia/asan-driver-after-registered.cmd` y `.log`). El
registro contiene las ejecuciones válidas, los diagnósticos del acceso fuera
de límites en O0/O2 y los rechazos de opciones incompatibles y runtimes
inválidos. Se emplearon Clang y su runtime existentes como dependencias de
referencia. La cobertura sigue limitada al código instrumentado: esta prueba
no demuestra instrumentación del runtime ni de bibliotecas precompiladas.

### Integración provisional del adaptador de heap

El 11 de noviembre se conectó el adaptador al enlace ASan. El compilador
inspecciona las referencias del runtime seleccionado, usa `llvm-objcopy` para
redirigir las tres operaciones de heap en una copia temporal y comprueba la
copia antes de enlazarla junto con el adaptador. Los enlaces ordinarios no pasan
por esa ruta. La biblioteca de entrada conservó el SHA-256
`3235a612c2dc62bb05c244885bc135ec76bf847f4f9a700badd9017c1f913a53`.

La compilación incremental de `nsc` y la prueba de unidad se registró en
`A:/nsc-fixes-evidencia/asan-original-97-20261111/integrate-build.log`.
Con el compilador de SHA-256
`37aa0810f459e2a5c1216e7992e2cecfa7817de404c68daaa784d7087d5693b3`,
`ctest -R address-sanitizer -V` pasó 3/3 en
`A:/nsc-fixes-evidencia/asan-original-97-20261111/integrate-tests.log`.
El ejemplo original sin cambios informa `heap-buffer-overflow` en O0 y O2;
un acceso válido devuelve 42 con y sin ASan, y un uso después de liberar
informa `heap-use-after-free` en ambos niveles. La prueba del driver también cubre
stackalloc, controles normales y rechazo de opciones incompatibles.

Esta evidencia corresponde al runtime generado en A y al host LLVM estático.
La presencia de las tres referencias no demuestra que un runtime personalizado
use únicamente ese heap ni que se conserve su semántica de asignación: esa
compatibilidad está pendiente y no debe darse por certificada. La comprobación
posterior `ctest -R address-sanitizer -V` pasó 4/4; su nueva regresión usa el
constructor de ejecutables directamente con runtime integrado, comprueba el
acceso válido (42) y el desbordamiento (`heap-buffer-overflow`) en O0/O2. Otra
comprobación transforma una copia del runtime para eliminar la referencia
esperada a `HeapReAlloc` y verifica que el driver la rechaza sin generar el
ejecutable. Comandos y resultados están en el mismo `integrate-tests.log`.
Un runtime alternativo conservaba las tres referencias de heap, pero sustituía
`__nativesharp_alloc` por una reserva en un bloque estático. El driver lo aceptó
y el programa que escribió fuera de límites devolvió 42 sin diagnóstico ASan:
`A:/nsc-fixes-evidencia/asan-original-97-20261111/custom-runtime-probe.log`.
Se decidió rechazar explícitamente ese contrato no verificado: para una
biblioteca independiente, el compilador genera temporalmente su propio runtime
y compara tamaño y SHA-256 con el configurado; si difieren, no enlaza con ASan.
El runtime integrado sigue generándose directamente en el mismo compilador.
Esta decisión también rechaza runtimes distintos que podrían ser compatibles;
no se afirma compatibilidad de esos runtimes ni se aplica la restricción a
enlaces normales. La generación añade trabajo a cada enlace ASan y requiere el
enlazador de la configuración. Una comparación previa encontró idénticos el
runtime aislado y el regenerado (`canonical-runtime-probe.log`).

Tras la comprobación de identidad, la compilación incremental y las cuatro
pruebas dirigidas volvieron a pasar (`integrate-build.log`,
`integrate-tests.log`); el runtime alternativo se rechazó sin producir
ejecutable. El compilador probado tiene SHA-256
`3d373c1edd67aa9e771fef3a6be422b531ea3fe2814a17424f104cd708687165`.
No se han verificado la ausencia de `llvm-objcopy` ni todas las opciones de ASan.

Se reprodujo que una transformación fallida se atribuía erróneamente a
NSLink: una herramienta de prueba aislada llamada `llvm-objcopy.exe` devolvió
23 y el driver informó `NSLink failed with process status 23`
(`failing-objcopy-probe.log`). Se corrigió la identificación de esa herramienta.
La regresión crea el proceso de prueba en un directorio temporal con espacios,
comprueba el diagnóstico `llvm-objcopy failed with process status 23` y que no
se publique el ejecutable. Las cuatro pruebas ASan volvieron a pasar con el
compilador SHA-256
`656e40ab66bcc6c3628caf7a8c570837bfee937a72d2bdc5acc0dcc23544d692`,
según `integrate-tests.log`. Esto verifica una herramienta que falla, no su
ausencia en `PATH`.

Esa reproducción reveló que la búsqueda de Windows también seleccionaba un
ejecutable desde el directorio del proyecto. La búsqueda de `llvm-objcopy` se
restringió posteriormente a las entradas explícitas de `PATH`, sin modificar
esa variable. La regresión actual deja la herramienta falsa en un directorio
con espacios, verifica que el compilador utiliza la herramienta configurada y
ejecuta el ejemplo instrumentado con diagnóstico `heap-buffer-overflow`.
Las cuatro pruebas dirigidas pasaron con el compilador SHA-256
`9c9b3c483b4adfabe4b70357e977e874deb43bec75efa5927cb9e6a116116c30`
(`integrate-tests.log`). La comprobación previa de fallo de la herramienta es
histórica; la regresión actual verifica la prioridad de búsqueda, y la ausencia
de la herramienta sigue sin probarse.

La reserva de tamaño `SIZE_MAX` mostró una diferencia adicional: `HeapAlloc`
devolvía nulo, pero el adaptador ASan terminaba con `allocation-size-too-big`
(`allocation-failure-run.log`, salida 1). Se añadió al adaptador
`__asan_default_options` con `allocator_may_return_null=1`; esto aplica a las
reservas ASan del proceso y puede ser sobrescrito por `ASAN_OPTIONS`. La prueba
dirigida usa el objeto emitido por el productor real, solicita reservas y
reasignaciones imposibles, verifica los nulos y que el bloque anterior siga
válido, y termina 42. ASan imprime avisos de reserva fallida. Las cuatro pruebas
dirigidas volvieron a pasar con el compilador aislado SHA-256
`e2b8a6ed5db173a2d8a422005b4fbb875b1471235df3da3b33bbe683b87a1a1e`;
comandos y salida en `integrate-tests.log`. No se afirma equivalencia para
fallas de todos los tamaños ni cuando el usuario cambia esa opción.
El adaptador rechaza argumentos de heap o punteros que no puede emparejar y,
con la cuarentena desactivada, puede emitir el error genérico 87 en lugar de
un diagnóstico específico de doble liberación. No se afirma instrumentación de
bibliotecas precompiladas ni del código del runtime. Cambios sin commit.

Con el compilador aislado SHA-256 `40566c10872f48dd5c12909c68983b1f972493d3639ec65997e3c190502d41bd`,
se actualizó incrementalmente el ejecutable auxiliar de las pruebas ASan
(SHA-256 `4d7d31bb74d06b976a2e46ded1023ff91ba072264985a9357dc6ca50c405b08a`)
y se repitieron las cuatro pruebas dirigidas: 4/4 pasaron sin omisiones.
`A:/nsc-fixes-evidencia/asan-before-readonly-update.log` registra diagnósticos de
desbordamiento y uso después de liberar en O0/O2, y el control de reserva
imposible terminó con 42. El log de compilación incremental del auxiliar es
`A:/nsc-fixes-evidencia/asan-current-helper-build.log`. Se usó Clang y runtime
externos ya autorizados y `detect_leaks=0`; no se certifican bibliotecas
precompiladas, fugas ni el runtime personalizado fuera de las condiciones
descritas en esta sección.

Después de los cambios de préstamos indexados se repitieron las mismas cuatro
pruebas dirigidas con el compilador aislado SHA-256
`5bf7b36a3f6bdc33e5f849ea71168f2cb56030adf920a53c5b7eaefee8cd2344`:
pasaron 4/4 sin omisiones (`A:/nsc-fixes-evidencia/asan-current-targeted.log`).
La ejecución conservó `detect_leaks=0` y las limitaciones anteriores; no
constituye validación completa de ASan ni de bibliotecas precompiladas.

Actualización: el adaptador de heap, sus dos regresiones directas y sus
fixtures quedaron registrados en el commit local `b5ea18afe`. Después de los
cambios de tuplas, la selección dirigida de cinco pruebas ASan pasó 5/5 sin
omisiones (`asan-original-97-20261111/asan-current-20261111.log`). La prueba
de destructor que lanza usa otros cambios todavía sin commit y no se incluyó
en ese commit. Se mantuvieron `detect_leaks=0`, el árbol con cambios locales
y el auxiliar previamente construido; no se acredita una revisión limpia ni
se amplían los límites de compatibilidad de runtimes personalizados.

Tras los cambios provisionales de limpieza de excepciones, se repitieron con
el compilador aislado SHA-256
`f8d6020ee7a69d327a21c7cc339eef43306001d8a67e2f25f542269a5afe8580`
las dos pruebas dirigidas `nsc.driver-address-sanitizer-heap-example` y
`nsc.driver-address-sanitizer-throwing-array-release`: pasaron 2/2 sin
omisiones. El ejemplo original informó `heap-buffer-overflow` en O0 y O2; la
segunda prueba informó el acceso posterior a liberación introducido a propósito
tras el destructor que lanza, para elementos struct y clase en O0 y O2. El
comando y la salida están en `A:/nsc-fixes-evidencia/asan-heap-targeted-20261111.cmd`
y `asan-heap-targeted-20261111.log`. Se usó `detect_leaks=0`: esta selección no
verifica fugas, bibliotecas precompiladas ni runtimes personalizados. No se
reconstruyeron el compilador ni el SDK para esta repetición.

El usuario aprobó exposición explícita y desactivada por defecto. La opción
debe seleccionar instrumentación de las unidades LLVM y enlazar la DLL y las
bibliotecas de runtime compatibles suministradas externamente; su ubicación
debe ser configurable y validarse sin alterar `C:/native-sharp` ni Clang.
El entorno contiene Clang 23 y los artefactos Windows `clang_rt.asan_dynamic`
en `C:/clang-llvm/lib/clang/23/lib/windows`, únicamente como referencia.

Tras los cambios provisionales de construcción se repitió únicamente
`nsc.driver-address-sanitizer` con el compilador aislado de hash
`7c969cf811f592e3`, Clang 23 y su runtime externo de referencia. Pasó 1/1 sin
omisión (`A:/nsc-fixes-evidencia/asan-driver-latest.log`): el fixture comprueba
la ejecución válida y el diagnóstico de acceso fuera de límites en O0/O2, la
publicación local de la DLL y el rechazo de opciones incompatibles. Esta
repetición no amplía la cobertura al heap propio de NativeSharp ni al código de
bibliotecas precompiladas que se describen abajo.

Prueba exploratoria posterior: se volcó IR de un programa NativeSharp con
escritura fuera de los límites de un `stackalloc`, se añadió el atributo de
sanitización a una copia del IR, y se aplicó `opt -passes=asan`. Clang enlazó
el objeto, el runtime NativeSharp de A, Shell32 y el runtime externo de ASan;
el ejecutable terminó con código 1 y ASan informó el acceso fuera de límites
(`A:/nsc-fixes-evidencia/asan-probe/run.log`, `link-result.json`). Esta prueba
usa `opt` y Clang directamente; no demuestra integración en `nsc`, NSLink,
productos particionados ni una opción pública funcional.
Un control con acceso válido pasó por el mismo recorrido y terminó con 42
(`A:/nsc-fixes-evidencia/asan-safe/results.json`).

Una segunda prueba mínima intentó enlazar el objeto instrumentado con NSLink
y las bibliotecas ASan y builtins de Clang. El enlazador de referencia rechazó
la variable externa `__asan_shadow_memory_dynamic_address` (NSL1020;
`asan-probe/nslink-dynamic-only-1.log`). Al añadir el thunk estático de Clang y la
importación de excepciones de Windows, ese símbolo dejó de ser el primer
error, pero apareció `__asan_default_options` sin resolver
(`asan-probe/nslink-thunk-import-1.log`). La prueba modifica una copia del IR para llamar
al inicializador y proporcionar un trap mínimo; por eso acota una dificultad
del enlace, pero no prueba que todas las rutas posibles de NSLink fallen.
`src/Linker/LldLinker.cpp` exige actualmente que `NSC_LINKER` sea nslink.exe,
de modo que usar LLD como alternativa explícita para ASan requiere trabajo de
integración y pruebas, no solo cambiar una variable de entorno.

`LlvmNativeSanitizer` inserta el atributo y aplica el pipeline `asan`; la
prueba existente verifica IR. El generador de objetos
dispone de una configuración interna que instrumenta cada unidad antes de la
optimización, desactiva su caché de objetos y rechaza LTO. La prueba
`nsc.project-oop-address-sanitizer` verifica en O0/O2 los símbolos de cada
objeto, los objetos normales antes y después, la reutilización de la caché
normal y el rechazo de ThinLTO (`A:/nsc-fixes-evidencia/asan-unit-test.log`).
El driver conecta esta ruta a `--emit-project-oop-exe` mediante
`--sanitize-address --asan-clang=<clang.exe> --asan-runtime=<DLL>`. La DLL
debe llamarse `clang_rt.asan_dynamic-x86_64.dll` y se publica junto al
ejecutable. El driver consulta a Clang por su DLL y compara tamaño y SHA-256;
acepta una copia idéntica y rechaza un archivo distinto. Se rechazan LTO, PDB, enlace incremental,
subsystem de ventana y caché de artefactos explícita; el modo ASan desactiva
la caché predeterminada. Bibliotecas precompiladas y runtime NativeSharp no se
instrumentan, por lo que los accesos dentro de esas bibliotecas no están
cubiertos. No se ha validado ASan para DLL, bibliotecas estáticas o ThinLTO.

La posición de esa pasada se comprobó con el mismo ejemplo de escritura fuera
de límites: `default<O2>` antes de ASan produjo una terminación sin diagnóstico
ASan (`asan-pass-order/results.json`); ejecutar ASan antes de `default<O2>`
conservó el diagnóstico `dynamic-stack-buffer-overflow`
(`asan-pass-before-opt/results.json`). Ambas pruebas usan IR y Clang externos,
no una opción de `nsc`; la prueba del generador comprueba además que el símbolo
de reporte persiste en el objeto generado en O0 y O2.

La prueba `asan-real-entry/results.json` avanzó a los objetos reales del
generador y a un módulo de entrada producido por `WindowsRuntimeBuilder`.
Un adaptador `main` generado por `WindowsRuntimeBuilder::buildSanitizerEntryAdapter`
llamó a `__nativesharp_start` después del arranque CRT, y
`WindowsExecutableBuilder` creó el adaptador y el objeto de entrada, reunió las
bibliotecas, y el método interno `LldLinker::linkAddressSanitizedExecutable`
invocó Clang/LLD para enlazar el runtime externo. El caso inválido produjo
`dynamic-stack-buffer-overflow` en O0 y O2; el control válido devolvió 42 en
ambos niveles. El fixture invocó el constructor fuera del driver.

La prueba posterior `nsc.driver-address-sanitizer` usa la opción real de
`nsc`, verifica DLL publicada sin añadirla a PATH, caso válido y caso inválido
en O0/O2, ejecución normal sin dependencia ASan y rechazo de varias
combinaciones incompatibles. Pasó junto a la prueba del generador: 2/2
(`A:/nsc-fixes-evidencia/asan-regressions.log`). Requiere configurar
`NSC_TEST_ASAN_CLANG` y `NSC_TEST_ASAN_RUNTIME`; sin ellas CTest la marca
omitida, no aprobada. Esta evidencia usa Windows x64, Clang 23 y el SDK aislado.

Prueba adicional del arranque (`A:/nsc-fixes-evidencia/asan-entry-probe/results.json`):
con el mismo objeto instrumentado y la DLL de ASan en `PATH`, el control con
arranque convencional de Clang enlazó y devolvió 42. Al forzar `/entry:main`,
LLD informó símbolos CRT sin resolver; al agregar explícitamente `libcmt`,
`libvcruntime` y `libucrt`, enlazó pero el proceso terminó con `0xC0000005`.
Esto evidencia que cambiar la entrada o añadir bibliotecas no basta en esta
configuración experimental; no identifica por sí solo la causa de la violación
de acceso ni demuestra que no exista una integración posible. El arranque real
de NativeSharp utiliza `__nativesharp_start` y `/nodefaultlib`.

Una prueba adicional de la vista prestada (`smart-pointer-view-asan-confirm/results.json`)
confirmó que el destructor del payload se ejecutó y después leyó el campo de
ese payload; el proceso devolvió 42 sin diagnóstico ASan. En esta configuración
el runtime de NativeSharp usa su propia ruta de asignación y no se ha acreditado
que ASan observe la liberación de esos objetos. Por tanto, este modo **no
garantiza detectar use-after-free en el heap administrado por NativeSharp**;
la prueba de stackalloc no demuestra cobertura de ese heap.

Las rutas no cubiertas y la compatibilidad entre versiones del runtime deben
permanecer visibles; una ejecución correcta no demuestra detección completa de
todo error de memoria.
Repetición dirigida después de integrar cambios, 2026-11-11:
`shared-throwing-base-after-integration-20261111.cmd` ejecutó
`nsc.sdk.shared-nontrivial-base-throw` (1/1) y el fixture contra el Core
binario aislado preexistente. Rc y Arc pasaron el análisis y ejecutaron 42 en
O0/O2. Los tres logs `shared-throwing-base-*-after-20261111.log` están en
`A:/nsc-fixes-evidencia`. Esta prueba conserva los límites documentados abajo:
el árbol contiene cambios locales, el Core no se regeneró y no se demuestra la
limpieza de todas las formas ni de una segunda excepción durante cleanup.
