# Vista activa y reemplazo a través de un alias ref

## Protección parcial por identidad local

Una selección adicional examinó la salida `ref/out` con dueños homónimos.
Los dos positivos de una vista del dueño exterior fueron rechazados con
`NS3020`. Cuatro negativos también fueron rechazados; uno falla ya en una
transferencia previa y no comprueba la invalidación posterior. En los casos
con dos dueños homónimos el diagnóstico no identifica cuál motivó el rechazo,
por lo que tampoco demuestra que se haya seguido la raíz interior.
`A:/nsc-fixes-evidencia/view-output-narrow-guard-candidates-20261112.log`
conserva fuentes, comandos y diagnósticos del análisis semántico; no se
ejecutaron los programas. El compilador tuvo SHA-256
`cd8ac03cd68262e604cad9bf6319552c886ae5ce2421a14313103bcf59bb1154`.

La ruta de salida en `CallAnalyzer.cpp` vuelve a resolver el nombre del dueño
en el ámbito de llamada y en el de destino: en el positivo puede encontrar
dos locales distintos con el mismo nombre. Sin embargo, sustituir la búsqueda
por cualquier identidad exterior conocida no es seguro: otra entrada o una
rama puede aportar una vista del dueño interior. El diseño y los
contraejemplos están en
`A:/nsc-fixes-evidencia/view-output-root-binding-analysis-20261112.md`.
La completitud de productores y escrituras sigue pendiente; no se ha cambiado
el diagnóstico ni se afirma resuelta la brecha.

Se añadió una comprobación limitada al reemplazo a través de una referencia
cuyo origen y lugar canónico identifican exactamente un dueño local por valor,
move-only, en el mismo callable y ámbito. Si una vista visible conserva una
identidad `Local` con el mismo ordinal, se rechaza el reemplazo. No se usa la
ausencia de esa identidad, `Unknown` ni `*` como prueba de seguridad; esos
casos mantienen el comportamiento anterior y siguen pendientes.

Después de ajustar además el callable y el ámbito del dueño, el build
incremental terminó con salida 0. Con nsc SHA-256
`bce834afc8c296bb34c7abb89585adfb8334ad9c33217e147a9e7ac031997f7d`,
`final-exact-view_ref_owner_replacement-20261112.log` registra seis variantes
de origen conocido rechazadas correctamente y seis variantes de receptor,
reasignación, salida o rama aún aceptadas inesperadamente. Todos los controles
positivos de esa selección pasaron. Los runners `final-exact-view_shadow_identity_vistas-20261112.log`
y `final-exact-tuple_inline_view_lifetime_vistas-20261112.log` pasaron.
Cuatro pares de pruebas de historia y ramas dieron el mismo diagnóstico tanto
por alias como por asignación directa; esto compara el contrato observado, no
demuestra que los ocho casos deban rechazarse ni descarta otros falsos positivos.
Los programas negativos solo se analizaron, nunca se ejecutaron.

Esta protección es parcial: no cubre orígenes inciertos, receptores, salidas
`ref/out`, proyecciones ni todas las uniones. La prueba completa de alias
continúa fallando y la brecha no se declara resuelta.

## Estado

**Reproducido en análisis semántico; corrección pendiente.** No se conoce un
ejecutable anterior verificado con el que atribuir el hallazgo a un commit.
No se han ejecutado los programas potencialmente inseguros.

## Reproducción

El compilador acepta reemplazar mediante un local `ref Owner` el dueño de una
vista viva. La asignación directa equivalente al mismo dueño se rechaza con un
diagnóstico de préstamo activo. El resultado se observó con origen directo de
la vista y con `owner.Borrow()`, con y sin un local homónimo en un scope
interior, y con una cadena de tres représtamos.

El volcado MIR de `negative_known_replace_alias_no_shadow` registra un
`reference-local` inicializado desde `local-address owner`, seguido de
`store Owner` hacia el puntero del alias. El volcado de cuerpos de la cadena
de tres représtamos conserva `frame-local:0`, `borrow-place=L0` y la
cadena de fuentes. La evidencia está en
`A:/nsc-fixes-evidencia/vistas-shadow-riesgos-20261112/resultados-comparacion/known-alias.mir.txt`
y `resultados-reborrow/reborrow-bodies.txt` bajo la misma carpeta.

El nsc de la primera comparación tuvo SHA-256
`4aedad5ddd323493d549492da5a649ec23bacae39096fc2e922c01731780211d`;
el de la tanda ampliada tuvo
`d6f7c8e534461a4df3394ae98d09a6ba9e26ebc24a3dc7de72a53efba88b38d1`.
Cada JSON conserva hashes de las fuentes, comandos, cwd, stdout, stderr y
hashes del ejecutable antes/después. Las dos tandas usaron ejecutables
diferentes; no constituyen comparación antes/después de una corrección.

`resultados-reborrow/results.json` registra siete controles positivos
aceptados, seis reemplazos por alias/représtamo aceptados inesperadamente,
dos reemplazos directos rechazados y cuatro escapes por `ref/out` rechazados.
Los positivos incluyen ausencia de vista, vista fuera de scope, dueño homónimo
distinto, campo no relacionado y représtamo sin vista.

## Límites y siguiente paso

El volcado semántico resuelve `Borrow` sobre el local `owner`; por
inspección, la recolección de fuentes del método de instancia registra esa raíz
como `Unknown`. El volcado no imprime directamente el vector, y la aceptación
por alias también se observa con origen directo. No debe atribuirse el hallazgo
a `Unknown` ni al shadowing sin una prueba adicional.

`AssignmentAnalyzer.cpp` identifica el local LHS y consulta la protección de
dueño para ese ordinal; `referenceEscapeOrigin` y `referencePlaceKey` del
alias ofrecen una posible ruta hacia el almacenamiento real. La propuesta
estática, sus controles de représtamo y el peligro de contar el propio alias
como conflicto están en
`A:/nsc-fixes-evidencia/vistas-shadow-riesgos-20261112/DISENO.md`. Quedan por
resolver explícitamente procedencia desconocida, proyecciones, parámetros,
uniones y binarios. La protección parcial posterior descrita arriba no es una
solución general ni una prueba general de seguridad.

## Prototipo conservador y regresión

Se preparó un guard WIP para una asignación a través de referencia mutable con
origen local y ubicación canónica `L<ordinal>`. Una primera versión trata
`Unknown`, `*` o procedencia heredada sin identidad como posible conflicto
en lugar de inferir seguridad por el nombre. La compilación incremental pasó.
El compilador utilizado en
`A:/nsc-fixes-evidencia/view-ref-owner-guard-20261112.log` tuvo SHA-256
`9c005763098c2409668d215430495015f4dd71848a9622816e0ac5b9bbeaf017`.
Los negativos de alias/représtamo de la selección se rechazaron, pero el
control válido `distinct_shadow_owner` también se rechazó: la prueba dirigida
en su conjunto **no pasó**. Los otros controles de esa selección pasaron.
Ningún programa negativo se ejecutó.

`collectViewSourceFields` añade actualmente `*` incluso al registrar
argumentos `ref` con identidad local conocida de una construcción de vista.
El modelo disponible no distingue el origen y momento de ese marcador, ni
ofrece en este punto una prueba completa de que la vista no fue reasignada a
través de una llamada u otra ruta. Ignorar el marcador simplemente porque
existe una raíz conocida distinta no demuestra que no pueda haber otra raíz.
La versión WIP no cumple simultáneamente conservación de este positivo y
tratamiento conservador del origen desconocido; no debe registrarse como
corrección terminada. Parámetros y proyecciones siguen fuera del guard.

### Orígenes inciertos acotados: propuesta no implementada

Una vía por investigar es conservar, para cada productor incierto de vista,
el callable, scope y límite de ordinales de locales existentes en ese momento.
Las copias trasladarían el límite original; una asignación posterior o una
llamada `ref/out` capaz de escribir la vista agregaría un origen incierto con
su propio límite. Los orígenes heredados sin certificado conservarían un límite
desconocido, no una garantía de seguridad.

La hipótesis es que una vista anterior no puede haber obtenido por vías seguras
la dirección de un struct local por valor que aún no existía. El límite podría
ayudar a probar que la vista exterior del control válido no presta del dueño
interior posterior. Esta conclusión no debe extenderse a referencias de clase,
objetos compartidos, alias, punteros ni memoria externa: un local posterior
podría referirse a un objeto ya existente.

La hipótesis exige demostrar que no se pierde ningún productor en una
reasignación, llamada o rama; que loops y reutilización de slots no vuelven a
poner viva una referencia inválida; y que `unsafe`, aliases externos y
captures no se interpretan como prueba de disyunción. Las uniones deben ser
conservadoras. En análisis por callable, el límite y sus identidades requieren
remapeo con `localBase` y verificación de la identidad del callable; la
serialización y la identidad de cualquier caché que consuma esos datos también
deben investigarse. La alternativa afectaría al modelo de locales y a varias
rutas de producción de vistas, no solamente a `AssignmentAnalyzer.cpp`.

No se ha implementado ni probado este diseño, ni se afirma que el límite de
ordinales baste para todos los controles. La política estricta falló
`distinct_shadow_owner`; su diff se conservó en evidencia y se retiró del
producto. No se debe ignorar `*` para hacer pasar ese control.

### Infraestructura informativa en preparación

La primera tanda conserva por local de vista observaciones de origen
desconocido: scope de producción y cantidad de locales observada. **No son
límites de vida ni pruebas de que dos almacenamientos sean disjuntos**. La
incertidumbre no localizada usa ambos campos vacíos. Las observaciones de una
copia o asignación se unen conservadoramente, y una salida `ref/out` añade una
observación de llamada; si el destino es una referencia cuyo local no se
conoce, marca las vistas visibles del callable. El análisis por ramas conserva
la unión existente, no demuestra un estado exacto por camino.
Un vector sin observaciones significa que no se registró evidencia: no
significa que el valor carezca de raíces desconocidas ni autoriza disyunción.

Cada local mantiene como máximo ocho observaciones distintas. Ocho es una
decisión de presupuesto para limitar memoria y búsquedas, no un valor medido ni
un límite semántico del lenguaje. Si llega una novena observación o cualquier
origen no localizado, toda la historia se sustituye por un único Unknown no
localizado que subsume las observaciones, y no puede volver a estrecharse por
una copia o una unión. La búsqueda recorre como máximo ocho entradas. El vector
vacío ocupa espacio en cada `MethodLocalSymbol` (su tamaño depende de la
implementación de la STL; no se ha medido el impacto total). Si una llamada
tiene varios destinos inciertos se recorre el conjunto de locales visibles una
vez, no una vez por argumento.

El fusionador remapea scope y conteo observado con las bases del shard; el
Unknown no localizado queda vacío al remapear. La compilación incremental
conjunta pasó. La prueba dirigida de modelo pasó sus 25 comprobaciones de
copias, asignación, salidas `ref/out`, referencia alias, dos callables,
deduplicación y colapso. La ejecución directa desde la raíz del repositorio
quedó en `A:/nsc-fixes-evidencia/view-unknown-model-repo-cwd-20261112.log`;
el ejecutable de prueba tuvo SHA-256
`3db5c6feb1a52b4be35b6f59d9baea309696a53b4a35db0b4c7c3bdc399a911b`.
La prueba CTest también pasó y su resumen está en
`A:/nsc-fixes-evidencia/view-unknown-origin-model-20261112.log`.

Con el compilador de SHA-256
`70569bd4b18d2b563cfdecc907f525bb251b670e95b896aef82b9fbb2166ff78`,
la prueba de identidad con shadowing conservó sus controles: cuatro programas
válidos ejecutaron con resultado 42 en O0 y O2, y tres escapes se rechazaron.
El registro es `A:/nsc-fixes-evidencia/view-shadow-origin-20261112.log`.
La selección semántica ampliada aceptó los nueve controles válidos y rechazó
los seis controles directos o de escape; aceptó inesperadamente los siete
reemplazos por alias que deberían rechazarse. Su registro completo es
`A:/nsc-fixes-evidencia/view-ref-owner-observation-20261112.log`. Esos siete
programas no se ejecutaron. La observación confirma que esta tanda informativa
no corrige la brecha, sin atribuirla a los commits de vistas.

No se ha
demostrado persistencia entre procesos de este modelo, consumo por MIR ni una
clave de caché para estas observaciones; tampoco se ha activado ningún guard o
consulta que las use para cambiar diagnósticos o codegen. Las salidas que no
se puedan vincular a locales y los efectos interprocedurales no constituyen un
contrato completo de procedencia.

Esta infraestructura no demuestra la propuesta de disyunción por ordinal, no
resuelve el reemplazo por alias y no convierte el marcador `*` en una prueba
de seguridad.

### Evaluación del consumidor tras `71dc280a0`

**Diseño pendiente, no activado.** El contador observado indica cuántos
locales había al recorrer un productor, no que el conjunto de productores sea
exhaustivo ni que ese productor se ejecute una sola vez. En particular, un
backedge puede ejecutar nuevamente un reemplazo después de que otra sentencia
del mismo bucle haya reasignado la vista. La revisión de sentencias en orden
léxico no demuestra el estado de esa segunda iteración. Los hechos informativos
de `collectViewSourceFields`, `recordViewAssignmentRoot` y
`recordViewOutputOrigins` tampoco certifican por sí mismos todas las escrituras
por proyecciones, alias indirectos y efectos interprocedurales. Por tanto, un
consumidor no puede usar la ausencia de un ordinal ni un conteo anterior como
prueba de ausencia de alias.

Una protección acotada para el reemplazo necesita: resolver la referencia a
una identidad local de almacenamiento exacta, determinar conservadoramente las
vistas activas en ese punto, y comprobar todas las procedencias que puedan
alcanzarlo. El análisis por camino debe unir ramas y alcanzar un punto fijo en
bucles; asignaciones, copias y salidas `ref/out` deben actualizar el estado
antes de consultarlo. Un origen desconocido, una escritura no modelada o una
identidad de almacenamiento no demostrada no pueden descartarse por el nombre,
el shadowing o el conteo. Para conservar `distinct_shadow_owner` es necesaria
una prueba independiente de que la vista exterior no puede apuntar al struct
local interior; las observaciones actuales todavía no acreditan sus
precondiciones. Parámetros, proyecciones, memoria compartida y ciclos requieren
tratamiento explícito antes de extender la protección a esos lugares.

Se añadieron al fixture semántico controles para shadowing en rama y bucle,
reemplazo dentro de un bucle, reasignación, salidas `ref/out` y asignación
condicional. En `view-ref-owner-expanded-isolated-20261112.log`, el análisis
semántico acepta los dos controles con dueños homónimos distintos, y acepta
inesperadamente los cinco casos de reemplazo por alias. Los cinco controles
equivalentes que sustituyen la misma operación por `owner = default(Owner)`
se rechazan con `cannot overwrite owner ... while a borrowed local is alive`;
están en `view-ref-owner-expanded-direct-controls-20261112.log`. Las rutas
completas de ambos registros comienzan en `A:/nsc-fixes-evidencia/`. En los
casos `ref/out` la vista de entrada termina su scope antes del reemplazo, por
lo que la vista de salida queda como el préstamo relevante. No se observaron
errores de sintaxis o tipado en los siete casos añadidos; el resultado se
limita al análisis semántico, no certifica ejecución ni seguridad general.
No se ejecutaron programas negativos. El compilador fue el de SHA-256
`70569bd4b18d2b563cfdecc907f525bb251b670e95b896aef82b9fbb2166ff78`,
invocado con `--check-project-method-bodies`, desde la raíz de la copia y
con temporales en A. La información actual no justifica activar un guard ni
agregar reglas de diagnóstico.

### Propuesta parcial de coincidencia exacta

Se preparó para revisión, sin aplicar ni compilar,
`A:/nsc-fixes-evidencia/view-ref-exact-local-proposal-20261112.patch`;
`git apply --check` comprobó únicamente que encaja sobre la copia actual. La
propuesta compara la identidad `Local` observada de una vista en scope con el
ordinal del almacenamiento de un local por valor move-only, cuando el lado
izquierdo es una referencia resuelta exactamente a `L<ordinal>`. Solo una
coincidencia provoca rechazo: los marcadores `*`, `Unknown` y su ausencia no
se usan como prueba de que el reemplazo sea seguro. Se conservan los casos
inciertos como brecha pendiente; esto no es una solución general de alias.

La colección es acumulativa y registra posibles fuentes, no necesariamente
el valor concreto en cada camino. Antes de aplicar deben revisarse posibles
falsos rechazos por reasignaciones y ciclos, referencias de clase, identidad de
los représtamos y propagación por `ref/out`. La selección dirigida incluye
controles válidos con shadowing, ausencia de vista y vida terminada, junto con
negativos directos, reasignados, ramificados, en bucle y con salida `ref/out`.
El diff no se ha construido ni se han ejecutado las pruebas contra él; no se
afirma que cubra receptores o salidas heredadas con identidad desconocida.

### Evaluación del prototipo aplicado

Posteriormente se aplicó el diff en la copia de integración y pasó una
compilación incremental conjunta. Con el compilador SHA-256
`99e04902488bce2b8b213714aa5e2a1128805a68fe11a6d84b1df8b4d1c8c1f4`,
`A:/nsc-fixes-evidencia/view-ref-exact-local-20261112.log` registra el rechazo
esperado de los seis reemplazos por alias de raíz directa, représtamo, último
uso léxico, rama o bucle; los controles positivos de esa selección pasaron.
Seis casos de receptor, reasignación, `ref/out` y asignación en rama siguen
aceptados inesperadamente. No se ejecutaron esos negativos. Este resultado es
parcial: la selección completa no pasa y no demuestra seguridad general.

**Riesgo por verificar:** `recordViewAssignmentRoot` incorpora identidades a
la colección existente del local, y el prototipo consulta esa colección sin
representar el valor actual, caminos excluyentes ni iteraciones. Una identidad
histórica puede, por tanto, activar el rechazo incluso después de reasignar
una vista o cuando el productor y el reemplazo ocurren en ramas distintas. La
inspección establece el mecanismo, no demuestra que el contrato aplicable
exija aceptar esos programas. Los controles candidatos pareados se prepararon en
`A:/nsc-fixes-evidencia/view-ref-historical-branches-20261112.py`: reset,
ramas excluyentes, copias reseteadas y reset en bucle. El análisis semántico
de los ocho ejemplos, con el compilador del hash indicado arriba, rechazó cada
variante por alias y su variante de dueño directo con el mismo mensaje
`cannot overwrite owner 'owner' while a borrowed local is alive`; en cada caso
el subrayado señala la operación de reemplazo correspondiente. El registro
completo, con comandos y fuentes, está en
`A:/nsc-fixes-evidencia/view-ref-historical-branches-20261112.log`. El runner
marcó los ocho como fallidos porque los controles se formularon como posibles
aceptaciones; no hubo un diagnóstico de sintaxis o tipos que explique esos
resultados. No se ejecutaron los programas.

Los pares son comparables para el mensaje de préstamo: la versión directa
omite la declaración del alias y escribe en `owner`, mientras la otra escribe
por `original`. No demuestran que ambos análisis empleen la misma ruta interna
ni que el lenguaje deba aceptar o rechazar los cuatro programas; tampoco
establecen que ningún otro caso válido quede rechazado. Hasta resolver esas
preguntas es razonable conservar el prototipo solo como protección parcial
en evaluación, sujeto a pruebas dirigidas adicionales. La aceptación previa
de los controles existentes no demuestra ausencia general de regresiones.

### Propuesta de propagación para revisión

**No aplicada ni compilada.** El diff
`A:/nsc-fixes-evidencia/view-receiver-output-proposal-20261112.patch` añade una
posible identidad Local al receptor de método únicamente cuando su binding
resuelto coincide con un local por valor, no estático, move-only y del mismo
callable. Conserva `Unknown` y no infiere un ordinal desde un nombre. Para
salidas `ref/out` y asignaciones por referencia une identidades locales ya
registradas solo a destinos locales exactos; los destinos desconocidos siguen
recibiendo únicamente observaciones desconocidas. También conserva las
identidades en copias cuando no existe raíz textual. El fusionador vigente
remapea ordinales Local con `localBase`.

Una identidad es una posible raíz bajo el contrato conservador de préstamo del
receptor, no demuestra que el método guarde esa raíz. Las uniones históricas y
de entradas pueden rechazar conservadoramente casos válidos; callable,
ordinales, ramas, bucles y fuentes indirectas necesitan validación. La
propuesta no trata Unknown como evidencia de disyunción y no garantiza cubrir
todas las rutas. `git apply --check` comprobó únicamente que el diff encaja.
Los controles candidatos sin ejecutar están en
`A:/nsc-fixes-evidencia/view-receiver-output-identity-20261112.py`; deben
validarse fuentes y diagnósticos antes de interpretarlos como regresiones.

### Comparación temporal del rechazo con shadowing

**Estado de la búsqueda inicial:** no se encontró el ejecutable anterior. En el resultado posterior a la propagación,
`view-receiver-output-after-20261112.log` registra que
`ref_output_shadowed_different_owner` falla con `NS3020` en la llamada
`Sink.Save`: la búsqueda textual encuentra el dueño interior homónimo. Los
resultados anteriores de alias y controles shadow están conservados en
`view-ref-exact-local-20261112.log` y `view-shadow-origin-20261112.log`, pero
no contienen este mismo ejemplo; no prueban si se aceptaba entonces.

No se encontró entre los ejecutables aislados examinados una copia con los
hashes previos `70569bd4b18d2b563cfdecc907f525bb251b670e95b896aef82b9fbb2166ff78`
o `99e04902488bce2b8b213714aa5e2a1128805a68fe11a6d84b1df8b4d1c8c1f4`.
El comprobador por nombre existe en `ca1013921`, pero su presencia estática
no determina por sí sola el resultado anterior, pues la propagación también
cambió. Un compilador más antiguo de otra copia de integración no tiene
procedencia suficiente para esta comparación y no se ejecutó. No se atribuye
el rechazo a un cambio concreto con esa búsqueda. La comparación controlada
posterior se registra en la sección siguiente.

### Comparación controlada de la propagación

Se retiró temporalmente solo el diff de propagación y se hizo una compilación
incremental; después se restauró el mismo diff y se recompiló. El compilador
base tuvo SHA-256
`5241a0c4dfe037e6bd4c46da009c6bbae8c385ef64b0fe9d82a98ed33a4bc107`
y el restaurado
`d592b472d459ebb511476dc7c0eb7d181141960a57daa86a70aef91e2e539597`.
Los seis pares de registros `view-{full,receiver,output}-{baseline,compare-after}-20261112.log`
están en `A:/nsc-fixes-evidencia/`; cada par contiene las mismas fuentes por
caso. Esta base comparativa ya incluye el guard parcial anterior; no equivale
al baseline original de la campaña.

En la selección principal, los 29 casos pasan con el diff restaurado. Las seis
variantes de receptor, reasignación, `ref/out` y rama pasaron de aceptación
inesperada a rechazo específico por préstamo activo; los otros controles
mantuvieron su resultado. La selección ampliada de receptor pasa 18 de 19: el
positivo `ref_output_shadowed_different_owner` recibe el mismo `NS3020` en la
misma columna en ambos compiladores. Cuatro positivos adicionales de shadowing
en la selección de salida también reciben `NS3020` idéntico en ambas versiones.
Por tanto ese comportamiento es anterior a **este diff aislado**, no se
atribuye a toda la historia de la rama. Es una brecha pendiente de precisión.

Los cinco negativos de la selección de salida recibieron `NS3020` en ambas
versiones. El candidato `known_and_unknown_same_root_safe_scope` es inválido:
ambas versiones rechazan la palabra reservada `borrowed` como nombre de
variable; no cuenta como control válido ni evidencia de falso rechazo. Las
otras expectativas de los candidatos no prueban corrección general de
procedencia. Los programas negativos no se ejecutaron.

Con el diff restaurado también pasaron la prueba de modelo de ordinales de 25
comprobaciones desde la raíz del repositorio, la selección de tuplas, los
controles originales de shadowing y las fábricas async dirigidas. La evidencia
está respectivamente en `view-receiver-ordinal-after-20261112.log`,
`view-tuple-receiver-after-20261112.log`, `view-shadow-receiver-after-20261112.log`
y `await-factory-receiver-after-20261112.log`, bajo la misma carpeta. Los
ejemplos positivos correspondientes ejecutaron en O0/O2 donde indican los logs.
La comprobación por nombre sigue sin demostrar completitud de productores;
el inventario está en `A:/nsc-fixes-evidencia/auditoria-raices-vista-20261112.md`.

### Identidad de nodos y bindings en el modelo

La prueba dirigida existente de fusión de callables se amplió con un método
que declara dos dueños homónimos, transfiere una vista por `ref/out` y usa la
vista en ambas ramas de un condicional. Recorre iterativamente el AST del
callable mediante `first`, `second`, `third` y `children`, sin visitar dos
veces el mismo nodo, y consulta `nameBinding(node, callable)` sobre la misma
compilación. Comprueba que los bindings locales apuntan a un local del mismo
callable, que los dueños homónimos tienen dos ordinales y que los dos
argumentos de salida corresponden al local esperado. Los nodos sin binding se
tratan como desconocidos y no se consideran evidencia de seguridad.

La compilación incremental del target pasó y su ejecución dirigida desde la
raíz de la copia registró `PASS: 30 checks` en
`A:/nsc-fixes-evidencia/view-binding-identity-model-20261112.log`. El
ejecutable de prueba tuvo SHA-256
`f26d1d94747f04f68c2d3773857d277bd29896afeb4cd2f70772419db67996ba`;
el archivo de prueba tuvo SHA-256
`f9dd913b47528804461ec738714e4162d359f4b933fa1a0883b86d799b6e60c8`.
La identidad de puntero solo se compara dentro de la misma compilación. Esta
selección no demuestra cobertura completa de sintaxis generada, efectos
indirectos, ciclos, caché ni identidad entre procesos; tampoco certifica la
completitud de procedencia requerida para corregir el rechazo de salida con
shadowing.
