# Continuación de finally durante la cancelación async

Estado: implementación en curso, sin certificación de todas las formas de
`yield` ni del abandono de un hijo pendiente. El árbol de referencia antes de
este ensayo es `2470df502`.

En `215f49ca5`, la reproducción aislada de una excepción lanzada durante la
limpieza de `Drop()` volvió a dar 30 en O0/O2: el finally y el destructor del
guard se ejecutan, pero el catch no recibe la excepción ni se destruye esta.
Los comandos y resultados están en
`A:/nsc-fixes-evidencia/async-drop-cleanup-error-current-20261113.log`.
La operación sigue pendiente: la llamada a `Drop` no declara el efecto
`throws` y su emisión usa una llamada sin arista de unwind. La reproducción
no acredita el comportamiento de un finally que queda suspendido.

Se ensayó recuperar el error del future de demanda, liberar el frame y
propagarlo por `Drop`, haciendo que el análisis exija manejar su tipo de
error. Los casos de Drop explícito, cancelación previa, Drop repetido y Drop
implícito devolvieron 42 en O0/O2. Con una excepción ya activa, la limpieza
terminó con 148 en vez de alcanzar el resultado esperado; nueve controles
existentes por nivel, que hacen Drop después de completar el iterador,
dejaron de compilar por la nueva exigencia de `throws`. La propuesta se retiró
y se reconstruyó incrementalmente el compilador restaurado; el control existente
volvió a pasar O0/O2 (`async-drop-restored-control-20261113.log`). No es una
corrección entregada. El parche, comandos y resultados se conservan en
`A:/nsc-fixes-evidencia/async-drop-error-prototype-20261113.patch`,
`async-drop-error-matrix-20261113.log` y
`async-drop-existing-control-20261113.log`. Quedan por resolver el contrato y
la propiedad de errores durante otra excepción, el análisis del estado
terminado y la suspensión en cleanup; el código 148 por sí solo no identifica
la instrucción que provocó la terminación.

El control `async-cancel-finally-current-20261113.log` devolvió 22 al cancelar
antes de entrar al `finally` y 23 al cancelar cuando ya había comenzado, en
O0 y O2. La ejecución normal devolvió 42. Se conserva la reproducción original.

El ensayo mantiene el motivo de cancelación mientras un `await` en `finally`
se suspende, reanuda la limpieza y descarta los valores cuya propiedad puede
identificar. En las rutas de clase aceptadas, un dueño transferido se destruye
una vez y un alias estático no se destruye. La forma directa `move` se admite
solo para un local dueño que el MIR identifica como transferencia y anula.
El alias de un dueño local acreditado se evalúa sin destruirlo: su región de
cleanup conserva la responsabilidad. La reproducción devolvía 22 en O0/O2;
la regresión incluye cancelación antes y durante el finally y ejecución normal
en `async-cancel-local-owner-alias-regression-20261113.log`.
Un alias no dueño copiado de un campo estático también devolvía 22. La prueba
de binding acredita `LocalValue` sin propiedad ni transferencia; la regresión
`async-cancel-local-borrow-alias-regression-20261113.log` verifica cancelación
antes y durante finally, ejecución normal y que el objeto no se destruya.

Una selección entre un dueño fresco y un alias local se comportaba como
operando desconocido y publicaba otro valor al cancelar: ambas ramas devolvían
22 en O0/O2 (`async-cancel-conditional-local-before-20261113.log`). Ahora el
plan reconoce un alias local en cualquiera de las ramas solo cuando su nodo,
tipo y binding coinciden. La condición y la rama escogida se evalúan una vez;
el dueño fresco se destruye y el dueño local conserva su cleanup. La regresión
`async-cancel-conditional-local-regression-20261113.log` cubre ambas ramas,
posición invertida, alias sin dueño, falla de fábrica y cancelación antes y
durante finally. La falla de la condición pasó O0/O2 en
`async-cancel-condition-throw-20261113.log`. Las demás formas siguen sujetas a
la evidencia de propiedad existente.

La selección entre `new Payload()` y un alias también devolvía 22 para ambas
ramas al cancelar (`async-cancel-conditional-new-before-20261113.log`). La
construcción directa solo se trata como dueña si el nodo y binding coinciden,
el objeto y su almacenamiento son owned, y la construcción no usa placement ni
promoción a stack. `async-cancel-conditional-new-regression-20261113.log`
registra diez casos O0/O2: ambas posiciones y dos construcciones, alias local,
cancelación durante finally, ejecución normal y excepciones de condición o
constructor. El CTest dirigido pasó en
`async-cancel-conditional-new-ctest-20261113.log`. Las construcciones con
suspensión o las formas de propiedad no acreditadas siguen fuera de esa ruta.
Los resultados escalares, las referencias locales y los temporales de valor
probados tienen rutas propias; las formas desconocidas no se declaran cubiertas.

`async-cancel-mixed-move-test-20261113.log` registra diez casos de propiedad y
alias O0/O2; `async-yield-eh-discard-controls-20261113.log` repite los controles
tras el último cambio. Los archivos `async-cancel-reference-test-20261113.log`,
`async-cancel-scalar-types-after-20261113.log` y
`async-cancel-value-owned-test-20261113.log` registran las pruebas dirigidas de
referencias, escalares y estructuras. El wrapper con concatenación comprueba
efectos y orden, no acredita por sí solo la liberación de la cadena.

Una llamada que lanza después de un primer `yield`, seguida de un `finally`
que produce un literal entero, terminaba anormalmente en O0/O2 antes de
entregar ese valor (`async-cancel-exception-pending-probe-20261113.log`). El
puente de excepción rechazaba esa forma de `yield`. El ensayo la admite y
conserva el dueño de la excepción hasta la reanudación, la cancelación o el
`Drop`. `async-yield-eh-owner-nested-20261113.log` registra nueve casos O0/O2,
incluidas excepciones de un hijo suspendido, reemplazo y finally exterior.
Los controles dirigidos anteriores de excepciones y de yield desde finally
pasaron en `async-yield-eh-controls-20261113.log` y
`async-yield-eh-generator-controls-20261113.log`.
El control del ejemplo 88 conservado pasó sus cuatro casos en O0 y O2
(`async-example88-continuation-control-20261113.log`).

Tras reorganizar los helpers y completar el tipo interno del plan, el build
incremental pasó en `async-cancel-plan-refactor-build-20261113.log`. Se volvieron
a ejecutar los controles dirigidos de propiedad mixta, valores, referencias y
excepciones: `async-plan-refactor-mixed-20261113.log`,
`async-plan-refactor-value-20261113.log`,
`async-plan-refactor-reference-20261113.log` y
`async-plan-refactor-exception-20261113.log`. Todos pasaron en O0 y O2; estas
ejecuciones no constituyen una batería completa del compilador.
Las regresiones enfocadas quedaron registradas como
`nsc.compiler.async-cancel-finally-regressions` en CTest. Su orquestador usa
un directorio temporal independiente y, por el contrato actual de esos
runners, se omite fuera de Windows o si el compilador y el temporal no están
en A. El test registrado pasó con el build aislado en
`async-cancel-finally-ctest-confirm-20261113.log`. `CMakeLists.txt` es un
archivo compartido que debe revisarse al integrar con interoperabilidad.

El `move` directo de una estructura local funciona en los casos dirigidos con
y sin campo dueño, constructor fallido, ejecución normal y cancelación después
de producir el valor: once casos O0/O2 en
`async-cancel-value-move-owned-started-20261113.log`. En una reproducción
distinta el verificador LLVM sigue rechazando el `move` de estructura por
dominancia de un funclet; esa forma no se declara corregida. Un ensayo inicial
provocó un error fatal de LLVM en esa reproducción y se limitó para recuperar
el diagnóstico anterior (`async-cancel-owned-move-guard-20261113.log`).
Siguen pendientes las demás formas de operando sin evidencia de propiedad.
El IR rechazado muestra una entrada directa desde el despacho de la corrutina
a un bloque que usa un pad SEH no dominante. La copia de cleanup contiene
además un `invoke` anidado en ese funclet. La solución debe preservar la
limpieza durante excepciones extranjeras sin suspender dentro del funclet;
retirar el token del pad por sí solo no demuestra corrección. El IR y el
ensayo temporal están en `async-value-move-ir-probe-20261113.log`; la
instrumentación se retiró después de capturarlo.
`Drop` ejecutado mientras un `finally` espera a un hijo suspendido devuelve
antes de completar la limpieza: la traza observada es 12 frente a 1234 en O0
y O2 (`async-cancel-drop-await-trace-20261113.log`). El drop libera después el
frame del iterador; no se ha verificado la liberación del frame hijo. No se
considera resuelto hasta definir y validar el ciclo de vida sin introducir una
espera síncrona indefinida. Los archivos y resultados de evidencia están bajo
`A:/nsc-fixes-evidencia` y no sustituyen los ejemplos originales.

En `27062a1bb`, la variante `late_owned_move` añadida a la regresión dirigida
reproduce la falla del verificador en O0/O2; las otras once variantes ejecutaron
42 en ambos niveles (`async-value-late-regression-before-20261113.log`). La
prueba queda deliberadamente fallida mientras se corrige el defecto.
Un ensayo conservó el descarte del valor dentro de la copia de finally del
funclet, sin suspensión, pero LLVM abortó porque el cleanup SEH contiene
acciones excepcionales anidadas. El diagnóstico está en
`async-value-move-current-20261113-O0.log` y el parche retirado en
`async-value-synchronous-funclet-trial-20261113.patch`, ambos en
`A:/nsc-fixes-evidencia`. Se restauró el código y se compiló incrementalmente.
El descarte por sí solo no resuelve el caso: también deben separarse las
operaciones que pueden lanzar de la región SEH y conservar la propagación y
limpieza de excepciones NativeSharp y extranjeras. No se declara corregido.

Un ensayo posterior separa la llamada del cleanup SEH mediante una función
guardada y habilita el puente de excepción para la forma de valor movido. La
cancelación y la propagación desde una llamada sin handler local pasaron; el
control de reemplazo del error también pasó en O0. La selección CTest dirigida
de cancelación pasó tras corregir una regresión inicial. Sin embargo, una
llamada que lanza dentro de un try con catch local omitió el valor del finally
(`async-value-exception-local-handler-20261113.cmd`, salida 21 en O0). El
ensayo permanece sin commit y no debe considerarse seguro: falta conservar el
yield y dirigir el error pendiente al handler local después de reanudar. Los
archivos de ensayo están bajo `A:/nsc-fixes-evidencia`.

La corrección posterior conserva la excepción antes del cleanup, mantiene su
propietario durante el yield y vuelve a lanzarla al dispatcher local después de
reanudar. El recorrido de cleanup solo incluye las regiones que se abandonan.
Para la copia de desenrollado SEH del finally comprobado, el yield se descarta
sin suspender y la llamada directa al constructor se protege mediante una
función que termina con trap si lanza durante ese desenrollado. Formas que no
cumplen esa comprobación no se dan por cubiertas.

Con compilador incremental build hash `43320fc7039b7771`, la selección CTest
`nsc.compiler.async-cancel-finally-regressions` pasó 1/1 sin omisión
(`A:/nsc-fixes-evidencia/async-cancel-finally-ctest-confirm-20261113.log`).
Incluye 18 variantes de valores en O0/O2: cancelación del valor movido con
campo owned, excepción desde llamada con y sin handler local, reemplazo del
error y cancelación después del yield. Los casos comprueban capturas, efectos
y destrucción de los objetos y excepciones. El control anterior que devolvía
21 con handler local ahora devuelve 42. Se mantiene el límite de formas de
finally reconocidas, y no se ejecutó una excepción extranjera real: la emisión
de su ruta no acredita por sí sola su comportamiento en ejecución. El `Drop`
con un await hijo pendiente sigue abierto.

La prueba `async_cancel_moved_value_binary.py` repitió cancelación y excepción
desde llamada consumiendo Core binario de `integracion-vistas-sdk`; las cuatro
ejecuciones O0/O2 devolvieron 42 (`async-value-binary-20261113.log` en A). Usa
la excepción del SDK, por lo que no cuenta su destructor; tampoco prueba el
handler local, reemplazo ni carga del iterador desde otra biblioteca binaria.

## Reemplazo después de un await en finally

En `83ef5c234`, las variantes `async_ready_replace` y
`async_suspended_replace` de `async_finally_exception_replacement.py`
seguían rechazándose en O0/O2 por una salida estructurada desde un funclet.
El log de esa repetición se sobrescribió durante el ensayo; la salida histórica
del mismo rechazo se conserva en `async-finally-exception-replacement-current.log`.
El control `async-replacement-preconstructed-20261113.cmd` trasladó la creación
de la primera excepción antes del try y devolvió 42; ese control no sustituye
los ejemplos originales. El MIR identificó en el original un constructor local
que solo guarda un entero en un campo directo y no tiene cleanup ni inicializador
estático; la arista excepcional de esa llamada generaba el cleanup incompatible.

Durante la emisión de una corrutina con finally suspendible, el compilador
comprueba esa forma concreta del cuerpo MIR y la ausencia de inicializador
estático antes de emitir la llamada directa. No presume que un constructor sin
`throws` sea incapaz de lanzar. Las variantes originales con await listo y
suspendido ejecutaron 42 en O0/O2; también pasaron los cuatro controles
síncronos seleccionados (`async-finally-replacement-directed-20261113.log`).
`async_finally_constructor_unwind.py` comprobó en LLVM que el constructor
acreditado usa llamada directa, mientras las variantes con inicializador
estático, llamada adicional o base conservan `invoke`
(`async-finally-constructor-unwind-final2-20261113.log`). Pasó además la selección
CTest dirigida de cancelación antes de reforzar la comprobación del símbolo; los
dos runners anteriores se repitieron después con build hash `46b9754cbc121623`.

El control con inicialización estática que lanza conserva `invoke`, pero terminó
con excepción no manejada tanto en el estado anterior como con este cambio; su
semántica sigue abierta (`async-no-unwind-static-init-20261113.cmd` e IR
`async-no-unwind-static-init-ir-20261113.log`). `sync_inner_catch` falló en
`83ef5c234` con el diagnóstico de acciones excepcionales en un funclet y no se
repitió en la selección aprobada. La demostración se limita a los constructores
y configuraciones comprobados y no cubre excepciones extranjeras reales.
