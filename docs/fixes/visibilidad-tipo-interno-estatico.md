# Acceso estático a una clase interna de otro proyecto

En la revisión `d2431a04b`, un consumidor de fuentes podía ejecutar
`Hidden.Plain()` o `Hidden.Generic<Payload>()` cuando `Hidden` era una clase
`internal` de otro proyecto sin contrato Friend. El uso mediante una
declaración local del mismo tipo se rechazaba, al igual que un método
`internal` de un tipo público. Sin `using Sample`, el acceso estático también
se aceptaba. La reproducción y sus comandos están en
`A:/nsc-fixes-evidencia/internal-visibility-contrast/result.json`; el caso
genérico se compiló y ejecutó con resultado 42 en
`internal-owner-visibility-probe/result.json`.

La búsqueda principal de tipos en expresiones respeta la visibilidad. Cuando
no registra candidato, `resolveWrittenMemberType` usa una búsqueda alternativa
por nombre que no comprobaba la visibilidad del tipo encontrado. Para esa vía,
la corrección comprueba la visibilidad existente de una clase `internal`
respecto al proyecto de la función que la usa, incluido el contrato Friend.
Si no es visible, emite un diagnóstico y rechaza el receptor. No cambia la
búsqueda alternativa compartida ni la selección de nombres públicos sin
importar, y no altera metadata o interoperabilidad C/C++.

La regresión dirigida `nsc.visibility.internal-static-type` comprueba el
rechazo con método ordinario, genérico y sin import, y conserva los controles
de método interno y declaración local. Comprueba que la API pública del
productor puede usar sus propios helpers internos, incluso desde una plantilla,
con ejecución 42 en O0/O2, y que un consumidor declarado Friend puede usar
el tipo. Pasó directamente y por CTest
(`internal-static-type-visibility.log` e `internal-static-visibility-ctest.log`
en `A:/nsc-fixes-evidencia`).

El compilador se reconstruyó incrementalmente en A. La regeneración de CMake
y la identidad de versión implicaron 37 acciones
(`internal-static-visibility-build.log`). El análisis semántico de Core y del
ejemplo original de punteros pasó; la prueba dirigida de uso de `Rc`/`Arc`
ejecutó 42 en O0/O2. El análisis `clang-tidy --checks=clang-analyzer-*` del
archivo modificado terminó con código 0 y cinco advertencias de padding en
encabezados (`internal-static-visibility-analysis.log`).

El cambio se limita al receptor obtenido por esa vía alternativa y a clases
`internal`. No demuestra protección de todos los tipos, rutas de nombres o
escenarios de metadata precompilada. La regresión
`generic_internal_owner_metadata.py` sigue fallando antes de consumir el
binario porque falta el propietario interno en la metadata; no se ha
modificado el productor de metadata compartido.

La reproducción se repitió con el compilador incremental posterior al cambio
de destrucción de campos. Continúa fallando al emitir el productor: la
verificación de lectura no encuentra el propietario materializable
`Sample.Hidden` del método genérico. Comando y salida:
`A:/nsc-fixes-evidencia/generic-internal-owner-refresh.cmd` y su archivo `.log`.
No se alcanzó el consumidor ni se modificó la ruta compartida de metadata.

Un ajuste posterior, aún sin commit, incorpora a la metadata los tipos que
contienen métodos genéricos incluidos en la clausura de plantillas. Conserva su
accesibilidad declarada, incluidos tipos anidados. La reproducción aislada ya
genera productor y metadata; el consumidor ejecuta 42 en O0/O2 y el análisis
rechaza el acceso directo a ambos helpers internos
(`A:/nsc-fixes-evidencia/generic-owner-nested.log`). La prueba dirigida previa
de visibilidad también pasó (`generic-owner-visibility-check.log`).

El cambio está en `src/Interop/LibraryMetadataConstruction.cpp`, un archivo
compartido con infraestructura de interoperabilidad: la modificación se limita
a la clausura de tipos de plantillas NativeSharp y no cambia las rutas C/C++.
Debe revisarse ese archivo durante la integración; no se han probado las rutas
de interoperabilidad, por estar fuera del alcance solicitado.

Al intentar emitir Core con este ajuste y una caché aislada nueva, el error de
propietario ausente desapareció, pero la validación del código genérico generado
falló al analizar conversiones a `AtomicSharedCounts*` y `LocalSharedCounts*`
(`generic-owner-core-build.log`). Esa primera ejecución procesó 71 archivos de
Core; la caché se conserva en `generic-owner-sdk-cache` para usos incrementales.
No se generó un SDK binario utilizable y no se considera resuelto su consumo.

Una reproducción mínima adicional usa un struct interno `Counter` en el cuerpo
de un método genérico e intenta convertir la dirección de un arreglo a
`Counter*`. La emisión falla con el mismo diagnóstico sintáctico que Core
(`generic-owner-pointer-check.cmd` y `generic-owner-pointer-reverted.log`).
El recolector actual excluye las referencias a tipos nominales del propio
módulo de `externalTypeReferences`; el parser que valida las plantillas solo
recibe los tipos publicados en metadata y las referencias externas.

Un experimento temporal registró también esa dependencia local y publicó el
tipo en metadata: la biblioteca se generó, pero el consumidor rechazó el tipo
interno ausente de su superficie materializada (`generic-owner-pointer-after.log`).
El experimento fue retirado y se recompiló incrementalmente. La reproducción
simple sin el puntero vuelve a pasar en O0/O2 (`generic-owner-after-revert.log`).
Por tanto, la solución debe conservar y materializar las dependencias locales
de las plantillas manteniendo sus reglas de accesibilidad; no basta eliminar el
error sintáctico ni publicar indiscriminadamente todos los tipos internos.

Un segundo prototipo conserva referencias nominales locales en el recolector y
materializa los tipos internos mencionados en el código de plantillas. Con un
fixture que solo accede a un campo, el productor y consumidor ejecutaron 42 en
O0/O2 y el acceso directo al tipo interno se rechazó
(`generic-owner-materialize-test.log`). Es un experimento sin commit: el
reconocimiento de nombres en el materializador todavía requiere endurecimiento.
Al añadir una llamada a un método ordinario del struct interno, el consumidor
rechazó su símbolo por no estar publicado (`generic-owner-internal-method.log`).

Core llegó a emitir `.lib` y metadata (`generic-owner-materialize-core.log`),
pero el análisis de la reproducción original de los cuatro punteros contra
ese binario falla por miembros internos que no aparecen en la superficie
reconstruida (`generic-owner-binary-consumer.log`). No se alcanzó la ejecución
de operaciones. Aunque se indicó una caché aislada, esa emisión procesó los 71
archivos de Core; hay que aclarar la invalidación antes de repetirla. La emisión
también creó automáticamente un encabezado de host en el directorio de evidencia
en A; no se usó ni modificó un build de interoperabilidad C/C++.

Un ensayo posterior también reconstruye como precompilado el propietario de
una plantilla y admite en bibliotecas estáticas métodos internos registrados
en metadata y presentes físicamente cuando su tipo aparece como propietario o
referencia de una plantilla. La reproducción del struct interno con método ya
enlaza y ejecuta 42 en O0/O2; se rechazan los accesos directos a los tipos
internos (`generic-owner-narrow-test.log`). La prueba de visibilidad anterior
también pasó (`generic-owner-precompiled-visibility.log`). El ensayo anterior
sin reconstruir el propietario alcanzaba el enlace pero duplicaba una entrada
de destrucción (`generic-owner-static-symbol-test.log`).

Este código sigue provisional y sin commit: la detección de referencias en
texto requiere una representación semántica más precisa, no se ha validado
el caso con DLL y el consumidor de Core existente todavía rechaza el receptor
estático `SharedAtomic` no registrado como dependencia
(`generic-owner-precompiled-core-consumer.log`). No se volvió a emitir Core en
esta continuación porque la caché anterior no evitó procesar todos sus archivos.

La variante mínima `NSC_METADATA_STATIC_CASE=1` reproduce la llamada a una
clase estática interna desde el cuerpo de la plantilla: se genera la biblioteca
pero el consumidor rechaza `Helper` como identificador desconocido
(`generic-owner-static-check.cmd` y `generic-owner-static-before.log`). Un
control distinto declara una variable local llamada `Helper` cuando también
existe un tipo interno con ese nombre; la llamada debe resolver al valor local.
Ese control ejecutó 42 en O0/O2 y el acceso directo al tipo interno fue rechazado
(`generic-owner-shadow-check.cmd` y `generic-owner-shadow.log`). Esto impide
tratar todo receptor de igual nombre como tipo estático por mera coincidencia de
texto. El camino de puntero con método también volvió a pasar
(`generic-owner-pointer-current.log`).

Se probó registrar candidatos por nombre corto sin reescribir la expresión.
Pasaron el ejemplo estático simple y el control con variable sombreada, pero
una reproducción con dos `ImportedHelper` en distintos namespaces y un
`using` que selecciona uno falló al consumir la biblioteca: la plantilla
reconstruida perdió esa selección (`generic-owner-static-collision.log`). La
búsqueda por nombre corto se retiró y se recompiló incrementalmente; el control
de puntero con método volvió a pasar (`generic-owner-pointer-after-static-revert.log`).
La reproducción con colisión se conserva. Se necesita vincular el receptor al
tipo elegido en el ámbito original, además de respetar los valores que lo
sombreen; conservar ambos candidatos por sí solo no implementa ese contrato.

Se agregó un control del mismo ejemplo de colisión como proyecto íntegro desde
fuentes: eligió el tipo importado y ejecutó 42 en O0/O2. La biblioteca se generó,
pero su consumidor volvió a rechazar el identificador en O0, antes de llegar a
O2 (`generic-owner-collision-source-control.log`). Esto separa la resolución
original correcta de la pérdida de contexto durante la reconstrucción binaria.
El recolector guarda imports y alias en `TemplateState` para especializaciones
locales; `GenericTemplateRecord` y la reconstrucción de metadata no transportan
ese ámbito, porque el diseño actual espera referencias autocontenidas. No se ha
decidido cambiar el formato de metadata ni se ha implementado esa conservación.

Un control adicional puso el `using Sample.Support` dentro del namespace de
declaración: desde fuentes ejecuta 42 en O0/O2 y la biblioteca se genera, pero
el consumidor binario sigue fallando en O0
(`generic-owner-namespace-import.log`). Una prueba distinta intentó situar el
`using` dentro del cuerpo del método y fue rechazada por el parser, que espera
la forma de gestión de recursos (`generic-owner-body-import.log`). Por tanto,
insertar indiscriminadamente imports en el cuerpo no conserva la gramática.
Estos controles no acreditan una solución ni prueban todas las interacciones
entre namespaces, alias y declaraciones parciales.

Preparación provisional: el registro interno `GenericTemplateRecord` conserva
ahora los imports y alias del ámbito de declaración, con los alias ordenados
para publicación determinista. La prueba dirigida de generics aprobó 85
comprobaciones, incluidas las nuevas para un import y un alias
(`generic-declaration-scope-test.log`). Es información en memoria: todavía no
se transporta en metadata ni cambia la reconstrucción, por lo que la colisión
binaria sigue abierta. Al afectar un encabezado compartido, la compilación
incremental de los dos objetivos seleccionados programó 159 acciones
(`generic-declaration-scope-build.log`); no se ejecutó una batería completa.
Entre las acciones se recompiló `CHeaderGenerator.cpp` como dependencia del
objetivo del compilador en el build aislado de A. No se modificó esa fuente ni
se ejecutaron pruebas de interoperabilidad; la acción debe considerarse al
evaluar el alcance de esta compilación.

La prueba dirigida se amplió con dos namespaces cuyas plantillas usan el mismo
alias para tipos diferentes. Ambos registros conservaron su propio destino y
pasaron las 88 comprobaciones (`generic-scope-isolation-test.log`). Esto prueba
el registro en memoria, no su transporte por metadata ni la reconstrucción.
Para esa repetición se recompiló únicamente el ejecutable de prueba afectado.

La decisión adoptada para esta tarea es conservar la identidad seleccionada en
el ámbito original de la plantilla y reconstruir referencias autocontenidas,
sin introducir una versión nueva del formato NSMETA. La autorización general
del usuario permite adoptar la opción recomendada sin detener la campaña. La
consecuencia es que el recolector debe distinguir una expresión que realmente
designa un tipo de otra que designa un valor con el mismo nombre: una búsqueda
por texto no es suficiente. Esta decisión no acredita aún una implementación
correcta ni excluye revisarla si aparece evidencia contraria.

Se añadió una reproducción con `using Pick = Sample.Support` y la llamada
`Pick.ImportedHelper.Value()`, manteniendo otro `ImportedHelper` en `Other`.
Desde fuentes devuelve 42 en O0 y O2; el productor binario se genera, pero el
consumidor falla en O0 con `unknown identifier 'Pick'`. Los comandos y la salida
están en `A:/nsc-fixes-evidencia/generic-owner-alias-static.cmd` y
`generic-owner-alias-static-before.log`. El ejemplo evidencia pérdida del alias;
no prueba una solución ni permite ignorar el caso de valores que sombrean tipos.

Un control adicional declara un campo estático privado `Helper` en el
propietario de la plantilla, junto a un tipo `Helper` en el namespace. Desde
fuentes, `Helper.Read()` elige el campo y devuelve 42 en O0 y O2; la biblioteca
se genera, pero el consumidor binario rechaza `Helper` en O0. Evidencia:
`A:/nsc-fixes-evidencia/generic-owner-field-shadow.cmd` y
`generic-owner-field-shadow.log`. Este caso muestra que conservar solo el
ámbito de imports tampoco basta: la reconstrucción debe preservar los miembros
necesarios del propietario y sus reglas de acceso. El filtro
`collectMetadataMemberOrdinals` solo selecciona miembros públicos o protegidos,
por lo que el campo privado de este control queda fuera de la publicación. No
se ha comprobado todavía si corregir esa selección bastaría: también deben
validarse la materialización, el enlace y la inaccesibilidad para consumidores.

Continuación experimental sobre el respaldo `57c47c297`: se incluyeron campos
estáticos de propietarios de métodos genéricos y sus tipos al construir la
metadata, se reconstruyeron con su accesibilidad original y se admitieron sus
símbolos presentes en una biblioteca estática. También se consideran métodos
públicos del tipo de esos campos como dependencias para validación del enlace.
El consumidor del control de `Helper` ejecutó 42 en O0 y O2; otro propietario
público leyó su campo privado desde la plantilla y el consumidor directo fue
rechazado. Fuente, comando y salida: `tests/integration/generic_internal_owner_metadata.py`,
`A:/nsc-fixes-evidencia/generic-owner-field-shadow.cmd` y
`generic-owner-field-shadow.log`. El compilador incremental reportó hash
`ddfb806dd332926e`; las pruebas dirigidas de puntero, sombreado local y
visibilidad previa pasaron en la misma construcción.

Es un prototipo: publica todos los campos estáticos de esos propietarios, no
solo los usados, y la selección provisional de símbolos aún utiliza referencias
por texto en otros caminos. Falta validar DLL, campos con tipos externos y
propiedades, y comprobar que no se amplíe indebidamente la superficie. El caso
de alias y el SDK Core binario siguen sin resolver; este resultado no acredita
un arreglo general ni un SDK listo para usar.

Un control nuevo coloca dos declaraciones genéricas del mismo módulo en
archivos distintos: ambas usan `Pick.Helper.Read()`, pero cada archivo asigna
`Pick` a un namespace diferente y espera 21 o 33, respectivamente. El proyecto
completo desde fuentes devuelve 42 en O0/O2; la biblioteca se genera, y su
consumidor falla en O0 con `unknown identifier 'Pick'` en ambas plantillas.
Fuente y evidencia: `tests/integration/generic_library_alias_isolation.py`,
`A:/nsc-fixes-evidencia/generic-owner-alias-isolation.cmd` y su `.log`.
El resultado descarta resolver el problema con un único alias global para todo
el módulo: cada declaración debe conservar su propio ámbito. La prueba queda
pendiente de pasar como consumidor binario.

Se repitió ese control desde fuentes con los dos alias dentro de dos bloques
separados que reabren el mismo namespace en un solo archivo. También devolvió
42 en O0/O2; el consumidor binario siguió fallando por pérdida de ambos alias
(`A:/nsc-fixes-evidencia/generic-owner-alias-namespace-blocks.cmd` y su `.log`).
Esto demuestra que bloques de namespace separados pueden conservar ambos
ámbitos en esa forma concreta. No resuelve declaraciones en el namespace
global: el parser exige un nombre después de `namespace`, y aún debe verificarse
su reconstrucción sin mezclar los imports de plantillas diferentes.

El recolector conserva ahora en memoria las directivas escritas exactamente
por el usuario, con el namespace léxico de cada una, por separado de las
variantes sintéticas que usa la búsqueda de nombres. La prueba dirigida
`nsc.stage44.generics-monomorphization` pasó 92 comprobaciones, incluidas
directivas de archivo, namespaces hermanos y anidados
(`A:/nsc-fixes-evidencia/generic-original-directives-test.log`). Pasó además el
control existente de caché de imports y las reproducciones dirigidas de
puntero y campo privado. El control de caché verifica el comportamiento previo,
no la serialización del nuevo campo; aún no se transporta por NSMETA ni se ha
probado su persistencia en todos los productos de caché. En consecuencia, los
consumidores binarios con alias siguen fallando.

Por cambiar un encabezado compartido, la compilación incremental del compilador
y la prueba seleccionada programó 159 acciones. Entre las dependencias se
recompiló `CHeaderGenerator.cpp` en el build aislado de A; no se modificó esa
fuente ni se ejecutaron pruebas de interoperabilidad C/C++.

Con el compilador `ddfb806dd332926e` se volvió a analizar el consumidor de
Core previamente emitido, sin reconstruir Core. Continúa el error de receptor
`SharedAtomic` desconocido en la plantilla reconstruida
(`A:/nsc-fixes-evidencia/generic-owner-core-consumer-refresh.cmd` y su `.log`).
La comprobación usa el binario anterior, por lo que no demuestra cómo se
emitiría Core con los últimos cambios del productor.
