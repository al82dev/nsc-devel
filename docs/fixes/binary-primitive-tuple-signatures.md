# Firmas binarias con tuplas de valores escalares

Un consumidor de biblioteca NativeSharp no encontraba métodos cuyos parámetros eran tuplas, aunque la metadata contenía las firmas. Con el compilador aislado se reprodujeron en O0 y O2 los rechazos de `ReadInts((int,int))` y `ReadNested(((int,int),int))`: `A:/nsc-fixes-evidencia/binary-tuple-primitive-before.log`. Se conserva la reproducción original; el fixture nuevo es una copia ampliada.

El conversor de claves estructurales reconstruye con una lista de trabajo las tuplas cuyos elementos son numéricos o booleanos, incluidas tuplas anidadas. No se cambia el formato ni se introduce una versión nueva de metadata. Esta etapa sigue rechazando elementos nominales, vistas, punteros y arreglos: habilitarlos requiere completar su reconstrucción y demostrar su contrato de vida. Las tuplas con vistas en particular permanecen pendientes; la protección provisional de llamadas binarias no constituye solución para lecturas válidas.

La prueba `nsc.sdk.binary-tuple-primitive-roundtrip` construye un productor y consumidores separados en O0 y O2, sin caché, y ejecuta cuatro casos por configuración: parámetro de enteros, tupla anidada, retorno de tupla y parámetro mixto de entero y booleano. Los ocho ejecutables devolvieron 42. La prueba registrada y los controles `nsc.ns3.t44.metadata-closure` y `nsc.generics.tuple-metadata-specialization` pasaron: `A:/nsc-fixes-evidencia/binary-tuple-primitive-ctest.log`. El control adicional `binary-tuple-primitive-view-control.cmd` confirmó que el método numérico se resuelve y las firmas con vistas siguen sin materializarse, incluidas las del productor con contrato incorrecto; no ejecutó llamadas inseguras.

La compilación fue incremental y utilizó el árbol aislado con cambios locales aún pendientes. La ejecución no prueba todos los tipos numéricos, aridades, campos, DLL ni combinaciones de optimización. Este resultado no completa el soporte general de tuplas binarias ni la campaña.

La reproducción se repitió con el compilador incremental SHA-256
`0d48e6a9098d9d24a33be2cc1e4a9d909854c7e92906675e32411b517609fe73`
(`A:/nsc-fixes-evidencia/binary-tuple-view-current.cmd` y `.log`). El control
de tupla numérica pasó el análisis, pero la lectura válida con `SharedView`
y las ocho llamadas a los métodos `Read` y `Save` con Rc/Arc, con argumento
directo o local, siguen rechazadas con `no compatible member`. Los productores
y Core binarios son artefactos aislados preexistentes; esta repetición no los
regeneró ni ejecutó llamadas inseguras.

La ruta `sourceTypeForMetadataKey` de `MetadataSourceMapping.cpp` solo
reconstruye como elementos de tupla números y booleanos; rechaza el resto antes
de materializar la firma. Además, `EscapeAnalyzer.cpp` rechaza de forma
provisional las tuplas con vistas al llamar a un método precompilado aunque
esa firma llegue a estar disponible. Por ello ampliar solo la conversión no
haría funcional la lectura válida, y retirar la segunda protección sin
acreditar la procedencia y el contrato de escape podría aceptar el productor
`Save` que conserva la vista. Ambas condiciones siguen abiertas; no se cambió
ninguna de ellas en esta comprobación.

Un productor nuevo con una tupla que contiene un `borrowed struct View` y
`Save` que asigna el parámetro a un campo estático se rechazó al compilar con
`tuple containing a borrowed view cannot be stored outside a tracked local`.
La prueba `tests/integration/tuple_borrow_escape_summary.py` conserva ese
control negativo separado; antes verifica que el productor seguro publica
`NoEscape` para `Read` y `ReadForward` y los resúmenes esperados para retornos
y alias. Pasó con el compilador de hash anterior sin caché
(`A:/nsc-fixes-evidencia/tuple-view-summary-save.cmd` y
`tuple-view-summary-save-reject.log`). La primera tentativa había supuesto que
el productor inseguro se compilaría para publicar `Global`; su rechazo real
está en `tuple-view-summary-save.log`.

Este resultado acredita la protección en ese productor nuevo, no convierte en
confiable un binario anterior ni demuestra la propagación del resumen por la
firma de `SharedView` materializada. La lectura binaria válida y la protección
de consumidores que reciban metadata antigua permanecen pendientes.

La inspección del productor histórico muestra que la tupla contiene una clave
nominal interna de la especialización de `SharedView`, no su escritura fuente
con argumentos (`A:/nsc-fixes-evidencia/tuple-view-metadata-inspect.log` y
`tuple-view-library-abi.log`). `--verify-library-metadata` informó cero
especializaciones en aquel productor. El productor mínimo se recompiló desde
una copia de sus fuentes, sin caché y usando el compilador actual y el Core
binario aislado preexistente; su metadata verificó correctamente y registró una
especialización (`tuple-view-fresh-producer.cmd`,
`tuple-view-fresh-producer-build.log` y `tuple-view-fresh-producer-metadata.log`).

El consumidor del productor recompilado sigue resolviendo la tupla numérica y
rechazando la lectura con vista con `no compatible member`
(`binary-tuple-view-fresh.cmd` y `.log`). Los controles `Read` y `Save`
restantes de ese script siguen usando el productor histórico inseguro, no el
recién generado. La diferencia de conteo no demuestra por sí sola que la
especialización registrada sea exactamente la requerida ni que pueda usarse
sin validar el resumen de escape; no se reconstruyó la firma ni se cambió la
protección del consumidor.

La inspección `tuple-view-producer-generics.cmd` identifica la especialización
publicada por el proyecto recompilado como `NativeSharp.Core.SharedView` con
argumento `TupleReader::Payload`; su nombre especializado coincide con la
clave nominal del parámetro. El siguiente ajuste experimental queda limitado a
materializar en una tupla esa especialización exacta cuando conste en la
metadata del propio productor. Conserva el rechazo de llamadas con vistas
precompiladas y no infiere argumentos del nombre físico. Se protegerán los
casos numéricos existentes y el productor histórico sin especialización. Este
paso no constituye la corrección de la lectura válida hasta que el resumen de
escape y la ejecución binaria estén comprobados. No se modifica C/C++ ni sus
fixtures, el formato de metadata o el SDK instalado.

El ajuste experimental en `MetadataSourceMapping.cpp` compila
incrementalmente (`tuple-view-source-mapping-build.log`). Con el compilador
SHA-256 `b982e107613cfaf48db7e3e94fa81600c17a9ff66f18fc9471d8e4122b7c080f`,
la llamada al productor recompilado ya resuelve `Read`, pero se rechaza en el
control provisional de escape `NS3020`; no se ejecutó la lectura. El productor
histórico sin especialización sigue sin materializar la firma, y los controles
de tuplas numéricas y resúmenes de escape pasaron por separado
(`tuple-view-mapping-regressions.cmd` y logs `tuple-view-mapping-*.log` en
`A:/nsc-fixes-evidencia`). La primera repetición de la prueba esperaba el
diagnóstico anterior y por ello terminó con fallo; se conservó en
`binary-tuple-view-after-mapping.log`. La expectativa posterior exige el
rechazo específico de escape y no trata la lectura como aprobada.

Esta es una etapa sin commit, no una solución completa: falta acreditar el
resumen del productor en el consumidor, hacer pasar la lectura segura y
comprobar el rechazo de bibliotecas con contratos desconocidos o que retengan
vistas. El control experimental depende de productores preexistentes en A y
debe convertirse en una regresión autónoma antes de cerrar el cambio.

Una inspección posterior compara los resúmenes serializados para el mismo
patrón de tupla (`A:/nsc-fixes-evidencia/tuple-view-summary-inspect.cmd` y
`.log`). El productor seguro recompilado publica `Read` como `NoEscape` (`0`),
pero el productor histórico que ejecuta `saved = value` también publica
`Save` como `NoEscape` (`0`); el `Read` histórico publica igualmente `0`.
Por tanto, el resumen por sí solo no distingue el almacenamiento inseguro de
la lectura segura. Las fuentes históricas del `Save` se conservan en
`A:/nsc-fixes-evidencia/view-tuple-binary-escape/producer/Main.ns`; el
compilador actual rechaza esa fuente al producirla de nuevo, como se documentó
arriba. No se alteró la metadata histórica ni se ejecutó el binario inseguro.

La presencia de una especialización en el productor recompilado tampoco
demuestra por sí sola que todos los productores que la registraron hayan
calculado un resumen correcto. Antes de permitir la lectura debe establecerse
una evidencia verificable de que el contrato procede de un análisis que cubre
la vista dentro de la tupla, y debe comprobarse el rechazo ante contrato
ausente, desconocido o incorrecto. La regla conservadora del consumidor
permanece activa; no se deduce seguridad de `NoEscape` sin esa procedencia.

El control independiente `tests/integration/binary_shared_view_tuple.py`
construye un productor temporal y un consumidor desde Core binario indicado
como argumento. Con el compilador experimental y el Core aislado anterior,
rechazó las cuatro lecturas de Rc y Arc con argumento directo o local mediante
el diagnóstico conservador; el control de tupla numérica ejecutó 42 en O0/O2
y el productor que intenta guardar la vista se rechazó al compilar. Comando y
salida: `A:/nsc-fixes-evidencia/binary-shared-view-tuple-autonomous.cmd` y
`.log`. El productor se genera desde la prueba y no depende de los binarios
de las reproducciones históricas. Su resultado actual confirma la protección,
pero la lectura segura todavía debe cambiar de expectativa y ejecutarse para
cerrar el defecto; el control histórico con un resumen incorrecto sigue siendo
necesario al validar la confianza del nuevo contrato.

Se amplió la prueba de publicación con dos productores inseguros adicionales:
uno copia la tupla a un alias antes de guardarla en un campo estático y otro
guarda una tupla anidada. Ambos, igual que el almacenamiento directo, se
rechazaron con el diagnóstico esperado; los resúmenes de lectura, retorno y
alias existentes volvieron a pasar. Evidencia:
`A:/nsc-fixes-evidencia/tuple-view-summary-alias-nested.cmd` y `.log`. Estos
casos usan un `borrowed struct View` mínimo, no prueban todas las rutas de
escape ni convierten los resúmenes históricos en confiables.

El catálogo existente `nonEscapingLifecycleTypeKeys` ilustra una garantía
publicada por el productor y tratada como desconocida cuando está ausente; es
específico del ciclo de vida de clases y no certifica escape de parámetros de
tupla. El catálogo de capacidades de lenguaje se valida como conjunto de bits
conocidos y se usa al leer metadata compartida. No se reutilizó ninguna de
estas señales como prueba de la propiedad distinta que falta, ni se añadió un
número de versión o se modificaron las rutas de interoperabilidad C/C++.

La prueba autónoma con Core binario real ahora inspecciona además los
resúmenes publicados por el productor: `Read` y `ReadForward` son `NoEscape`,
`Echo` es `Return` y el control numérico es `NoEscape`. Para ambas familias,
con argumento directo o guardado en un local, las tres llamadas se rechazaron
por la protección conservadora: doce rechazos esperados. Los controles
numéricos ejecutaron 42 en O0/O2 y el almacenamiento global inseguro se rechazó
al compilar. Se usó el compilador experimental de hash
`b982e107613cfaf48db7e3e94fa81600c17a9ff66f18fc9471d8e4122b7c080f` y el
Core aislado anterior. Evidencia en
`A:/nsc-fixes-evidencia/binary-shared-view-tuple-forward.cmd` y `.log`.
La prueba comprueba la publicación y la protección, pero no prueba todavía
que los resúmenes `NoEscape` puedan acreditarse de forma segura para consumo.

El MIR del productor de lectura se conservó en
`A:/nsc-fixes-evidencia/tuple-view-safe-mir.log` mediante
`tuple-view-safe-mir.cmd`. `Reader.Read` desestructura el parámetro en locales,
llama a `SharedView.Get` sobre la vista local, lee un campo escalar y suma el
entero. Una comprobación independiente del cuerpo debe reconocer esa llamada
y su contrato, no solo prohibir escrituras directas en el cuerpo de `Read`:
de lo contrario, un método llamado podría retener la vista. La evidencia MIR
es del productor fuente inspeccionado; no está publicada como prueba verificable
en las bibliotecas históricas.

El catálogo existente de ciclos de vida se produce con un recorrido MIR que
rechaza formas desconocidas y se transporta opcionalmente en la metadata. Su
mecanismo es una referencia para diseñar una evidencia semántica propia del
parámetro, no una prueba reutilizable para este caso. Una implementación debe
atar esa evidencia a la identidad del método, su parámetro y dependencias,
conservar `Unknown` si falta o no se puede verificar, y no modificar la
interoperabilidad C/C++ ni agregar un esquema de versiones ficticio. No se ha
implementado ni certificado aún ese nuevo contrato.

La regresión autónoma crea también un módulo independiente que declara una
estructura genérica `NativeSharp.Core.SharedView` homónima. El control `Ping`
de ese productor pasa el análisis y la firma `Read` con su tupla no se
materializa como si perteneciera al Core real. La selección completa de la
prueba pasó (`A:/nsc-fixes-evidencia/binary-shared-view-tuple-counterfeit.cmd`
y `.log`). Esto comprueba esa identidad homónima concreta; no certifica todas
las colisiones posibles ni autoriza todavía llamadas binarias con vistas.

La preparación precompilada dispone de un producto opcional con fuente pública,
sintaxis y snapshot, pero ese producto no proporciona necesariamente el cuerpo
real de un método ordinario. `PublicSurfaceBuilder::emitMethodMember` usa texto
inline cuando existe; en otro caso `emitOrdinaryMethod` genera un cuerpo vacío
para un método no abstracto. Las plantillas genéricas tienen una ruta separada
para su fuente. Por tanto, volver a analizar la superficie ordinaria generada no
demuestra lo que ejecuta la biblioteca. La procedencia de una garantía de escape
debe venir del productor o de una prueba equivalente vinculada al artefacto;
el cuerpo vacío no debe tratarse como evidencia de `NoEscape`.

Tras el cambio experimental de retorno prestado se repitieron las dos
selecciones dirigidas mediante `A:/nsc-fixes-evidencia/pointers-binary-return-current.cmd`.
La selección de cuatro casos de `borrowed struct` rechazó retorno, retorno en
arreglo y reemplazo para Rc y Arc; su control con dueño vivo ejecutó 42 en O0
y O2 para ambas familias (`pointers-carrier-return-current.log`). La prueba
autónoma de tuplas binarias terminó sin fallos según sus expectativas actuales
(`pointers-binary-return-current.log`): conserva el rechazo preventivo de las
lecturas con vista, por lo que no acredita soporte de lectura binaria segura.
Se usó el compilador aislado SHA-256
`dd8ef173cd74345f8d888052033ec37b7d66dde9ced36fd2cb332d70d0b3785c` y
`pointers-sdk-refresh/NativeSharpCore.lib` preexistente, SHA-256
`f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`.
No se regeneró el SDK ni se afirma que provenga del compilador actual. Las
garantías para metadata histórica y la lectura binaria siguen pendientes.

Tras los cambios de cleanup, `pointers-binary-return-current.cmd` volvió a
pasar con el compilador aislado SHA-256
`f8d6020ee7a69d327a21c7cc339eef43306001d8a67e2f25f542269a5afe8580` y el
Core binario preexistente SHA-256
`f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`.
En Rc y Arc el análisis rechazó retorno, retorno en arreglo y reemplazo del
portador prestado; el control con dueño vivo ejecutó 42 en O0/O2
(`pointers-carrier-return-current.log`). El productor de la prueba autónoma
volvió a rechazar almacenamiento inseguro y el control numérico ejecutó 42,
pero las lecturas binarias con vista siguen rechazadas por la protección
conservadora (`pointers-binary-return-current.log`). Estas expectativas no
acreditan soporte de lectura segura ni certifican el SDK completo. La prueba
dirigida ASan de liberación de arreglos también pasó 1/1 con este compilador
(`asan-throwing-array-registered.log`), sin verificar fugas generales.

Un productor separado con `Pack` que devuelve una tupla con un clon de Rc o
WeakRc compilaba, pero el consumidor no encontraba el método; las variantes
Arc y WeakArc ejecutaban 42 (`shared-tuple-return-binary-20261111/clone-before.log`).
La reconstrucción provisional de elementos de tupla reconoce ahora también
las especializaciones exactas de Rc y WeakRc de NativeSharpCore cuando la
identidad completa consta en metadata. Con compilación incremental, las cuatro
familias ejecutaron 42 en O0 y O2 y los contadores fuertes o débiles volvieron
al valor anterior al terminar el ámbito de la tupla. La regresión registrada
`nsc.sdk.shared-tuple-return-binary` pasó 1/1 con Core binario aislado anterior
y compilador SHA-256
`65bf16c0ebba6175b3b93a7ec95e0e63c7b939902e2f4cdc5974e8ec17a33939`.

La regresión incluye productores de otro módulo que declaran tipos homónimos:
el control numérico es accesible y las cuatro firmas de tupla homónima no se
tratan como si pertenecieran al Core real. También pasaron las comprobaciones
dirigidas de prohibición de almacenamiento compartido para Rc/WeakRc, de vistas
binarias y de tuplas numéricas (`static.log`, `views.log`, `numeric.log`,
`registered.log` y `namespace.log` en
`A:/nsc-fixes-evidencia/shared-tuple-return-binary-20261111`). El control con
homónimos almacenados en un estado de namespace se detuvo previamente por la
regla de almacenamiento y no acredita identidad; se usaron métodos para llegar
a la comprobación. La variante independiente `move(input)` sigue rechazada en
el productor (`move-parameter.log`) y no queda resuelta por el caso de clon.
La ruta sigue provisional y no demuestra compatibilidad general con metadata
histórica ni seguridad completa para vistas o formas arbitrarias de alias.

La variante `return (42, move(input))` se rechazaba en las cuatro familias,
aunque el retorno directo `return move(input)` era aceptado. El analizador de
tuplas analiza ahora un `move` explícito con permiso de movimiento únicamente
cuando la propia tupla está en un contexto de consumo. Las regresiones
dirigidas `nsc.sdk.shared-tuple-move-binary`,
`nsc.sdk.shared-tuple-move-partial-binary` y `nsc.sdk.shared-tuple-move-analysis`
pasaron 3/3 sin omisión: prueban las cuatro familias en retorno binario O0/O2,
la liberación si falla otro elemento, y rechazos específicos de copia, doble
movimiento, uso posterior y tupla descartada. La prueba de vistas binarias
también pasó (`A:/nsc-fixes-evidencia/shared-tuple-move-20261111/registered.log`
y `views.log`). Compilador incremental SHA-256
`59445192e91fdacf05f67f18efe747cc1dbf9d87c949804f2dba18a65becc3a2`;
se usó Core binario anterior, sin regenerar.

Un primer ensayo más amplio permitió descartar la tupla con un propietario
movido y se observó que no se destruía en los controles Rc/Arc. Se restringió
la habilitación al contexto de consumo; la forma descartada se rechaza y no
se afirma que su destrucción esté implementada. La repetición sobrescribió el
log de la observación inicial, por lo que esta no conserva un log separado;
la regresión actual sí exige el rechazo específico. Los cambios de código y
prueba permanecen sin commit y no acreditan todas las combinaciones de tuplas.

La prueba adicional `nsc.sdk.shared-tuple-move-call` pasó 1/1 sin omisión:
para Rc, WeakRc, Arc y WeakArc pasa una tupla con el handle movido a un método
por valor, con salida normal y con excepción, en O0/O2. Los 16 procesos
devolvieron 42 al comprobar caducidad o contador después de la llamada. El
comando y resultado están en
`A:/nsc-fixes-evidencia/shared-tuple-move-20261111/call-registered.cmd` y
`call-registered.log`. Se usó el mismo compilador y Core binario anterior del
párrafo previo. El método y su llamador se compilan en el mismo proyecto;
esta prueba no demuestra ese paso de argumentos a través de una biblioteca.

El control independiente `nsc.sdk.shared-tuple-move-call-binary` trasladó el
método receptor a una biblioteca estática NativeSharp y pasó 1/1 sin omisión.
Los 16 cruces de las cuatro familias, salida normal o excepcional y O0/O2
verificaron caducidad o contador desde el consumidor separado. Productor y
consumidor se compilaron al mismo nivel en cada cruce; se utilizó el Core
binario anterior y el mismo compilador del párrafo previo. Comando y resultado:
`A:/nsc-fixes-evidencia/shared-tuple-move-20261111/call-binary-registered.cmd`
y `call-binary-registered.log`. No se prueban DLL, combinaciones de niveles
diferentes ni metadatos históricos. No se reconstruyó el Core.

## Vista extraída hacia un parámetro de salida

Una reproducción nueva analiza una función que desestructura un parámetro
`(SharedView<Payload>, int)` y copia la vista a `ref SharedView<Payload>`.
El llamador declara la salida en un ámbito exterior, crea el propietario en
uno interior y usa la salida después de terminar ese ámbito. El análisis
aceptó las variantes directas y reenviadas para Rc y Arc. Los programas
potencialmente inválidos no se ejecutaron; las fuentes, comandos y salida
están en `binary-view-escape-consumer.log` bajo
`A:/nsc-fixes-evidencia/shared-tuple-call-commit-20261111/`.

Un productor independiente con un `borrowed struct View` mínimo publicó
`NoEscape` para el parámetro de tupla tanto en la asignación directa como en
la que pasa por un método (`Save: [0, 0]`, `Forward: [0, 0]`); el parámetro
de vista del método intermedio se publicó como `CallerOwned` (`Store: [0, 2]`).
La lectura sin escape también publica `NoEscape`. El productor y sus
resúmenes están en `binary-view-summary-ref.log`. Por ello un resumen
`NoEscape` emitido por el productor actual tampoco basta para habilitar la
llamada binaria sin una prueba que cubra la transferencia a salidas.

La regresión `tests/integration/borrowed_tuple_ref_output.py` conserva cuatro
expectativas de rechazo y dos controles con dueño y salida en el mismo ámbito.
Hoy fallan los cuatro rechazos porque el análisis acepta los ejemplos; los dos
controles se aceptan. No ejecuta código inseguro. El registro está en
`borrowed-tuple-ref-output.log` de la misma carpeta. La prueba aún no está
registrada en CTest y usa Core binario anterior con otros cambios locales en
el compilador. El manejo de la procedencia de entrada a salida y su validación
en el llamador siguen sin corregir; la protección conservadora para llamadas
binarias con vistas compuestas permanece activa.
Repetición dirigida posterior, 2026-11-11: con el compilador incremental de la
copia y el Core binario aislado preexistente, `binary_shared_view_tuple.py`
terminó con `FAILURES []`. El registro de comandos y diagnósticos está en
`A:/nsc-fixes-evidencia/binary-shared-view-current-20261111.log`. Se rechazaron
las llamadas conservadoras con vistas y el almacenamiento inseguro comprobado;
los controles numéricos ejecutaron en O0/O2 y se comprobó el tipo homónimo.
Esto conserva una barrera de seguridad: las lecturas binarias válidas con
vistas siguen rechazadas y no se demuestra seguridad general de metadata
histórica. El compilador contiene otros cambios locales y el Core no se
regeneró en esta repetición.
Tras los cambios del backend decimal, `nsc.sdk.shared-tuple-return-binary`
pasó 1/1 sin omisiones con el Core binario aislado preexistente
(`A:/nsc-fixes-evidencia/pointers-after-decimal-20261111.log`). El Core no se
regeneró y el compilador contenía otros cambios locales; este control no
certifica todas las formas de vistas ni una revisión limpia.
