# Investigación independiente: cleanup de finally y excepciones

Rama `development-fixes-limitaciones-agente-3`, base
`a56498f051939b8e19410212a5dd1585b1fef117`. El árbol contiene una copia de
trabajo previo sin registrar de la campaña; no es equivalente a un checkout
limpio de esa revisión.

El alcance es el manejo de cleanup y excepciones de los casos async asignados,
sin modificar interoperabilidad C/C++, Daikon, fuentes de LLVM/Clang, la copia
principal ni la instalación de C:. Sólo se autorizan compilaciones incrementales
y pruebas dirigidas. No se realiza push, merge, rebase ni despliegue.

## Reproducción

`A:/nsc-agent3-evidencia/reproduce.cmd` ejecutó
`tests/integration/async_finally_exception_replacement.py` con el compilador
de la campaña `A:/nsc-fixes-evidencia/build/bin/nsc.exe`, runtime y bibliotecas
de importación de la misma zona, y temporales separados de este agente. El
resultado está en `A:/nsc-agent3-evidencia/reproduce.log`.

Las cuatro variantes síncronas `sync_preserve`, `sync_nested_preserve`,
`sync_replace` y `sync_call_replace` devolvieron 42 en O0 y O2.
`sync_inner_catch` fue rechazada por LLVM con `Cleanup funclets for the SEH
personality cannot contain exceptional actions`; `async_ready_replace` y
`async_suspended_replace` fueron rechazadas con `semantic invariant forbids
structured exit across an active cleanup funclet`. Ambos fallos ocurrieron en
O0 y O2. El ejecutable de campaña informa build hash `f41bdab170d74940` y
contiene trabajo local adicional, por lo que no demuestra el comportamiento
de una compilación limpia de la rama de este agente.

## Compilador híbrido de diagnóstico

Se copiaron doce bibliotecas y los objetos de entrada del build de campaña a
`A:/nsc-agent3-evidencia/hybrid/`. `link.rsp` conserva las dependencias de la
regla Ninja existente y `link.cmd` enlaza en esa carpeta. Se compiló
incrementalmente `OopLlvmCleanup.cpp` de esta copia mediante
`compile-cleanup.cmd` y se incluyó ese objeto antes de `nsc_llvm.lib` en el
enlace. El resultado reproducía los mismos controles y fallos: véase
`hybrid/object-baseline.log`.

Este ejecutable es híbrido: las demás bibliotecas, las cabeceras públicas y el
runtime provienen del build y árbol de la campaña con cambios no confirmados.
La evidencia sirve para aislar hipótesis, no acredita la identidad de un build
limpio de la rama, ni el conjunto del compilador o SDK.

## Hipótesis y experimento

`OopLlvmImplementation::emitExceptionalCleanupChain` emite el cuerpo de cada
`finally` primero dentro de un `cleanuppad` que sirve de fallback para
excepciones no aceptadas por el filtro. Después, para `finally` síncrono sin
pad padre, emite otra copia fuera del funclet y la dirige por un catch de
excepciones NativeSharp. Una captura interna de la primera copia puede producir
las acciones excepcionales que LLVM rechaza dentro de ese cleanup.

Como comprobación únicamente diagnóstica se omitió temporalmente esa primera
emisión para `finally` síncrono sin pad padre. Con el ejecutable híbrido,
`sync_inner_catch` devolvió 42 en O0 y O2 y los cuatro controles síncronos
también devolvieron 42. Las dos variantes async continuaron rechazadas. Comando
y salida: `hybrid/compile-cleanup.cmd`, `hybrid/link.cmd`,
`hybrid/reproduce.cmd` y `hybrid/fallback-diagnostic.log`.

La omisión **no es una corrección**: se perdería el cleanup ante excepciones
que no acepte el filtro. Se retiró inmediatamente y `fc /b` confirmó que
`OopLlvmCleanup.cpp` volvió a coincidir con el archivo de la campaña. El objeto
y el ejecutable híbrido se reconstruyeron tras restaurar el archivo; su
SHA-256 es `b0bf37b752f2189736c119b5c0fbf55f1910d111910bc0243c5a6e4bb56a7b26`.

La evidencia acota la causa de `sync_inner_catch` a la emisión del fallback en
este fixture. No demuestra una solución para excepciones ajenas, suspensión,
cancelación, captura de locales ni otros casos SEH. La siguiente tarea es
preservar el fallback sin ejecutar acciones excepcionales dentro del cleanup;
cualquier extracción a otra función debe transportar el estado y soportar las
salidas estructuradas necesarias. El caso permanece abierto.

## Distinción de las causas en async

Otro ensayo temporal omitió la emisión del `finally` en el cleanup nativo
únicamente en corrutinas; tampoco constituye una corrección porque elimina el
fallback. Las dos variantes async compilaron en O0/O2, pero devolvieron 21 y
31 en lugar de 42 (`hybrid/async-fallback-diagnostic.log`).

Una copia diagnóstica del fixture, sin cambiar el original, distinguió captura,
cantidad y log de destrucciones. Para ambas variantes y optimizaciones, la
salida 52 de `hybrid/async-fallback-trace-drops.log` corresponde a excepción
capturada con id 2 y una sola destrucción cuyo id registrado es 2. En ese punto
no se observa la destrucción de la excepción 1. Este resultado no demuestra
por sí solo lo que ocurre tras el fin del proceso. El script y comando están en
`hybrid/async_exception_trace.py` y `hybrid/async-trace.cmd`.

Se retiró la omisión y se reconstruyó el objeto y ejecutable híbrido. El
resultado muestra que el rechazo dentro del funclet es una barrera causal,
pero resolver sólo esa barrera no entrega el contrato de propiedad de la
excepción original. Sigue pendiente conservarla y liberarla correctamente al
reemplazarla sin alterar la limpieza de excepciones externas.

### Ensayo no integrable de propiedad

Con el mismo bypass diagnóstico del fallback async, otro ensayo copió el error
pendiente a `finallyOwnerSlots` antes de emitir la limpieza y lo trató durante
esa limpieza como un propietario activo. Se retiró el dueño temporal al concluir
la limpieza normal. Para las dos variantes de reemplazo las cuatro ejecuciones
devolvieron 42 (`hybrid/async-owner-prototype.log`). También pasaron los 24
controles de finally y diez de catch en O0/O2; los logs son
`hybrid/owner-prototype-finally-controls.log` y
`hybrid/owner-prototype-catch-controls.log`.

Cuatro controles adicionales verifican la preservación tras suspensión, un
catch interno, finally anidados y reemplazo por error de un future hijo: ocho
salidas 42 (`hybrid/async-owner-extra.cmd` y
`hybrid/owner-prototype-extra.log`). Sus fuentes se conservan en
`hybrid/async_owner_extra.py` sin modificar los ejemplos de reproducción.

El ensayo omitió el fallback SEH y por ello **no es una solución ni debe
integrarse**. No acredita cancelación o abandono del finally suspendido,
excepciones ajenas, todos los modos de salida ni todos los niveles de
anidamiento y propiedad. Se conservan los dos archivos experimentales bajo
`hybrid/owner-prototype-Cleanup.cpp` y
`hybrid/owner-prototype-StructuredExits.cpp`. Se retiraron ambos cambios de la
copia de trabajo y se recompilaron los objetos y el ejecutable híbrido; `fc /b`
confirmó coincidencia con los archivos de la campaña. Falta resolver el
fallback SEH de forma válida y comprobar la vida del dueño ante abandono.

La lectura de `ProjectOopHirModel::rewriteAsyncThrow` aporta una ruta concreta:
un `throw` que sale del cuerpo async se transforma en escritura de
`self->error`, publicación del estado fallido y un retorno con las regiones de
cleanup originales. La escritura precede a la ejecución del `finally`; un
segundo throw que sale de ese finally puede escribir el mismo campo. La
estructura provisional reserva `finallyOwnerSlots`, pero no se encontraron
lecturas ni escrituras de propiedad en ellos. Esta relación explica una vía
posible para la pérdida observada y requiere aún comprobación de identidad y
orden en IR, suspensión, captura local y abandono antes de implementar una
liberación.

Con el híbrido restaurado, `hybrid/catch-controls.cmd` ejecutó diez controles
de propiedad de catch, incluidos suspensión, relanzamiento, reemplazo,
cancelación y `Drop`. Los diez devolvieron 42 en O0 y O2; las veinte salidas
están en `hybrid/catch-controls.log`. Este resultado no cubre el fallo de
reemplazo en `finally`, ni convierte el híbrido en un build limpio.

`hybrid/finally-controls.cmd` ejecutó además los 24 controles de la prueba
dirigida de continuación de `finally`: las 48 ejecuciones en O0/O2 devolvieron
42 (`hybrid/finally-controls.log`). Cubren, entre otras rutas, retorno,
excepción, espera suspendida, `break`, `continue` y limpiezas anidadas. No
ejercitan el reemplazo de la excepción previamente activa ni resuelven ese
caso; emplean el mismo compilador híbrido y runtime de campaña.

### Candidato separado con fallback intacto

La regresión `tests/integration/async_finally_pending_exception.py` ejercita un
future hijo que falla listo o tras suspensión durante el `finally` de una
excepción activa, el caso anidado y un control que conserva la excepción sin
reemplazarla. Antes del cambio, los tres casos de reemplazo devolvieron 8, 8 y
7 respectivamente en O0/O2; el control devolvió 42
(`hybrid/pending-exception-before.log`).

Un cambio separado en `emitRemainingStructuredCleanups` conserva el error
pendiente en el slot de finally del frame mientras ejecuta ese cuerpo y usa el
mecanismo existente de propiedad de catch para liberarlo si la salida lo
reemplaza. Tras finalizar el finally sin reemplazo, retira la propiedad
temporal. La ruta SEH nativa permanece intacta. Las ocho ejecuciones de las
cuatro variantes devolvieron 42 (`hybrid/pending-exception-after.log`); los 24
controles de finally y diez controles de catch pasaron de nuevo en O0/O2
(`hybrid/owner-only-finally.log`, `hybrid/owner-only-catch.log`).

El diferencial exacto con el archivo de campaña se conserva en
`hybrid/owner-only.patch`. Depende del código provisional que reserva
`finallyOwnerSlots`, ausente del HEAD de esta rama, y se probó con el compilador
híbrido; no es un commit autónomo ni acredita un build limpio o integrado. El
reemplazo mediante throw explícito y catch interno sigue bloqueado por el
fallo del fallback SEH. No se ha demostrado la liberación al abandonar un
future mientras está pendiente: el intento previo de avanzar manualmente con
`Poll()` fue rechazado por el análisis, lo que no prueba que no exista otra vía.

La dependencia concreta del candidato comprende la declaración
`finallyOwnerSlots` en `OopLlvmCoroutineState.h`, la reserva por finally con
suspensión en `MirFunctionLowerer.cpp::appendCoroutineCatchOwners` y el
mapeo de nodo a slot en `OopLlvmCoroutines.cpp::beginCoroutineDispatch`.
`zeroCoroutineDropFlags` inicializa a cero el frame completo. Estos componentes
forman parte del trabajo sin commit de la campaña; no pertenecen al HEAD limpio.

Se añadió un control de abandono antes de iniciar el future: crea `Work()` y
sale de su ámbito; en O0/O2 devuelve 42 si no hay excepciones construidas ni
destruidas (`hybrid/pending-with-abandon.log`). No ejercita suspensión ni el
slot de finally. Un intento separado de llamar a `Poll()` sobre ese mismo
future fue rechazado en análisis en O0/O2, antes de ejecutar
(`hybrid/abandon-poll.cmd` y `hybrid/abandon-poll.log`). El abandono durante
una suspensión sigue sin verificar; el rechazo no prueba que no exista otro
mecanismo para producirlo.

La prueba se amplió con dos waits dentro del mismo finally y con una excepción
original procedente de un primer future hijo, reemplazada por la excepción de
un segundo future durante cleanup. El primer ajuste cubría el primer caso, pero
el segundo seguía devolviendo 10 en O0/O2 porque la ruta
`emitSuspendingAwaitErrorPath` ejecutaba directamente su cleanup. El ajuste de
esa ruta reutiliza `emitRemainingStructuredCleanups` con una región y
continuación excepcional, manteniendo su recorrido anterior de catches.

Las siete variantes de `async_finally_pending_exception.py` devolvieron 42 en
O0/O2 (`hybrid/pending-child-original3-after.log`), incluidos preservación sin
reemplazo, dos esperas, anidamiento y descarte sin iniciar. El caso original
procedente de await devolvía 10 antes de modificar esa ruta
(`hybrid/pending-child-original2-after.log`). Pasaron nuevamente los 24
controles de finally y diez de catch en O0/O2 (`hybrid/owner-dual-finally.log`
y `hybrid/owner-dual-catch.log`). El ejecutable híbrido final tiene SHA-256
`4d19d61a56f56633b680990ae2a49968a2f00ab866604b6556064f0fe9df663b`.
El segundo diferencial separado está en `hybrid/owner-await.patch`. Persisten
las mismas dependencias no confirmadas y la limitación de abandono suspendido.

Con el mismo híbrido, el control dirigido `exception_suspended_no_clone` del
SDK desde fuentes devolvió 21 para Rc y Arc en O0/O2
(`hybrid/shared-finally-controls.cmd` y `hybrid/owner-dual-shared.log`). No
ejercita clonación dentro de la limpieza ni consumo binario del SDK.

La misma selección pasó con el Core binario preexistente en
`A:/nsc-fixes-evidencia/pointers-sdk-refresh/NativeSharpCore.lib`: cuatro
salidas 21 en O0/O2 (`hybrid/shared-finally-binary.cmd` y
`hybrid/owner-dual-shared-binary.log`). Ese Core no se reconstruyó en esta
tanda y la selección sigue sin cubrir clonación dentro de la limpieza.

Un control adicional comprueba la finalización normal del finally cuando la
excepción original proviene de un await fallido: exige capturar el id 1 y
destruirlo exactamente una vez. Con ese control, las ocho variantes devolvieron
42 en O0/O2 (`hybrid/pending-dual-expanded-final.log`). Aún no se verifican
destructores que lanzan mientras se libera la excepción reemplazada.

## Verificación posterior en el árbol de integración

El compilador de la copia principal se reconstruyó incrementalmente con el
candidato y otros cambios locales. La regresión autónoma, sin especificar una
versión de lenguaje en el proyecto, pasó sus ocho casos en O0/O2 con salida
42; el CTest no se omitió. Comando y resultado:
`A:/nsc-fixes-evidencia/async-pending-integrated-20261111.cmd` y `.log`.
Los controles dirigidos de continuación de finally pasaron 48 ejecuciones y
los de propiedad de catch 20, todos O0/O2 y con salida 42; se conservan en
`async-finally-after-pending-20261111.log` y
`async-catch-after-pending-20261111.log`. El árbol contiene otros cambios sin
registrar: estos resultados no certifican una revisión limpia.

El agente observó además un iterador async que lanza una excepción, produce
un valor dentro de finally y se descarta: inmediatamente después de `Drop()`
se había construido una excepción y no se observó su destrucción, en O0/O2.
La salida 50 y el fixture están en `A:/nsc-agent3-evidencia/hybrid/abandon-yield-finally.py`
y `abandon-yield-selected.log`. No demuestra liberación posterior ni establece
por sí sola el contrato del yield en finally. Se investiga por separado; el
fallback SEH y el abandono suspendido no se declaran resueltos.

## Iterador async: yield dentro de finally

El agente reprodujo que, con una excepción pendiente y un `yield` en finally,
`Drop()` no observaba la destrucción de la excepción. Al reanudar, la siguiente
demanda podía permanecer pendiente: el IR mostraba que se limpiaba el slot sin
publicar la excepción en el future nuevo. Se amplió el tratamiento del slot
al iterador, se devuelve la excepción a la demanda reanudada y se publica su
estado terminal. Se conserva el fallback SEH.

Con el compilador incremental integrado, la prueba autónoma sin versión de
lenguaje pasó nueve variantes en O0/O2: Drop, reanudación, cancelación, varios
yields, finally anidados, excepción originada en await, flujo normal y
reemplazo por un future hijo. Las 18 salidas fueron 42; comando y log:
`A:/nsc-fixes-evidencia/async-generator-integrated-20261111.cmd` y `.log`.
La prueba anterior de excepciones pendientes también pasó de nuevo. El árbol
contiene otros cambios locales y no acredita una revisión limpia. No se
demuestra el abandono durante un await pendiente, el throw explícito que falla
en el fallback SEH ni el manejo de excepciones extranjeras.
