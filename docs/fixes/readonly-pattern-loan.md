# Referencia de patrón a un propietario move-only

## Reproducción y causa

Un `match (owner.Get())` con un enlace `ref item` rechaza una referencia de
solo lectura: el parser sintetizaba para el sujeto un local de referencia
mutable incluso cuando ninguno de los enlaces era `ref mut`. Un cambio del
parser por sí solo permitió la lectura, pero también aceptó reemplazar el
propietario mientras el enlace seguía activo. El ensayo inseguro no se conservó.

## Cambio

El sujeto de un match de sentencia se convierte en referencia de solo lectura
cuando todos sus enlaces por referencia lo son; si algún enlace es `ref mut`,
conserva la referencia mutable anterior. Para el sujeto sintético cuya llamada
devuelve una referencia de instancia ligada a `this` y cuyo receptor es un
valor move-only o una vista, el análisis registra el préstamo con la procedencia
existente. Cuando una referencia de patrón proviene de un objeto ya marcado
como préstamo de solo lectura, conserva también ese atributo y su registro.
Los orígenes que no se pueden vincular a un propietario comprobado se rechazan.

## Validación y límites

`nsc.compiler.readonly-pattern-loan` utiliza un propietario move-only mínimo,
independiente de las familias públicas en preparación. La lectura, el fin del
préstamo, el receptor ordinario y un patrón mixto ejecutaron 42 en O0 y O2;
el análisis rechazó la escritura, el reemplazo o movimiento del propietario
durante el préstamo y el origen temporal. Se ejecutó con el compilador aislado
en A. Tras el ajuste que limitó el registro a receptores move-only o vistas,
los controles existentes de patrones fundamentales, exhaustividad y guardas
volvieron a vincularse incrementalmente y pasaron.

Se probaron además casos de Rc y Arc desde fuentes, pero esas familias aún no
están registradas ni verificadas como SDK binario. Las pruebas se ejecutaron en
un árbol con otros cambios en curso: no constituyen validación aislada de la
corrección ni cubren todos los tipos de receptor, patrones condicionales, match de
expresión, formas de escape o rebindings. La salida se observó directamente y
no se conservó un log separado. La bitácora detallada de esta investigación es
`prestamo-get-punteros.md`.

## Vista temporal encadenada en un patrón

La prueba dirigida se amplió con `match (owner.Borrow().Get())`: la vista
temporal contiene su propio campo y `Get()` devuelve una referencia a ese campo.
La compilación actual rechaza la lectura con `borrowed local requires...`
(`readonly-chain-before.log`); los controles anteriores pasaron hasta ese
punto. La reproducción pública de Rc/Arc ya estaba registrada en
`prestamo-get-punteros.md` y la prueba de punteros también se detuvo ahí
(`39-receiver-pointer-control.log`).

Decisión de trabajo: conservar la vista temporal hasta el final del `match` y
atar el préstamo al dueño comprobado que produjo la vista. Para el primer
ensayo se requiere un sujeto `match` de referencia de solo lectura, una llamada
exterior con retorno `ref` ligado a `this`, un receptor que sea una vista sin
drop devuelta por una llamada de instancia sobre un identificador local o
parámetro por valor move-only y un dueño que siga vivo durante el patrón. La
vista temporal se materializa en almacenamiento del marco mediante
`TemporaryAddress`; se comprobará en ejecución con referencia a su propio
campo. Reemplazar o mover el dueño dentro del patrón debe rechazarse; después
del patrón debe permitirse. Una fábrica de dueño temporal sigue rechazada. No
se habilitan campos de dueño, vistas con drop ni otras formas hasta demostrar su
procedencia y limpieza.

El análisis enlaza ahora el sujeto sintético del patrón al dueño local o
parámetro que produjo la vista temporal, dentro de las condiciones anteriores.
El build incremental pasó (`readonly-chain-build.log`, hash
`31522081a37f1982`). La prueba independiente ejecutó lecturas de campo propio
de la vista y de los controles en O0/O2; rechazó escritura, movimiento y
reemplazo durante el patrón, permitió reemplazar un dueño ajeno o hacerlo
después del patrón y rechazó el dueño temporal
(`readonly-chain-expanded.log`). La primera ejecución posterior al cambio
esperaba otro texto de diagnóstico para el dueño temporal, aunque este ya se
rechazaba; se ajustó únicamente esa expectativa (`readonly-chain-after.log`).

Cinco casos seleccionados de las familias públicas pasaron para Rc y Arc:
lectura O0/O2, rechazo de reemplazo y movimiento, reemplazo posterior O0/O2 y
rechazo de dueño temporal (`readonly-chain-families.log`). La prueba completa
dirigida de punteros avanzó más allá de la lectura encadenada de Rc O0/O2 y
luego falló en el caso ya pendiente de dueño guardado en campo
(`readonly-chain-pointer-control.log`); no se afirma que haya pasado ni se
retiró ese caso. Los controles de operadores fuente, binarios y structs pasaron
(`readonly-chain-operator-*.log`). Las pruebas usan el árbol de trabajo con
otros cambios en curso, sin certificar el SDK binario ni todos los patrones o
formas de receptor. El cambio permanece sin commit.

## Dueño en un campo durante `match`

La prueba pública se detuvo en `match (holder.value.Get())`; una reproducción
independiente con un dueño move-only en un campo también se rechazó
(`readonly-field-pattern-before.log`). La protección existente para vistas de
campo registra el objeto raíz y los campos posibles, comprueba alias y rechaza
llamadas potencialmente mutantes. El sujeto sintético de `match` es una
referencia y no se registraba como vista, por lo que esa protección no se
aplicaba.

El ensayo enlazará únicamente una referencia de patrón de solo lectura que
provenga de un método con retorno `ref` ligado a `this`, invocado sobre un
campo move-only directo de un objeto local RAII comprobado. Registrará en el
sujeto la misma raíz y campo usados por las protecciones existentes. Se
probarán lectura, reemplazo y movimiento directos, reemplazo por alias,
llamada potencialmente mutante, modificación de otro campo y reemplazo después
del patrón. Los demás receptores y fuentes conservan las reglas actuales. La
política de llamadas es conservadora: mientras el campo está prestado, una
llamada que pudiera modificarlo se rechaza sin demostrar que lo haga.

El build incremental pasó (`readonly-field-pattern-build.log`, hash
`7ffb61c0c3a60451`). La prueba independiente ejecutó la lectura O0/O2 y rechazó
reemplazo, movimiento, alias y llamada potencialmente mutante; permitió cambiar
otro campo y reemplazar el campo después del patrón
(`readonly-field-pattern-after.log`). Los siete casos seleccionados de las
familias públicas pasaron para Rc y Arc: lecturas y controles válidos O0/O2,
además de los cuatro rechazos semánticos (`readonly-field-pattern-families.log`).
Los controles de operadores fuente y binario también pasaron
(`readonly-field-pattern-operator-source.log` y
`readonly-field-pattern-operator-binary.log`).

La validación usa fuentes en el árbol de trabajo y no acredita todas las
formas de alias, receptores calculados, campos anidados o consumo binario. La
prueba completa de punteros no se repitió después de este cambio; el último
resultado completo sigue siendo fallido en el caso que ahora pasó por separado.
Los cambios permanecen sin commit.

### Comprobación posterior del alias y consumo binario

El control de alias se corrigió para declarar `borrowed Holder alias = holder`.
La prueba independiente volvió a pasar con ese alias
(`A:/nsc-fixes-evidencia/readonly-field-pattern-source-alias.log`); los siete
casos seleccionados de las familias públicas también pasaron para Rc y Arc
(`A:/nsc-fixes-evidencia/readonly-field-pattern-families-alias.log`). Las
lecturas, el cambio de otro campo y el reemplazo posterior devolvieron 42 en
O0 y O2; se rechazaron reemplazo, movimiento, alias y llamada durante el
préstamo. La selección no equivale a ejecutar todos los casos de la prueba.

Un consumidor separado de Core binario aislado comprobó Rc y Arc: lectura,
uso ordinario del alias, modificación de otro campo y reemplazo posterior
devolvieron 42 en O0 y O2. El reemplazo directo, el reemplazo por el alias y
la llamada potencialmente mutante se rechazaron durante el análisis. Para el
alias, el diagnóstico señala el reemplazo del campo prestado; el control sin
el patrón demuestra que el alias es válido. Comando y salida:
`A:/nsc-fixes-evidencia/readonly-field-pattern-sdk-alias.cmd` y
`A:/nsc-fixes-evidencia/readonly-field-pattern-sdk-alias.log`.

Se usaron el compilador incremental aislado con hash de build
`7ffb61c0c3a60451` y `A:/nsc-fixes-evidencia/pointers-sdk-refresh/NativeSharpCore.lib`,
generado anteriormente desde la copia aislada. La biblioteca no se regeneró
con ese compilador para esta comprobación; su procedencia y hashes constan en
`A:/nsc-fixes-evidencia/pointers-sdk-refresh-scope.md`. Estos controles no
demuestran la vida de módulos, todos los alias o receptores, ni el contrato
completo de las cuatro familias. La prueba binaria permanece sin registrar en
CTest y los cambios siguen sin commit.

El consumidor binario se amplió con el intento de mover el campo prestado y
exige ahora el diagnóstico específico para cada rechazo: movimiento parcial,
sobrescritura directa o por alias y llamada potencialmente mutante. Rc y Arc
rechazaron esas cuatro operaciones por el motivo esperado; lectura, control de
alias, modificación de otro campo y reemplazo posterior ejecutaron 42 en O0 y
O2. La ejecución sin caché está en
`A:/nsc-fixes-evidencia/readonly-field-pattern-sdk-diagnostics.cmd` y `.log`.
Usó el compilador incremental SHA-256
`0d48e6a9098d9d24a33be2cc1e4a9d909854c7e92906675e32411b517609fe73` y el
Core binario anterior indicado arriba. Esta selección no cubre procedencias
calculadas, campos anidados ni vida del módulo productor.

### Controles adicionales desde fuentes

Con el mismo compilador aislado y las fuentes actuales del Core se ejecutaron
dos selecciones de `nsc.sdk.shared-public-pointers`. En
`A:/nsc-fixes-evidencia/readonly-pattern-following-cases.log` pasaron para Rc
y Arc los siete casos seleccionados: lectura de clase y control de patrón
mutable local en O0/O2, más los rechazos esperados de reemplazo del dueño de
una vista, escritura y escape de clase anidada, referencia mutable de una vista
de solo lectura y reemplazo del dueño tras cambiar la vista de procedencia.

En `A:/nsc-fixes-evidencia/readonly-pattern-class-cases.log` pasaron seis
casos por familia: lectura directa y mediante vista en O0/O2, y rechazo de
escritura, escape, reemplazo del dueño y llamada mutante. Los dos logs
identifican los comandos, el compilador y las fuentes utilizados. Se trata de
selecciones dirigidas, no de la prueba completa de punteros; tampoco cubren
WeakRc o WeakArc como dueños de las vistas, ni prueban el consumo binario de
estos casos particulares. La corrección sigue en un árbol con cambios locales.

## Destrucción de un objeto que contiene el dueño prestado

Se reprodujo que `delete` y `destroy` podían invalidar un préstamo de un campo
anidado, directamente o mediante alias. El análisis consulta ahora la vista
activa, conserva conservadoramente la procedencia desconocida de lecturas
estáticas y llamadas, y registra ese origen también en reasignaciones. Una
identidad estática de fuente disponible y forma estrictamente comprobada puede
conservar el origen de su parámetro. Esta regla puede rechazar operaciones
válidas cuando no se puede demostrar la independencia del valor.

Con el compilador aislado SHA-256 `510b15206d816612fa59021ced6138c3df17971aa79b3b3f8fc3ddb4797ffd6c`,
pasaron controles dirigidos con Core binario y fuentes para Rc y Arc, y los
casos de alias estático aplicables a Arc. Los comandos, resultados, límites y
casos previos se conservan en `A:/nsc-fixes-evidencia/readonly-nested-chained-scope.md`
y sus logs. No se acreditan todas las formas de alias ni campos indexados. La
otras formas de vista temporal encadenada y la integración siguen pendientes; no hay commit.

El ensayo posterior admite `match (outer.inner.value.Borrow().Get())` cuando el
dueño pertenece a una cadena de campos comprobada desde un objeto local RAII y
la vista temporal no tiene destructor. Conserva la raíz y los campos durante el
préstamo. Las pruebas dirigidas para Rc y Arc pasaron con Core binario y con
fuentes; incluyen lectura, reemplazo, movimiento, escritura y destrucción por
alias, además de los casos estáticos aplicables a Arc. También pasaron los
controles previos de patrón. Los comandos, hash y límites figuran en
`A:/nsc-fixes-evidencia/readonly-nested-chained-scope.md`; siguen pendientes
otros receptores, alias, rutas de excepción e integración. No hay commit.

El ensayo del receptor indexado `holders[0].value.Get()` ya admite la lectura
para Rc y Arc y conserva posibles fuentes de los objetos insertados en el
arreglo. Pasaron 20 casos dirigidos con Core binario, con las excepciones Rc
para almacenamiento estático, y cuatro por familia con fuentes. También se
probaron inserciones mediante campos estáticos y de instancia. Las llamadas
que reciben arreglos y su publicación en campos se tratan conservadoramente:
pueden producir rechazos de objetos independientes. El detalle y los límites
están en `A:/nsc-fixes-evidencia/readonly-indexed-scope.md`; faltan otras
formas de inicialización, alias y escape, y el ensayo no tiene commit.

Se añadieron controles y seguimiento para `new Holder[] { parameter }` y
colecciones que expanden otro arreglo. Para Rc y Arc se rechazan los casos
probados de destrucción de un objeto compartido y pasan los controles de
arreglos independientes en O0/O2. El seguimiento también cubre la reasignación
del arreglo completo desde un inicializador con un parámetro o una expansión:
los casos compartidos se rechazan y el control independiente devuelve 42 en
O0/O2, tanto para Rc como para Arc con Core binario. La selección con fuentes
incluye ocho casos indexados por familia, incluidas esas reasignaciones. No se
concluye que estén cubiertos todos los inicializadores o escapes; resultados y hash se conservan en
`A:/nsc-fixes-evidencia/readonly-indexed-scope.md`.

La selección `readonly-indexed-parameter-arrays.cmd` incorpora una lectura
válida y dos controles de destrucción para un arreglo recibido como parámetro,
con Core binario y el compilador SHA-256
`5bf7b36a3f6bdc33e5f849ea71168f2cb56030adf920a53c5b7eaefee8cd2344`.
En Rc y Arc, los tres casos se rechazan al formar la vista con `borrowed local
requires a direct live owner or borrowed identifier source`: la lectura válida
no llega a ejecutarse y los controles destructivos no llegan al análisis que
debería impedir destruir un alias. La selección falla deliberadamente respecto
al comportamiento esperado; `readonly-indexed-parameter-arrays.log` conserva
los diagnósticos. Admitir la lectura requiere mantener la vida del parámetro y
comprobar alias sin asumir que arreglos y objetos recibidos son independientes;
esta evidencia no establece aún la corrección ni justifica retirar la guarda.

El ensayo posterior permite esa raíz cuando es un parámetro de arreglo nativo
pasado por valor y liga la vista a dicho parámetro. La liberación del arreglo
mientras vive la vista también se comprueba mediante sus posibles raíces, para
cubrir un alias local; la comprobación de arreglos `owned` locales existente se
conserva. Con el compilador incremental SHA-256
`295a2bf645ba05f5c7fb2d58a12e606db808f6226529a46a50f99bf04922b00b`, la
selección binaria pasó lectura y liberación de un arreglo independiente en
O0/O2 para Rc y Arc; rechazó destrucción de objetos potencialmente compartidos,
sobrescritura, llamada mutante y liberación del arreglo directo o por alias
(`readonly-indexed-parameter-arrays.log`). La selección con fuentes pasó la
lectura y los rechazos de destrucción y sobrescritura para ambas familias en
O0/O2 (`readonly-indexed-parameter-source.log`); también pasó la selección
indexada anterior (`readonly-indexed-prototype2.log`). Los diagnósticos previos
permanecen en `readonly-indexed-parameter-arrays-before.log`. La comparación de
parámetros es conservadora y puede rechazar referencias independientes; estas
pruebas no cubren todos los alias, llamadas ni escapes, y los cambios no tienen
commit.

Un control adicional mostró que, con Arc, `delete[] Registry.Saved` se aceptaba
mientras vivía una vista procedente de un arreglo parámetro que podía ser el
mismo (`readonly-indexed-static-alias-before.log`); la vía insegura no se
ejecutó. En Rc el programa ya era rechazado por la prohibición de almacenamiento
estático de tipos que contienen Rc, por lo que esa ejecución no reproduce el
fallo de alias. La protección provisional trata también como origen desconocido
un campo estático o el resultado de una llamada usada por `delete[]`. Con el
compilador SHA-256
`70d91c95451b7329a8cf3f040cd56aada9bdd41d0f198b5ac9bdd41b09333c531b`,
la selección `readonly-indexed-parameter-arrays.cmd` rechaza las dos variantes
para Arc; Rc conserva el diagnóstico por almacenamiento estático. Los demás
controles de ambas familias pasaron, incluida la lectura en O0/O2 y la
liberación de un arreglo local independiente. También pasaron los controles
indexados anteriores y la selección con fuentes. La regla es conservadora: una
llamada o campo estático que en realidad sea independiente puede rechazarse;
no se ha demostrado cobertura de todos los orígenes posibles.
