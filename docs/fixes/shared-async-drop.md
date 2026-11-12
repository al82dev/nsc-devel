# Drop de iterador asíncrono con catch activo

El cambio de corrutinas todavía provisional mantiene la excepción activa de un
`catch` que suspende. El control `drop_nested_catches` amplía
`tests/integration/async_catch_owner_continuation.py`: después de obtener un
valor de un iterador con dos catches y tres finally anidados, llama a `Drop()`
y comprueba dos destrucciones y la secuencia de limpieza `32415`.
`A:/nsc-fixes-evidencia/async-catch-current.cmd` pasó los diez casos en O0/O2;
el resultado y sus 20 códigos de salida están en `async-catch-current.log`.

`tests/integration/shared_async_catch_drop.py` prueba adicionalmente `Rc` y
`Arc`: el generador recibe un clon, entra a un catch, produce un valor y se
destruye mientras está suspendido. Se exige que el contador fuerte vuelva de
dos a uno y que el objeto compartido se destruya al salir su dueño restante.
Con las fuentes de Core de esta rama, el fixture también exige destruir una vez
la excepción local de prueba. Las cuatro combinaciones de familia y O0/O2
devolvieron 42 (`A:/nsc-fixes-evidencia/shared-async-catch-drop.cmd` y `.log`).
El fixture incorpora `Send.ns` y `Sync.ns` del SDK para resolver los traits.

La variante `shared-async-catch-drop-binary.cmd` consume el Core binario
preexistente en `A:/nsc-fixes-evidencia/pointers-sdk-refresh/` y pasó las mismas
cuatro combinaciones. Esta variante usa la excepción de Core y no mide su
destructor: acredita los contadores y la destrucción del payload, no la
destrucción de la excepción. Los comandos completos y resultados se conservan
en el log del mismo nombre. El compilador usado es el árbol conjunto sin commit;
la prueba no acredita la procedencia de un Core recién construido.

Esta evidencia cubre `Drop()` después de un `yield` dentro de catch. No demuestra
la limpieza de `finally` que suspende, ni todas las rutas de error, abandono o
anidamiento. Ese defecto sigue abierto y el cambio de corrutinas no está
consolidado en un commit.

La reproducción independiente `exception_suspended_no_clone` se repitió con
el compilador aislado actual y `Rc`/`Arc`, tanto con fuentes del SDK como con
el Core binario preexistente. Las ocho combinaciones O0/O2 devolvieron 12 en
vez de 21: el consumidor no observó la excepción y el contador fuerte seguía
en dos. Comandos y salidas `shared-finally-current-source` y
`shared-finally-current-binary` en `A:/nsc-fixes-evidencia/`. La ejecución no
demuestra por sí sola cuándo o si se libera posteriormente ese propietario.
La regresión fuente incorpora ahora los traits reales `Send` y `Sync`, que son
necesarios para analizar las definiciones de `Arc` y `WeakArc`.

La reproducción se repitió con el compilador incremental actual mediante
`A:/nsc-fixes-evidencia/shared-finally-verify-latest.cmd`: Rc y Arc devolvieron
12 en O0 y O2, frente al 21 esperado. El control independiente
`async-finally-value-verify-latest.cmd` registró 20 para el retorno y 11 para
la excepción cuando el `finally` se suspende; las dos variantes con future ya
listo devolvieron 42 en O0/O2. Los logs de ambos comandos están en esa carpeta.
No se modificaron las corrutinas en esta comprobación; el defecto sigue abierto.

En la implementación actual el resultado o error se escribe y el future se
marca listo o fallido antes de ejecutar las limpiezas; un `await` dentro de la
limpieza reutiliza el estado del future para reanudar. La corrección requiere
conservar la salida pendiente y la identidad de cada instancia de limpieza,
incluidas las rutas excepcionales SEH; esta constatación no constituye una
corrección y el problema permanece abierto.

La lectura conjunta de `lowerTryRegion`, `emitOneTryCatchBody` y
`emitExceptionalCleanupChain` delimita una posible ruta de implementación:
un manejador nativo solo capturaría la excepción NativeSharp y saldría mediante
`catchret`; la excepción y el destino de la salida pendiente se conservarían
en el frame de la corrutina, y el `finally` suspendible se ejecutaría como
código ordinario. El mecanismo existente para un `catch` suspendible ya guarda
un propietario de excepción en el frame y saca su cuerpo del funclet, pero no
proporciona automáticamente esa transformación para un `try/finally` sin
`catch`. Por tanto reutilizar esa infraestructura requiere tratamiento nuevo,
no basta cambiar el bloque de reanudación.

La continuación también necesita distinguir copias del mismo `finally`,
preservar retorno, excepción, `break` o `continue`, y conservar la propiedad de
la excepción cuando otra excepción reemplaza la primera. Antes de adoptar esa
ruta deben verificarse `finally` anidados, await de hijo fallido, cancelación y
abandono, además de las reglas de dominancia del backend Windows. La lectura es
análisis del diseño; no constituye una corrección ni demuestra el tratamiento
de excepciones ajenas a NativeSharp. No se cambió el backend en esta tanda.

La selección `async-finally-exit-scope-current.cmd` volvió a ejecutar salidas
relacionadas en O0/O2 con el compilador aislado: `normal`, `nested_normal`,
`continue`, `break_ready` y `child_error_finally_ready` devolvieron 42;
`break` devolvió 5 y `child_error_finally_suspend` devolvió 11. Una variante
diagnóstica añadida a `async_finally_continuation.py` codifica el número de
iteraciones y ejecuciones de limpieza como `10 + 3 * iteraciones + Log`:
`break_diagnostic` devolvió 18 en O0/O2, compatible en este fixture con dos
iteraciones y dos limpiezas, cuando debería salir tras una de cada una. Su
control `break_ready` devolvió 42. Comandos y salidas en
`A:/nsc-fixes-evidencia/async-finally-exit-scope-current.cmd` y `.log`, y
`async-finally-break-diagnostic-current.cmd` y `.log`. Estos resultados
delimitan el problema: una corrección debe conservar también el destino de
`break`; las pruebas no demuestran una solución ni cubren todas las salidas.

Para distinguir las posibles combinaciones codificadas por el diagnóstico
anterior, `break_trace` registra el número de cada iteración en secuencia y
devuelve 52 únicamente si hubo dos iteraciones y el registro fue `12`. Esa
variante devolvió 52 en O0 y O2; el control `break_ready` devolvió 42. Comando
y salida: `A:/nsc-fixes-evidencia/async-finally-break-trace-current.cmd` y
`.log`. La evidencia confirma esas dos iteraciones y limpiezas en este fixture;
no establece que todas las formas de `break` tengan el mismo comportamiento.

La prueba de sustitución de excepciones quedó conservada como
`tests/integration/async_finally_exception_replacement.py`. Con el compilador
actual falla en las seis combinaciones: la variante síncrona entrega la
segunda excepción pero observa una sola destrucción (salida 52), y las variantes
asíncronas con future listo o suspendido son rechazadas al generar LLVM por una
salida estructurada desde un `cleanup funclet`. El comando y la salida están en
`A:/nsc-fixes-evidencia/async-finally-exception-replacement-current.cmd` y `.log`.
La regresión queda deliberadamente fallida hasta implementar el contrato de
sustitución; no acredita una fuga definitiva después del fin del proceso.

La misma regresión se ejecutó contra el compilador de `baseline-build` mediante
`A:/nsc-fixes-evidencia/async-finally-exception-replacement-baseline.cmd`.
La variante síncrona dio 52 en O0/O2 y las variantes async lista y suspendida
fallaron en ambos niveles con el mismo diagnóstico de salida estructurada del
funclet. La evidencia está en el `.log` correspondiente. El compilador base
tiene build hash `14717501b74375ce` y SHA-256
`1108bd18b8a85c9d2351dd4f3d6b8055b84817be35b5a45faf2a96da123929b5`;
su CMake apunta a `A:/nsc-fixes-baseline`, cuyo HEAD es
`152654e5a0e2d84d4c2f245ea7590488f0497430`. La ejecución usó el runtime de la
carpeta aislada actual, no demuestra la identidad de sus fuentes con la base.
Esta comparación muestra los mismos resultados con ambos compiladores en esa
configuración; no acredita la causa ni corrige el defecto.

Se volvió a ejecutar la selección `throw_during_await`,
`propagate_await_error` y `propagate_with_finally` de `tuple_await_replay.py`
con el compilador aislado actual. Las seis ejecuciones O0/O2 terminaron con
42: el elemento `Guard` previamente construido se destruyó una vez cuando el
future hijo falló, tanto al capturar localmente como al propagar, y el control
con finally no suspendido observó la destrucción antes de la limpieza exterior.
Comando y salida: `A:/nsc-fixes-evidencia/tuple-await-error-current.cmd` y
`.log`. Esta selección no prueba cancelación durante un await ni corrige el
finally que se suspende; el compilador contiene otros cambios locales.

El selector de `tuple_await_replay.py` antes terminaba con éxito si se le
pasaba un nombre inexistente y no ejecutaba ningún caso. Ahora rechaza nombres
desconocidos antes de crear el proyecto; `unknown_case` devolvió código 1 con
`unknown tuple await cases`. La selección válida anterior se repitió y sus
seis ejecuciones devolvieron 42. Este cambio protege la validez de las pruebas
dirigidas y no cambia el compilador ni corrige el finally suspendido.

## Reproducción y trazado posteriores

Con el compilador aislado actual se repitió `exception_suspended_no_clone`:
Rc y Arc devolvieron 12 en O0/O2, frente al 21 esperado
(`A:/nsc-fixes-evidencia/shared-finally-20261111-recheck.cmd` y `.log`). El
control sin punteros repitió retorno 20 y excepción 11 cuando el finally se
suspende, mientras las dos variantes con future listo devolvieron 42
(`async-finally-values-20261111-recheck.cmd` y `.log`).

El fixture reducido `async-finally-20261111/Program.ns` conserva la misma forma
del caso de retorno. Sus volcados HIR y LLVM, obtenidos mediante `dump.cmd`,
aportan evidencia más precisa: HIR escribe resultado 5 y estado listo antes de
entrar a la limpieza. LLVM emite dos copias del mismo `await` de finally, una
para la salida por retorno y otra para la salida normal. Ambas guardan el estado
de suspensión 1, pero el despacho de entrada para 1 apunta a la segunda copia;
tras reanudar por allí se alcanza la continuación normal que publica el valor
predeterminado. Esto explica el resultado 20 observado para ese fixture. No
demuestra por sí solo toda la ruta excepcional, cancelación o casos anidados.

La corrección necesita distinguir las instancias emitidas de una misma limpieza
y conservar su salida pendiente durante la suspensión. `collectCoroutineSuspensions`
asigna hoy el identificador por nodo MIR, mientras `enterCoroutineResumeTarget`
sobrescribe el destino del mismo identificador al emitir otra copia. Una simple
reasignación de destinos puede afectar repeticiones de operandos, bucles y
cancelación, que también usan esos identificadores. No se cambió el backend ni
se presenta esta observación como una solución; las pruebas continúan fallidas.

Se probó después un prototipo que asignaba identificadores por copia y volvía a
publicar el estado de salida al terminar la limpieza. En una selección dirigida,
retorno, excepción explícita, `break`, `continue` y los casos Rc/Arc sin clon
pasaron O0/O2; sin embargo, las limpiezas anidadas y una excepción de un future
hijo fueron rechazadas por el verificador LLVM porque la reanudación entraba
en un funclet sin que su `cleanuppad` dominara todos los usos
(`async-finally-20261111/prototype-initial-finally.log` y
`prototype-initial-shared.log`). Un primer ajuste para excluir los funclets
siguió produciendo el rechazo; otro impidió el salto pero el caso del future
hijo excedió el límite de 120 segundos (`prototype-finally.log`).

Se retiraron únicamente esos cambios experimentales y se reconstruyó el
compilador incrementalmente. La selección restaurada vuelve a los resultados
anteriores: finally anidado normal y retorno con future listo pasan O0/O2;
el error de hijo devuelve 11, y Rc/Arc devuelven 12 en vez de 21
(`restored-finally.log`, `restored-shared.log`). El compilador restaurado tiene
SHA-256 `5e16826475652767add778eaa966c24da404cb769ec13a9ad0fb745116afb74d`.
El diff experimental se conservó en `prototype-with-existing-changes.patch`;
incluye también cambios de trabajo previos de esos archivos y no debe aplicarse
como parche independiente. El defecto sigue abierto: el siguiente diseño debe
sacar la continuación suspendible del funclet y conservar tanto la excepción
como su destino, sin saltar a través del límite nativo.

El volcado posterior `async-finally-20261111/child-llvm.ll` identifica tres
copias del mismo `finally` para el error de un future hijo: una dentro de un
`cleanuppad`, una en la ruta explícita de error y otra en la continuación
normal. La ruta explícita guarda el objeto de excepción y el estado -2 antes
de ejecutar la limpieza; al suspender, el estado se sustituye por el
identificador del await. El despacho vigente apunta a la copia normal. Esto
corrobora dos necesidades distintas: preservar el error pendiente y evitar
una entrada de reanudación que atraviese el límite del funclet. El volcado es
de un fixture reducido, no demuestra cómo tratar todas las excepciones nativas.

La regresión dirigida `async_finally_continuation.py` ahora permite configurar
`NSC_ASYNC_FINALLY_TIMEOUT` y registra cada tiempo agotado como fallo antes de
continuar con los demás casos. Conserva 120 segundos por defecto. Con 0.001
segundos registró los cuatro tiempos agotados deliberados y devolvió fallo
(`timeout-check.log`); con 10 segundos, los tres controles seleccionados
pasaron O0/O2 (`timeout-controls.log`). Esto agiliza los próximos experimentos
sin convertir una espera agotada en éxito ni ejecutar la batería completa.

Con el compilador aislado SHA-256
`40566c10872f48dd5c12909c68983b1f972493d3639ec65997e3c190502d41bd`, las
reproducciones dirigidas se repitieron después de los cambios de préstamos:
`shared-finally-current-recheck.log` vuelve a registrar 12 frente a 21 para
Rc y Arc en O0/O2; `async-finally-values-current.log` registra 20 y 11 en las
rutas de retorno y excepción suspendidas, y 42 para los dos futuros listos.
Ambas selecciones fallan por los casos suspendidos. Se confirmó en código que
`lowerTryRegion` emite la limpieza normal y que la cadena excepcional la emite
dentro de un `cleanuppad`; reanudar directamente desde el despacho a ese cuerpo
violaría el límite del funclet. El resultado no corrige el defecto ni demuestra
un tratamiento para excepciones extranjeras; no se modificó el backend en esta
comprobación.

La selección adicional `async-finally-multiple-current.cmd` con el mismo
compilador y límite de 10 segundos confirma en O0/O2: `nested_normal` y
`return_finally_throws` terminan 42, al igual que el control de futuro listo;
`nested_return` termina 7 y `two_awaits` termina 8, frente al 42 exigido.
El log conserva los diez resultados y la selección devuelve fallo por los cuatro
casos erróneos. Esto verifica el resultado visible de esos escenarios, no la
liberación de todas sus excepciones ni el comportamiento frente a SEH ajeno.

La revisión de la sustitución de excepciones identifica otra restricción para
el diseño: `buildThrowBody` publica el objeto nuevo en el slot TLS antes de
llamar a `RaiseException`; `lowerExceptionalTransfer` solo invoca la entrega o
liberación de propietarios de `catch` mediante `emitCatchOwnershipOnThrow`.
En cambio, `emitExceptionalCleanupChain` ejecuta el `finally` dentro de un
`cleanuppad` y no obtiene ahí un propietario explícito de la excepción previa.
Por ello, consultar el slot después de lanzar el reemplazo no demuestra que se
pueda recuperar el objeto anterior: el diseño debe conservar su identidad antes
de sobrescribir ese estado y distinguir el caso de relanzar el mismo objeto.
Esta es una inferencia a partir del código, no una demostración de la causa de
la salida 52 ni una corrección; quedan por analizar el orden de búsqueda SEH,
excepciones extranjeras y límites entre módulos antes de modificar esta ruta.

El control `async_finally_outer_owner.py` se repitió con
`A:/nsc-fixes-evidencia/async-finally-outer-owner.cmd` y el compilador aislado
SHA-256 `1225f90ae8fcc71472212f77b374f582781c8cddf3d0d0b685cf1201df74b0a1`.
En O0 y O2 las dos variantes de `catch` con propietario exterior devolvieron
42. El `finally` excepcional suspendido devolvió 30: el fixture ya había
comprobado que sus dos pasos se ejecutaron con el propietario exterior vivo,
pero el consumidor observó `caught == 0`, en lugar de la excepción con id 7.
Esa salida ocurre antes de las comprobaciones finales de destrucción, por lo
que no demuestra si el propietario o la excepción se liberaron correctamente.
El comando y las salidas están en `async-finally-outer-owner.cmd` y `.log`; la
ejecución anterior se conservó como `async-finally-outer-owner-previous.log`.

Al revisar la disposición vigente se confirmó que el frame reserva un slot de
propiedad por nodo `CatchRegion`; `beginCoroutineDispatch` calcula esos slots
al final del frame, justo antes de las banderas de destrucción. Un `finally`
sin `catch` no recibe uno mediante ese mecanismo. Además, en la ruta de catch
no coincidente `emitUnmatchedTryCatch` conserva la excepción como valor LLVM y
la usa después de emitir las limpiezas. Por sí sola esa forma no garantiza que
el valor sobreviva a un retorno y una reentrada de la corrutina. Esto delimita
un requisito de diseño, no demuestra una solución ni el tratamiento de todos
los caminos de cancelación o de excepciones externas.

Se añadió `exceptional_finally_outer_owner_lifetimes` al control dirigido para
medir los contadores incluso si la excepción no llega al consumidor. La salida
es 42 únicamente si se captura id 7 y ambos destructores se ejecutan una vez;
de lo contrario suma a 60 indicadores independientes para `Owner.Drops == 1`,
`Exception.Drops == 0` y `capturado == 0`, con pesos 1, 2 y 4. En O0 y O2
devolvió 67, correspondiente en esta ejecución
a un destructor de propietario, cero destructores de excepción y excepción no
capturada. Los dos controles `catch` continuaron devolviendo 42. El comando y
las ocho salidas están en `A:/nsc-fixes-evidencia/async-finally-outer-owner.cmd`
y `.log`, con el compilador SHA-256
`1225f90ae8fcc71472212f77b374f582781c8cddf3d0d0b685cf1201df74b0a1`.
La observación se limita al punto posterior al `await` del fixture y no prueba
qué ocurre con ese objeto después ni los demás caminos asíncronos.

Se ensayó como paso parcial volver a publicar el estado terminal después de la
limpieza en `lowerStructuredExit` y en la salida de error de un future hijo.
El build incremental terminó correctamente; con `nsc.exe` SHA-256
`486e004287e560a03a692f109238a8d173c03d191a38f37c5ef79ba3657a3d4f`, los diez
casos de catch y los tres de error de await pasaron en O0/O2
(`async-catch-current.log`, `tuple-await-error-current.log`). Los controles de
finally con future listo también devolvieron 42, pero el retorno suspendido
siguió en 20 y la excepción suspendida en 11, en ambos niveles
(`async-finally-values-current.log`). El cambio se conserva como trabajo en curso,
no como corrección validada: las ejecuciones aún reanudan la copia equivocada
y falta resolver la limpieza excepcional dentro del funclet.

El siguiente ensayo asigna estados de suspensión distintos por cada emisión
ordinaria de un `finally`, conserva el mapeo al repetir operandos y compara los
estados remapeados de `await` anidados. La publicación del error de un future
hijo vuelve a realizarse tras ejecutar la limpieza. El build incremental terminó
con `nsc.exe` SHA-256
`a6aa4486926d2957838284ef3fa02c556b2ea317d94e4b536b6df53b549794b7`.
La prueba dirigida completa de `async_finally_continuation.py` devolvió 42 para
sus 24 casos en O0 y O2 (`async-finally-directed-copy-ids.cmd` y `.log`), incluidos
retorno, error de hijo, salidas de ciclo, dos await y limpiezas anidadas. Los
dos controles de propietario exterior y los dos controles de catch devolvieron
42 en ambos niveles (`async-finally-outer-owner.log`); la selección Rc/Arc sin
clon devolvió 21 como se esperaba (`shared-finally-current-recheck.log`). Los
controles de catch con suspensión, error de await, cancelación de tupla, y
await anidado o repetido también pasaron. Las pruebas de drop de catch con Rc y
Arc pasaron desde fuentes y con Core binario preexistente; ese Core no se
reconstruyó en esta tanda.

La selección más amplia de propietarios **no pasó**: desde fuentes fallaron
las cuatro combinaciones Rc/Arc y O0/O2 de
`exception_suspended_clone_explicit_ctor`; con el Core binario preexistente
fallaron doce combinaciones de `exception_suspended_clone`,
`exception_suspended_clone_explicit_ctor` y `exception_ready_clone`. El backend
LLVM informó `Cleanup funclets for the SEH personality cannot contain exceptional
actions`. Ese mismo diagnóstico ya aparecía en las mismas variantes binarias en
`shared-finally-binary-current.log` antes de este ensayo; no se afirma su causa
ni se dispone de la misma comparación anterior para todas las variantes fuente.
Comandos y resultados actuales: `shared-finally-regression.cmd`,
`shared-finally-regression-binary.cmd` y sus logs en `A:/nsc-fixes-evidencia`.
La sustitución de excepción sigue fallando en las seis combinaciones de su
prueba dirigida, tal como se documentó antes. El cambio permanece provisional y
sin commit; el éxito de la selección de 24 casos no cubre la ruta SEH, abandono
o todas las formas de propietarios y no constituye cierre de la campaña.

El volcado `shared-finally-binary-ir/llvm.log`, obtenido con
`shared-finally-binary-ir.cmd`, muestra `invoke` dentro de `cleanuppad`: entre
ellos la destrucción de un elemento de tupla y llamadas a `Clone` y `Slow` en
el cuerpo de la limpieza. El diagnóstico de LLVM no identifica por sí solo
cuál de esas acciones dispara el rechazo.

Se preparó una protección específica para el drop glue de estructuras cuando
ya se está ejecutando un `Cleanup` nativo: un helper separado captura una
segunda excepción y aplica el trap 148. El control dirigido
`unwind-struct-double` ahora da 148 en O0/O2; antes el control devolvía 42 al
capturar una excepción (`unwind-struct-before.log`). La versión anterior de ese
control no medía el contador del destructor, por lo que no establece cuál de
las dos excepciones capturó. El build incremental y la selección
`destructor-release-registered.cmd` pasaron; también pasaron la prueba de
destructores de punteros compartidos y las dos pruebas de drop de valores. Los
24 casos de `async_finally_continuation.py` continuaron pasando en O0/O2.
Compilador aislado SHA-256:
`fe497909055c1e93311ba60416006c3ad6f8867abb67fe8929da59f607b1fbe9`.

Este ajuste no resuelve el error del `finally` con Core binario: las doce
combinaciones de las tres variantes con clon siguen rechazadas por LLVM
(`shared-finally-regression-binary-public-ctor.log`). Permanecen llamadas
potencialmente excepcionales en el propio cuerpo del `finally` durante un
desenrollado, además de la suspensión dentro del funclet. No se presenta la
protección del destructor como solución de esas rutas.

El control `unwind-struct-single` comprueba la ruta sin doble excepción: el
destructor se ejecuta una vez y el catch recibe la excepción, con salida 42 en
O0/O2 (`destructor-release-registered.log`). Esto complementa el control de
trap, sin demostrar todas las formas de cleanup.

El runtime vigente confirma la precaución sobre la propiedad de la excepción:
`buildExceptionCurrent` lee un slot TLS, mientras `buildEhFilter` identifica el
código y el payload desde el registro de excepción y puede publicarlo en ese
slot. La lectura aislada del slot desde un `cleanuppad` no identifica el
registro que originó el desenrollado. Una solución de sustitución debe vincular
el propietario al registro o a un catch que lo haya validado y debe conservarlo
al suspender; no se cambió esta ruta por mera lectura del TLS.

Se comprobó una estructura SEH aislada en
`A:/nsc-fixes-evidencia/eh-replace-investigation/catch-fallback-run.ll`: un
`invoke` dirige a un `catchswitch` cuyo filtro, si acepta, hace `catchret` a
una ruta ordinaria, y si rechaza desenrolla mediante un `cleanuppad` hacia el
exterior. La primera llamada acepta y suma 1; la segunda rechaza, ejecuta el
cleanup alternativo y suma 10, y un manejador exterior la captura. La compilación
y ejecución `catch-fallback-run.cmd` devolvieron 42, que exige traza 11. Se
usaron Clang y LLD de la configuración externa autorizada y las bibliotecas de
importación generadas en A. El primer intento de enlace carecía de
`__C_specific_handler`; se incorporó `UCRTBASE.import.lib` de esa misma
configuración antes de ejecutar. Esto demuestra solo esa forma nativa sin
suspensión ni objetos NativeSharp; todavía debe integrarse con el frame,
ownership, rutas anidadas y cancelación, y no es una corrección del compilador.

Como preparación para esa ruta, el cálculo del frame reserva un slot por
`finally` cuyo subárbol contiene suspensión, antes de los slots existentes de
catch. El backend reproduce el mismo inventario y asocia esos slots; el código
todavía no los usa para poseer ni liberar excepciones. En el volcado del
fixture reducido aparece `coro.finally.owner` en el offset 48
(`async-finally-20261111/llvm-owner.ll`). No se añadieron slots a funciones sin
`finally` suspendible. El build incremental terminó con `nsc.exe` SHA-256
`d2c1f717b61f2a4025b97d66c96e1a4149e4f92f8426a21e93aa32b7e9279367`.

En la primera selección dirigida se observó un fallo de ejecución O2 en
`nested_normal`; una selección menor lo pasó, y la selección completa volvió a
fallar en ese caso con otro código. Se detectó que el ejecutor reescribía el
mismo proyecto sin desactivar la caché de artefactos. Se añadió
`--no-artifact-cache` a cada compilación de esa prueba y las 48 ejecuciones
O0/O2 pasaron (`async-finally-directed-copy-ids.log`). El log de la primera
ejecución fallida se conserva como `async-finally-frame-first.log`; la
comparación no demuestra por sí sola que la caché fuera la causa del fallo.
Pasaron también los controles dirigidos de catch, cancelación, drop de Rc/Arc y
propietario exterior. Estos resultados verifican las configuraciones indicadas,
no la integridad general del frame ni el uso futuro del slot.

Para la ruta síncrona se ensayó un `catchswitch` que identifica la excepción
NativeSharp y ejecuta el `finally` fuera del funclet, conservando el cleanup
nativo como fallback para lo rechazado por el filtro. El propietario de la
excepción original se entrega de nuevo si la limpieza termina, o se libera si
un `throw` explícito o una llamada la reemplaza. `sync_preserve`,
`sync_nested_preserve`, `sync_replace` y `sync_call_replace` ejecutaron 42 en
O0/O2; la prueba verifica el orden de destrucción 12 en las sustituciones. Antes
`sync_replace` daba 52 y `sync_call_replace` dio 51 con la primera versión del
ensayo. `sync_inner_catch` aún falla al generar código con el diagnóstico de
acciones excepcionales dentro de cleanup SEH. Las variantes asíncronas de
sustitución siguen rechazadas por la salida estructurada del funclet. Todo está
en `async-finally-exception-replacement-current.log`; la selección completa
termina con fallo y el código permanece provisional.

La selección de 24 casos de `finally` async, la regresión dirigida de
destructores y los controles de catch con suspensión pasaron. También se
reconstruyeron incrementalmente y ejecutaron los controles existentes
`stage45-10.eh-raii-reuse-after-catch` y `stage45-11.eh-cleanup-execution`:
fallan 24/66 y 8/68 comprobaciones, respectivamente, por diagnósticos de
`throws` en análisis. Los binarios de la base aislada producen los mismos
conteos y diagnósticos en las mismas pruebas (`baseline-tests.log` frente a
`tests.log`, en `eh-replace-investigation`). Esta comparación usa el runtime
generado actual en A y no convierte las pruebas fallidas en exitosas. El
compilador del ensayo tiene SHA-256
`507da2e01630a77c8ea403aa1a88f595602b8709f26ead87362e58cccb7943eb`.
No se ha demostrado aún la liberación de la excepción anterior ante una
excepción ajena a NativeSharp durante la limpieza, ni todos los escenarios de
anidamiento, reemplazo o abandono.

Se investigó el caso de un segundo fallo ajeno a NativeSharp durante la
limpieza de la excepción original. La reproducción
`eh-replace-investigation/foreign-unhandled.ns` provoca un acceso inválido y
usa un destructor que lanza; tanto antes como durante el ensayo el proceso O0
terminó con `0xC0000005` (`foreign-release-before.log` y
`foreign-release-attempt.log`). El caso no instala un manejador exterior para
esa excepción y, por tanto, no demuestra que se haya producido el desenrollado
necesario para evaluar la limpieza. El comportamiento O2 tampoco se utiliza
como evidencia porque la lectura inválida permite optimizaciones indefinidas.
Se retiraron únicamente el ensayo de cleanup y la expectativa inválida del
runner; la reproducción y los logs se conservan. También se probó una variante
que intentaba lanzar un alias prestado de la excepción: el análisis la rechazó
por el contrato de propiedad, así que se retiró esa expectativa de ejecución.

Tras restaurar y reconstruir incrementalmente, `destructor-release-registered`
y la selección de 24 casos async volvieron a pasar. La regresión de reemplazo
conserva los cuatro controles síncronos que devuelven 42 en O0/O2 y los fallos
documentados de `sync_inner_catch` y las dos variantes asíncronas. El compilador
restaurado tiene SHA-256
`f8d6020ee7a69d327a21c7cc339eef43306001d8a67e2f25f542269a5afe8580`.
La propiedad ante excepciones ajenas sigue sin verificar; este ensayo no
demuestra que se haya corregido ni que exista una fuga definitiva.

La revisión de la emisión identifica una limitación adicional de la posible
extracción del `finally` a una función auxiliar: la emisión de una función usa
direcciones de parámetros y locales, banderas de destrucción, celdas de captura,
receptor y almacenamiento del retorno propios de esa función. Una auxiliar
debe recibir o reconstruir los datos utilizados por el cuerpo; no puede reutilizar
referencias LLVM locales de la función llamadora. Además, una llamada ordinaria
a una auxiliar que devuelve `void` no representa por sí sola un `return`,
`break`, `continue` o suspensión del cuerpo original. El guard de destrucción
de elementos de arreglo en `emitNativeArrayElementDestruction` sí muestra un
patrón existente de función auxiliar y restauración parcial del estado de
emisión, pero recibe únicamente un puntero y atrapa fallos para terminar con
trap: no demuestra la semántica necesaria para trasladar un `finally` completo.

Los slots `finallyOwnerSlots` aparecen en la declaración del estado y en su
asignación durante `beginCoroutineDispatch`, sin ninguna lectura o escritura
posterior en `src/LLVM`. Por ello reservarlos no conserva aún la excepción en
una suspensión ni garantiza su liberación si se abandona la corrutina. Sigue
pendiente diseñar y verificar ambos caminos junto con el reemplazo de excepción.

La prueba `shared_async_finally_owner.py` reescribe `Main.ns` para cada familia
y caso, pero no desactivaba la caché de artefactos. Ahora pasa
`--no-artifact-cache` al compilador para que los ejemplos sucesivos de esta
prueba dirigida se compilen a partir de su contenido actual; esto no demuestra
que la caché causara los fallos observados anteriormente. Con el compilador
aislado SHA-256 `f8d6020ee7a69d327a21c7cc339eef43306001d8a67e2f25f542269a5afe8580`,
`shared-finally-no-cache-20261111.cmd` ejecutó el caso suspendido sin clonación
para Rc y Arc, O0 y O2: las cuatro ejecuciones con fuentes y las cuatro con el
SDK binario devolvieron 21, el valor esperado. La variante con clonación y
constructor explícito, mediante `shared-finally-clone-no-cache-20261111.cmd`,
falló en compilación en las ocho combinaciones de familia, optimización y
fuente/binario con el diagnóstico de acciones excepcionales en cleanup SEH.
Los cuatro logs conservan cada resultado en `A:/nsc-fixes-evidencia`; el SDK
binario usado se generó con un compilador anterior. El ajuste del ejecutor no
corrige ese defecto de generación ni acredita el cierre de las cuatro familias.

Para investigar la extracción de la limpieza sin alterar el compilador, el
ensayo LLVM aislado `eh-replace-investigation/finally-outline-nested.ll` genera
una excepción exterior y hace que un filtro la rechace; el `cleanuppad` llama
una función auxiliar con la dirección de una variable local. La auxiliar
modifica esa variable, lanza y captura una excepción interna diferente y
termina; después el desenrollado llega a un manejador exterior que sólo acepta
el código de la excepción original. La salida 42 exige tanto esa identidad como
la secuencia de modificaciones 123. Con Clang/LLD de la configuración ya
autorizada, el ensayo devolvió 42 en O0 y O2; el comando y los resultados están
en `finally-outline-nested.cmd` y `finally-outline-nested-O0.log`/`O2.log`.
Es evidencia del mecanismo SEH aislado, no del código generado por NativeSharp.
No demuestra transferencia de todos los locales y celdas, salidas estructuradas,
suspensión, cancelación, ownership de objetos ni el comportamiento ante una
excepción que escape de la auxiliar. El caso real de Rc/Arc sigue fallando y no
se implementó aún esta estrategia en el compilador.

Una variante posterior del mecanismo aislado deja escapar una excepción nueva
de la auxiliar llamada desde el cleanup. El primer fixture sólo aceptaba la
excepción nueva en la captura exterior, por lo que la excepción original no
tenía handler y la ejecución no demuestra que se iniciara su desenrollado:
O0 terminó con `0xE0000001` y O2 agotó el límite de 30 segundos. Ese resultado
no debe interpretarse como fallo de propagación de la excepción nueva.

La variante corregida acepta también la excepción original y exige que el
filtro exterior observe la nueva y que el local cambie de 1 a 12. Sin impedir
inlining, O0 devolvió 42 pero O2 terminó con `0xE0000001`; el IR optimizado
muestra que el llamador con el cleanup fue incorporado en `main`. Con `noinline`
en el llamador y la auxiliar, ambas configuraciones devolvieron 42. Al aislar
las dos fronteras, `noinline` sólo en el llamador también dio 42 en O0/O2;
sólo en la auxiliar dio 42 en O0 y agotó el límite de 30 segundos en O2.
Los procesos temporizados fueron detenidos por el límite del ejecutor.

Fuentes, comandos y salidas independientes: `finally-outline-escape*.ll`,
`.cmd`, `.py` y `.log`, además de `finally-outline-escape-optimized.ll`, en
`A:/nsc-fixes-evidencia/eh-replace-investigation/`. Clang informó que sustituía
el triple de destino. La comparación es evidencia del fixture nativo y de una
sensibilidad a la optimización en esa forma; no demuestra que `noinline` sea
necesario o suficiente en general, ni que el emisor de NativeSharp preserve
locales, propiedad, salidas estructuradas, suspensión o excepciones extranjeras.
El compilador no se modificó para este ensayo y los casos reales siguen abiertos.

La reproducción completa de sustitución se repitió con el compilador aislado
actual (`async-finally-20261111/replacement-after-asan.log`): cuatro controles
síncronos pasaron en O0/O2; el catch anidado volvió a fallar en LLVM y las dos
variantes asíncronas siguieron rechazadas por salida estructurada de un
funclet. No se ha acreditado una corrección de esas rutas.
