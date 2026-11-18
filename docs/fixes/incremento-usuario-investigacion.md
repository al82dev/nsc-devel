# Incremento y decremento definidos por usuario

Las dos reproducciones originales siguen rechazadas con el compilador aislado:
`operator ++` y `operator --` reciben el diagnóstico explícito de
`MemberParser.cpp` porque escribir el resultado mediante una segunda
evaluación del operando puede duplicar sus efectos. Comandos y salidas:
`A:/nsc-fixes-evidencia/39-increment-current.cmd` y los dos logs asociados.
Los archivos originales se conservaron; sus comentarios los describen como
intencionadamente inválidos con el compilador actual.

La decisión de diseño para desarrollar el caso es permitir las sobrecargas,
con un destino modificable evaluado una vez; prefijo produce el valor nuevo
y posfijo el anterior. La llamada, la escritura y la gestión de propiedad deben
mantener coherencia también ante excepciones. Los casos donde no pueda
garantizarse esa semántica deben diagnosticarse, sin transformar la expresión
en una segunda evaluación del receptor. La decisión no afirma implementación.

El análisis muestra tres etapas que hoy impiden cerrar el caso:

- `MemberParser.cpp` rechaza explícitamente la declaración, aunque el mapa de
  nombres reconoce `op_Increment` y `op_Decrement` y valida aridad uno.
- `analyzeOverloadedUnary` excluye ambos tokens; la ruta incorporada
  `analyzeIncrementOrDecrement` exige destino numérico o puntero. La selección
  de sobrecarga y su compatibilidad con el tipo del destino aún faltan.
- El descenso HIR usa `CompoundStore` con constante uno y `booleanValue` para
  elegir el valor posfijo. Una llamada ordinaria al operador no implementa por
  sí misma la actualización ni conserva la evaluación única de su destino.
  Deben investigarse las rutas de propiedades, indexadores, referencias,
  valores `owned` y excepciones antes de elegir el mecanismo de almacenamiento.

No se ha retirado la prohibición ni cambiado la semántica de producto. Falta
una regresión que contraste prefijo y posfijo, ambos operadores, destinos con
efectos, errores de tipo y aridad, propiedad y excepciones, y los controles de
incremento numérico en O0/O2. El resultado actual continúa pendiente.

## Comprobación dirigida actual

Los dos ejemplos originales se volvieron a analizar sin modificaciones mediante
`A:/nsc-fixes-evidencia/39-original-current-check.cmd`; ambos siguen rechazados
en la declaración (`39-increment-latest.log` y `39-decrement-latest.log`). El
comando conserva el código de cada análisis dentro de su log y su propio código
de salida no indica que los ejemplos hayan pasado.

Un control independiente de los operadores incorporados aplicó prefijo y
posfijo sobre un elemento de arreglo cuyo índice incrementa un contador. Las
cuatro expresiones observaron los valores esperados y exactamente cuatro
evaluaciones del índice en O0 y O2; el ejecutor terminó con código 0
(`A:/nsc-fixes-evidencia/39-builtin-once-control/run.cmd`). El compilador de
ambos builds informa hash `c51f19ed93efd1c3`. Esto verifica esos operadores
incorporados, no una sobrecarga de usuario ni todos los destinos posibles.

Se preparó `tests/integration/operator_increment_decrement.py` para exigir a
sobrecargas de un struct los mismos resultados y evaluación única. La prueba
falló al compilar en O0 y O2 con los dos diagnósticos de prohibición esperados
(`A:/nsc-fixes-evidencia/39-user-increment-before.log`); no se registró como
aprobada. Es una primera regresión, no la matriz completa: aún faltan clases y
retornos owned como los ejemplos originales, excepciones, propiedades,
indexadores, aridad y tipos inválidos. No se ha modificado el compilador.

Para no atribuir a la sobrecarga un fallo de infraestructura del fixture, un
control separado creó el mismo tipo de arreglo de estructuras y ejecutó
operaciones equivalentes mediante métodos y asignaciones explícitas. Devolvió
42 en O0 y O2 (`A:/nsc-fixes-evidencia/39-struct-array-control/run.cmd`). Esta
comprobación no verifica la evaluación única del operador de usuario; esa
parte permanece en la regresión que hoy se rechaza al analizar.

## Primera etapa de implementación, aún sin habilitar el uso

Objetivo autorizado: implementar ambos operadores respetando una única
evaluación, prefijo/posfijo, propiedad y excepciones. Esta etapa cambia solo
el reconocimiento de sus declaraciones y la identificación semántica de sus
contratos; el análisis de expresiones seguirá rechazando su aplicación hasta
que el descenso y la escritura estén implementados. No es cierre ni pretende
sustituir el soporte completo.

Cadena observada: `MemberParser.cpp` rechaza incondicionalmente declaraciones
de aridad uno, aunque el parser ya conoce sus nombres; `TraitSemantics.cpp` y
`OperatorClassification.cpp` no registran sus contratos. La primera etapa
incluirá esos tres archivos, una comprobación dirigida del catálogo y los dos
ejemplos originales sin modificarlos. Se preservan la aridad, los demás
operadores, todas las restricciones de la campaña, los archivos locales ajenos
y la prohibición de interoperabilidad C/C++, instalaciones, merge y push.
La regresión de uso debe seguir fallando de forma segura hasta completar el
descenso; no se registrará como aprobada ni se omitirá su resultado.

El ensayo retiró el rechazo sintáctico incondicional y registró las dos
identidades en el catálogo y en la clasificación semántica. El build
incremental de `nsc` y del test de catálogo terminó con código 0 en
`39-declaration-build.log`; el compilador resultante informa hash
`361648217d68d695`. Los ejemplos originales sin cambios pasaron `--check`
(`39-increment-declaration-after.log` y `39-decrement-declaration-after.log`).
Las declaraciones con dos parámetros se rechazaron en ambos operadores con
`user-defined operator has invalid arity` (`39-invalid-arity/plus.log` y
`minus.log`). El control numérico de prefijo/posfijo y evaluación única volvió
a ejecutar 42 en O0/O2 (`39-builtin-once-after.cmd`, salida 0).

La prueba preparada de uso sobre struct sigue fallando en O0/O2, ahora en el
análisis de la expresión porque exige un destino numérico o puntero
(`39-user-increment-declaration-after.log`). Esto confirma que la declaración
aceptada no equivale a soporte de uso. La prueba `nsc.ns3.t05.operator-traits`
terminó fallida: 19/20 comprobaciones pasaron, incluida la nueva identidad;
la restante esperaba rechazar un operando temporal `in`, pero el prototipo
independiente `70-temporary` del árbol conjunto lo acepta
(`39-declaration-ctest.log`). No se modificó esa expectativa ni se atribuye la
falla a las nuevas identidades sin una comparación aislada. No hay commit de
esta etapa; permanecen pendientes la selección, descenso, ownership, manejo de
excepciones y controles completos de las sobrecargas.

## Etapa siguiente: selección sin emisión

El descenso de `CompoundStore` solo calcula aritmética incorporada. Aceptar la
sobrecarga antes de cambiarlo emitiría una operación equivocada o rechazaría en
una fase tardía. Se preparará primero la selección de `op_Increment` y
`op_Decrement` mediante la resolución de candidatos existente: destino
modificable, una sola coincidencia y resultado asignable al destino. Hasta que
la generación esté terminada se emitirá un diagnóstico explícito de capacidad
no implementada después de seleccionar; ninguna llamada se ejecutará. Esta es
una etapa provisional, no un subconjunto entregado como solución final. Se
comprobarán candidatos válidos, destino constante, tipo de retorno incompatible
y operadores numéricos sin modificación. La evaluación única y la transferencia
de ownership se implementarán y probarán antes de retirar la barrera.

El ensayo de selección quedó en `analyzeOverloadedUnary`: busca candidatos de
ambos tokens y exige destino modificable y conversión implícita del resultado al
tipo del destino. Después de seleccionar emite deliberadamente el diagnóstico
`user-defined increment/decrement update is not implemented`; no emite una
llamada incompleta. La compilación incremental terminó con código 0
(`39-selection-build.log`), con compilador de hash `d4d33dc68010fa76`.

La regresión ampliada identifica esa barrera para prefijo y posfijo de ambos
operadores en O0/O2; termina con código 1 porque todavía espera ejecución 42.
Además, verifica que un parámetro `in` no sea destino y que un retorno `int`
no se asigne a `Counter`: ambos se rechazaron con sus diagnósticos específicos
(`39-selection-regression-expanded.log`). El control incorporado numérico
volvió a devolver 42 en O0/O2 (`39-builtin-once-selection.cmd`, código 0).
El ensayo no demuestra el resultado de conversiones, generación de llamadas,
propiedad, excepciones o destinos de acceso especiales. Sigue sin commit y no
constituye la implementación completa.

## Ensayo de descenso con destino único

Se preparará un ensayo limitado a estructuras copiables, parámetro por valor y
retorno del mismo tipo. La barrera continuará para clases, valores move-only,
parámetros por referencia, conversiones y demás formas hasta implementar y
verificar su transferencia. El objetivo del ensayo es probar la arquitectura,
no declarar cerrado el caso ni reducir el contrato solicitado.

HIR debe conservar la dirección ya calculada y el miembro seleccionado. La
ruta iterativa y la ruta alternativa deben coincidir. MIR debe transportar una
llamada estática que LLVM pueda invocar con el valor ya cargado mediante el
reemplazo de argumentos existente; después se almacena el nuevo valor y se
devuelve el anterior para posfijo o el nuevo para prefijo. No se debe evaluar
de nuevo el destino y la escritura debe ocurrir después de completar la llamada.
Se mantendrán los controles numéricos y se probarán excepciones, destinos con
efectos, aridad y tipos inválidos antes de ampliar el alcance. El código y las
pruebas originales de otros trabajadores se conservan.

El prototipo usa `CompoundStore` con la identidad del miembro y una operación
distintiva; MIR resuelve el nombre del destino y LLVM invoca la sobrecarga con
el valor cargado de la dirección ya calculada. El argumento auxiliar generado
en LLVM solo aporta el tipo al adaptador de ABI y se reemplaza por el valor
real. El nodo conserva un segundo hijo `DefaultValue` por la forma histórica
de `CompoundStore`; LLVM no lo evalúa en esta ruta. La interacción de ese hijo
con todos los consumidores y optimizadores anteriores a LLVM no está probada.
La escritura se efectúa después de que la llamada retorne. La selección aún
mantiene la barrera para todas las formas fuera del ensayo indicado.

La compilación incremental pasó (`39-store-prototype-build.log`), con build
hash `0dae0748ca0d15e0`. `39-store-wide-test.log` registra salida 42 en O0 y
O2 para estructuras de 4 y 16 bytes, con prefijo, posfijo e índices con
efectos; la misma prueba conserva los rechazos de destino readonly y retorno
incompatible. `39-store-ir/llvm.log` muestra una única llamada a `Index`, una
carga, llamada a `op_Increment`, escritura del resultado y uso del valor
anterior para posfijo. En un control separado el operador lanzó y el catch
observó índice evaluado una vez y destino sin cambio en O0/O2
(`39-store-exception-probe/run.cmd`, salida 0). El control numérico previo
también volvió a pasar (`39-builtin-once-store-prototype.cmd`).

Un uso con clase y retorno owned todavía recibe el diagnóstico deliberado
de capacidad pendiente (`39-class-barrier/check.log`). No se han acreditado
propiedades, indexadores, bibliotecas binarias, parámetros `in`/`ref`, objetos
move-only, conversiones, corrutinas ni cleanup de ownership. El mecanismo de
optimización, caché y todos los consumidores de `CompoundStore` aún requieren
revisión; los controles no certifican seguridad general ni justifican retirar
la barrera de las formas restantes. Todo permanece sin commit.

## Ensayo de parámetro `in`

La regresión amplió el mismo fixture a operadores con parámetro `in`; las
estructuras de 4 y 16 bytes se rechazaron en O0/O2 por la barrera, mientras las
variantes por valor ejecutaron 42 (`39-in-before.log`). La ABI de llamadas a
operadores existente espera para `in` la dirección del operando, sin envolverla
como agregado por valor. El siguiente ensayo conserva la carga previa para el
resultado posfijo y pasa a la llamada la dirección ya calculada. Se limita a
estructuras copiables, operador fuente, tipo exacto y destino direccionable;
mantiene las barreras para propiedad, `ref`, conversiones y binarios. Se probará
en ambos tamaños y optimizaciones antes de atribuirle corrección.

El ensayo pasó la dirección al argumento `in` y mantuvo la carga previa para el
resultado posfijo. La compilación incremental terminó con código 0
(`39-in-build.log`); el compilador informa hash `a270ff584f503fdf`. La matriz
`39-in-after.log` ejecutó 42 en ocho combinaciones: structs de 4 y 16 bytes,
parámetros por valor o `in`, O0 y O2. Incluye las cuatro formas del struct
pequeño y los controles de aridad semántica del destino y retorno.

`39-store-in-ir/llvm.log` muestra una sola llamada a `Index`, la carga del
valor anterior, la llamada a `op_Increment` con la dirección del elemento y
la conservación del valor anterior para posfijo. Un operador `in` que lanza
se capturó y dejó intacto el elemento con el índice evaluado una vez en O0/O2
(`39-in-exception/run.cmd`, salida 0). La clase con retorno owned siguió
rechazada (`39-class-barrier/in-check.log`). La prueba no acredita `ref`,
propiedad, binarios, conversiones ni otros destinos, y el ensayo completo
sigue sin commit. El análisis de optimización y cachés permanece pendiente.

## Frontera binaria: productor

La prueba `tests/integration/operator_increment_binary.py` intenta producir una
biblioteca NativeSharp y consumirla por separado. El productor falla antes de
emitirla: la reconstrucción de la superficie no puede escribir `op_Decrement`
(`39-binary-before.log`). `metadataOperatorToken` no incluye ninguno de los dos
nombres recién admitidos. Se añadirá exclusivamente el mapeo de esos nombres
a sus símbolos en `src/Driver/PrecompiledSurface/ContractMaterialization.cpp`,
una ruta de API NativeSharp; no se modifica el formato de metadata ni la
interoperabilidad C/C++. La barrera del consumidor precompilado permanece hasta
comprobar el productor y su ABI. No se atribuye todavía un resultado al
consumidor, que no llegó a compilar.

Con el mapeo corregido, el build incremental pasó y el productor emitió su
biblioteca con el compilador hash `aaf0f9145006444b`. El consumidor llegó a
selección y se rechazó en O0/O2 por la barrera explícita de métodos
precompilados (`39-binary-producer-after.log`); no se ejecutó. El ensayo
siguiente conserva todas las demás condiciones —struct copiable, parámetro
exacto por valor o `in`, retorno exacto y destino direccionable— y comprobará
si el MIR precompilado conserva el símbolo y la ABI. No extiende el soporte a
clases, propiedad, `ref`, conversiones ni otros formatos.

La compilación incremental `39-binary-consumer-build.log` pasó. Con el
compilador hash `1d262296d7f34d37`, la prueba produjo una biblioteca en O0 y
el consumidor compiló y ejecutó 42 en O0/O2 (`39-binary-consumer-after.log`).
La prueba se amplió después con un struct de 16 bytes y operador por valor,
además del struct de 4 bytes con `in`; ambos verifican prefijo/posfijo e
índices con efectos en el mismo consumidor (`39-binary-wide-after.log`,
salida 0). Los ocho controles de fuentes y los rechazos semánticos volvieron
a pasar con el mismo build (`39-binary-source-controls.log`).

El mapeo de nombres es parte de la reconstrucción de la API NativeSharp; no
cambia la metadata ni implementa interoperabilidad C/C++. La prueba produce y
consume en la misma ejecución sin caché; no verifica DLL, productor O2, caché,
clases, ownership ni excepciones binarias. El árbol y el SDK siguen incluyendo
otros cambios locales y esta etapa no tiene commit.

## Ensayo propuesto de parámetro `ref`

La regresión extendida a `ref` conserva las ocho combinaciones previas por
valor e `in`, que pasaron; las cuatro combinaciones `ref` se rechazaron con la
barrera deliberada (`39-ref-before.log`). Se probará únicamente con structs
copiables, tipos de parámetro y resultado exactos y destino direccionable.
El resultado posfijo debe capturar el valor anterior antes de la llamada; la
dirección del destino se pasa al operador y el resultado se escribe solamente
si la llamada retorna. Si el operador modifica su parámetro y luego lanza, su
propia modificación puede persistir: no se promete revertirla.

La ruta LLVM auxiliar utiliza `wrapIndirectArgument`, que para un struct de
16 bytes envuelve valores por copia; pasarle una dirección marcada como struct
sería incorrecto para `ref`. El ensayo identificará el argumento auxiliar como
puntero para `ref`, sin alterar el adaptador común de llamadas. Se comprobarán
ambos tamaños y optimizaciones, además de una mutación real del destino y los
controles existentes. El resultado seguirá siendo provisional hasta verificar
la ABI y los límites binarios, excepciones y consumidores del nodo.

El build incremental pasó (`39-ref-build.log`), compilador hash
`0a38d7e9a4b7fd51`. `39-ref-after.log` registra 14 ejecuciones con resultado 42:
structs de 4 y 16 bytes por valor, `in` y `ref`, más un `ref` que modifica el
destino; cada variante se ejecutó en O0/O2. El caso con mutación comprobó el
valor posfijo anterior y una sola evaluación del índice. Los negativos de
destino no modificable y resultado incompatible se siguieron rechazando.

El primer intento de añadir excepciones al proyecto mínimo falló por no
referenciar Core (`39-ref-exception-after.log`), no por el operador. El
proyecto separado con Core aislado comprobó en O0/O2 que, al modificar el
destino y lanzar, el catch observa esa modificación y una evaluación del índice
(`39-ref-exception-sdk.log`). Un productor de biblioteca en O0 y consumidores
en O0/O2 verificaron `ref` con mutación de un struct de 16 bytes y los controles
anteriores (`39-ref-binary-after.log`). No se probó productor O2, DLL, caché,
excepción binaria, clases, propiedad ni las demás rutas de optimización y
consumidores de `CompoundStore`. El trabajo permanece sin commit y el caso 39
no está cerrado.

## Investigación de clases con `owned`

Los ejemplos originales declaran operadores sobre `IntegerBox` que devuelven
`owned`, pero no llegan a utilizarlos. Para probar el uso sin modificarlos se
añadió `tests/integration/operator_increment_owned.py`: conserva el valor
posfijo como un propietario RAII, obtiene el prefijo como vista prestada y
cuenta las tres destrucciones esperadas. Actualmente se rechaza en O0/O2 con
la barrera del operador y diagnósticos de inicialización RAII y del origen del
préstamo (`39-owned-before.log`); todavía no se ejecuta.

La decisión de trabajo para un destino RAII y un operador que recibe una
referencia de clase sin transferirla es que el posfijo entregue el propietario
anterior, mientras el destino conserva el nuevo; el prefijo expone el nuevo
como préstamo ligado al destino. Si se ignora el resultado posfijo, el dueño
anterior deberá destruirse al terminar la expresión. Esta decisión debe
integrarse con el análisis de préstamos, el cleanup y los errores; aceptar el
puntero anterior como simple copia produciría propietarios duplicados o fugas.

El control explícito `39-owned-control/Main.ns` mostró que la asignación directa
de una llamada a un local RAII se rechaza: exige `move` (`39-owned-control.log`).
La variante con dos movimientos explícitos pasó y destruyó ambos objetos una
vez en O0/O2 (`39-owned-move-control.log`). Una segunda variante llama primero
al productor y solo después mueve el valor anterior y reemplaza el destino;
también pasó O0/O2 (`39-owned-call-first.log`). Este orden es candidato para
preservar el destino si la llamada falla, pero ese control no ejecutó una
excepción y no demuestra el cleanup de un resultado posfijo ignorado.

El descenso provisional `CompoundStore` solo carga, llama y sobrescribe; no
transfiere ni destruye el propietario anterior, no identifica el origen del
préstamo prefijo y no ejecuta las protecciones de reemplazo con préstamos
activos. Por eso la barrera de clases continúa y no se considera implementada
la ruta `owned`. También faltan campos, arreglos, propiedades, excepciones,
ABI binaria y composición de expresiones; no se han generalizado los controles
manuales a esas formas.

Un control posterior con Core aislado hizo lanzar la llamada antes de mover el
destino: el `catch` observó el objeto original y su valor, sin destrucciones
prematuras; al salir se destruyó una vez en O0/O2
(`39-owned-throw-control.log`). Esto confirma el orden en ese programa manual,
no la emisión del operador todavía rechazado.

Otro control comprobó que la asignación explícita a un local RAII se rechaza
cuando mantiene un préstamo vivo (`39-owned-borrow-control.log`): la ruta del
operador deberá aplicar la misma restricción antes de escribir. La regresión
propietaria ahora incluye un posfijo descartado que exige destruir el valor
anterior y un negativo de préstamo activo que exige ese diagnóstico.
Actualmente falla en O0/O2 por la barrera y el negativo recibe el diagnóstico
genérico de funcionalidad pendiente (`39-owned-safety-before.log`). No se
cuenta como prueba de protección del operador.

En el análisis existente, el inicializador RAII acepta un resultado con
`ownership == Owned` y `ownershipTransfer`, mientras la vinculación de un
préstamo local exige un origen rastreable. El HIR ya distingue posfijo con
`booleanValue`, pero el almacenamiento compuesto aún no implementa la
transferencia ni el cleanup de un resultado descartado. La implementación
necesita conectar esas etapas sin marcar como transferido el nuevo propietario
que sigue en el destino; los controles manuales no prueban esa conexión.

### Protección semántica del destino RAII

El control anterior muestra una comprobación faltante específica de esta ruta:
las asignaciones llaman `rejectUnavailableAssignmentOwner`, pero el análisis
de la sobrecarga unaria no la llama. Antes de habilitar clases se aplicará esa
misma comprobación únicamente cuando el operando directo sea un identificador
local RAII de tipo referencia y haya candidatos para `++` o `--`; se conserva
la barrera posterior hasta completar transferencia, cleanup y préstamo. Esto
debe rechazar tanto el reemplazo con préstamo activo como el de un local fijado,
sin modificar la semántica de los operadores numéricos o structs. La regresión
propietaria debe reconocer el diagnóstico de préstamo; sus casos de ejecución
seguirán fallando hasta que exista la implementación restante.

La comprobación se conectó en `IdentifierAnalyzer.cpp` antes de resolver la
sobrecarga, reutilizando la misma función de las asignaciones. El build
incremental pasó (`39-owner-guard-build.log`, compilador hash
`a486723f73436ec8`). La prueba dirigida ahora observa los diagnósticos
específicos tanto para el préstamo activo como para el dueño fijado
(`39-owner-guard-pinned.log`). Sus casos positivos de clase todavía fallan en
O0/O2 por la barrera, y la prueba completa retorna fallo: no se considera
aprobada. Los 14 controles de structs por valor, `in` y `ref` pasaron de nuevo
en O0/O2 (`39-owner-guard-struct.log`). No se han comprobado destinos de
campos, elementos de arreglo ni otras formas indirectas; la comprobación nueva
solo cubre el identificador local directo y permanece sin commit.

### Preparación de cleanup para el posfijo propietario

El recorrido de `ownDroppableTemporaries` ya destruye resultados propietarios
descartados y receptores temporales usando `OwnedTemporaryBorrow` y cleanup de
expresión completa, pero `isFreshDroppableTemporary` solo reconoce llamadas y
operaciones de cadena. El resultado posfijo de `CompoundStore` para una clase
propietaria debe participar en ese mecanismo; el prefijo devuelve una vista y
no debe tratarse como propietario. Se ampliará exclusivamente el reconocimiento
del nodo posfijo con miembro seleccionado y tipo referencia, manteniendo la
comprobación existente del binding `Owned` con transferencia y sin dueño
fuente. La barrera semántica para clases sigue cerrada: este cambio aislado no
habilita aún su emisión. Se volverán a ejecutar los controles de structs y el
negativo de clases, y se comprobará que la ruta de resultados temporales de
llamadas ordinarias sigue intacta con una prueba dirigida existente.

El reconocimiento quedó preparado en `HirExceptionAndCleanup.cpp`. El build
incremental pasó (`39-postfix-cleanup-build.log`, hash `f21433c07f87b581`).
Un control de llamada ordinaria propietaria descartada destruyó el objeto una
vez en O0/O2 (`39-postfix-call-control.log`) y los 14 controles de structs
volvieron a pasar (`39-postfix-struct-control.log`). Los dos negativos de dueño
prestado/fijado conservaron sus diagnósticos; los positivos de clase siguieron
rechazados (`39-postfix-owned-pending.log`), por lo que el comando conjunto
terminó con código 1 esperado. La rama nueva de cleanup del posfijo aún no se
ha ejecutado porque la barrera semántica impide llegar a ella: es preparación,
no evidencia de liberación del posfijo. Falta implementar y probar transferencia
y resultado prefijo antes de poder habilitarla.

### Preparación del reemplazo del dueño local

Para el caso aún protegido de un local RAII directo de clase, HIR debe conservar
el ordinal de su almacenamiento en ambos recorridos de descenso. MIR ya copia
el ordinal. En LLVM, después de que el operador devuelva el nuevo dueño, el
destino recibe ese dueño; el posfijo entrega el anterior al consumidor y el
prefijo destruye el anterior mediante el mecanismo existente y devuelve el
nuevo como vista. Es importante instalar primero el nuevo en el slot cuyo
cleanup ya está activo: el destructor anterior puede lanzar, y así el nuevo
continúa protegido por el dueño local. Si la llamada del operador lanza, la
escritura no se alcanza y el dueño anterior permanece en el slot. El caso
permanece rechazado hasta que el análisis y la vinculación del préstamo estén
conectados. Campos, arreglos, parámetros, conversiones y structs con drop no se
habilitan mediante este ensayo.

La preparación se aplicó en los dos recorridos HIR y en el descenso LLVM. El
build incremental pasó con hash `77db32c201b2250e`
(`39-owner-lowering-build.log`); las pruebas existentes de structs desde fuente
y de productor/consumidor binario pasaron (`39-owner-lowering-struct.log`,
`39-owner-lowering-binary.log`). La regresión de clases continúa fallando en
O0/O2 por la barrera, mientras los negativos de préstamos y pin siguen
rechazados (`39-owner-lowering-owned-pending.log`). Por ello el comando conjunto
terminó con código 1 y **no ha ejecutado la nueva ruta de propiedad**. Aún debe
verificarse con destructores, excepciones y limpieza en ejecución después de
conectar el análisis; tampoco se ha auditado todavía la interacción de su ordinal
con todas las pasadas MIR y cachés. Los cambios permanecen sin commit.

### Ensayo de conexión semántica

Se probará la conexión del caso reproducido: operando identificador local
RAII de clase, parámetro por valor no propietario del tipo exacto y retorno
`owned` del mismo tipo. El posfijo se clasifica como transferencia del dueño
anterior; el prefijo como préstamo cuyo origen es el local actualizado. El
préstamo directo de un inicializador prefijo registrará ese mismo local en el
análisis, con su ámbito máximo y las comprobaciones ya existentes. Cualquier
otra forma continúa bajo la barrera. Se ejecutará la regresión propietaria con
destructores, descarte y negativos; un resultado positivo aislado no acredita
excepciones, caché, composición ni seguridad de todos los consumidores.

La conexión se implementó en `IdentifierAnalyzer.cpp` y el vínculo de
inicializador prestado en `StatementAnalyzer.cpp`; build incremental
`39-owner-connected-build.log`, hash `566aeaeea8d55342`. La regresión
`39-owner-connected-owned.log` pasó en O0/O2: posfijo conserva el objeto
anterior, prefijo observa el nuevo, el resultado descartado libera el anterior
y los destructores se ejecutan una vez. También rechazó dueño con préstamo
activo y dueño fijado. La ampliación posterior rechazó retornar el prefijo
como valor sin calificador o como préstamo, y usarlo para crear otro dueño
(`39-owner-borrowed-escape.log`).

Un operador propietario que lanza dejó el destino y su objeto original vivos
hasta el final del ámbito; su destructor se ejecutó una vez en O0/O2
(`39-owned-op-exception.log`). La prueba utiliza Core binario de la copia en A.
Los controles fuente de structs y el productor/consumidor binario volvieron a
pasar (`39-owner-connected-struct.log`, `39-owner-connected-binary.log`).
Estas pruebas no demuestran todas las formas de escape, excepciones del
destructor, composición de expresiones, caché ni optimizadores MIR; el soporte
para clases precompiladas, campos, elementos de arreglo y propiedades sigue
rechazado. La tarea y el caso 39 permanecen abiertos y sin commit.

### Posfijo como argumento prestado

La prueba ampliada `39-owner-composition-before.log` reproduce que
`Read(value++)` se rechaza al pasar el resultado a un parámetro `borrowed`:
se interpreta su transferencia como `move`, aunque el posfijo ya reemplazó el
dueño de origen y su resultado necesita un dueño temporal hasta terminar la
llamada. Los negativos de retorno y almacenamiento en campo sí se rechazaron.
El mecanismo de préstamo de expresión completa existe, pero
`callIsFullExpressionBorrow` lo limita a strings. Se marcará únicamente el
resultado propietario del posfijo ya admitido, y se permitirá ese marcador para
un parámetro de clase explícitamente `borrowed`; no se amplía a parámetros sin
calificador ni a otros propietarios o movimientos. La prueba deberá comprobar
el objeto anterior durante la llamada y su destrucción posterior en O0/O2.

La compilación incremental pasó (`39-owner-composition-build.log`, hash
`c3c7773638e6279a`); al cambiar una estructura interna compartida tuvo 206
acciones incrementales, sin construir la batería de pruebas. La regresión
`39-owner-composition-after.log` ejecutó O0/O2 con resultado 42: la llamada
prestada leyó el valor anterior, se destruyó ese objeto al terminar la
expresión y el dueño nuevo permaneció vivo. Conservó los negativos de préstamo
activo, pin, escape por retorno, copia a dueño y escape a campo.

Una llamada con un segundo argumento que lanza después del posfijo destruyó
el temporal anterior durante el desenrollado y conservó el dueño nuevo hasta
salir del ámbito en O0/O2 (`39-postfix-arg-exception.log`). Las regresiones de
structs y consumo binario existentes pasaron con la misma compilación
(`39-composition-struct.log`, `39-composition-binary.log`). No se probó el
argumento posfijo en funciones de namespace, despacho genérico o consumidor
binario de clase; la marca se aplica a las llamadas de método. El trabajo
continúa sin commit y no acredita el caso completo.

### Función de namespace con argumento posfijo prestado

La reproducción independiente `39-postfix-namespace/Main.ns` se rechazó con
`no viable overload` (`39-postfix-namespace-before.log`, compilador hash
`c3c7773638e6279a`). La selección de funciones de namespace solo registra
préstamos temporales para strings, aunque HIR ya consume el registro de
`FreshOwnedTemporaryToBorrowedParameter`. Se extenderá esa selección solamente
al marcador de posfijo propietario y a parámetros explícitamente `borrowed`,
sin permitir paquetes ni parámetros sin calificador. La prueba comprobará la
duración del objeto anterior y ambas destrucciones en O0/O2.

La compilación incremental terminó correctamente (`39-postfix-namespace-build.log`,
hash `75702b7e217e2833`). La reproducción ejecutó 42 en O0/O2 y comprobó la
destrucción del temporal después de la llamada y del dueño actualizado al
salir (`39-postfix-namespace-after.log`). La regresión permanente incluye este
caso y rechaza tanto el parámetro sin calificador como `move` explícito hacia
`borrowed` (`39-postfix-namespace-controls.log`); los controles de structs
pasaron (`39-postfix-namespace-struct.log`).

La variante con un argumento posterior que lanza limpió el valor anterior y
conservó el actualizado hasta el final del ámbito en O0/O2
(`39-namespace-arg-exception.log`). Estas pruebas no cubren paquetes,
consumidores precompilados de clases ni todas las formas de composición. Los
cambios no tienen commit y el caso general continúa abierto.

### Operador de clase desde biblioteca NativeSharp

La prueba `tests/integration/operator_increment_owned_binary.py` genera una
biblioteca con operadores de clase y un consumidor separado que verifica
prefijo, posfijo y destrucciones. El productor O0 se generó, pero ambos
consumidores O0/O2 fueron rechazados por la barrera de miembros precompilados
(`39-owned-binary-before.log`, hash `75702b7e217e2833`). Se probará retirar
únicamente esa barrera del caso RAII local exacto; se conservan las exigencias
de tipo, propiedad y parámetro, y se comprobarán símbolo, ABI y destrucción en
ejecución. No se modifica el formato de metadata ni la interoperabilidad C/C++.

El build incremental pasó (`39-owned-binary-build.log`, hash
`fb9ba1c1cfe820bc`). La biblioteca O0 y consumidores O0/O2 compilaron y
ejecutaron 42 con los valores y destrucciones esperados
(`39-owned-binary-after.log`). La prueba permanente se amplió para comprobar
que el posfijo precompilado prestado a un método vive durante la llamada y se
libera después, así como el posfijo descartado; ambos consumidores pasaron
(`39-owned-binary-expanded.log`). También volvieron a pasar las regresiones
de clases desde fuente y de structs desde biblioteca
(`39-owned-binary-source-controls.log`, `39-owned-binary-struct-controls.log`).
No se probaron productor O2, DLL, excepciones binarias ni caché; campos y
arreglos de dueños continúan fuera de esta ruta. Sin commit; caso general
abierto.

La prueba permanente se amplió a productores O0/O2 y consumidores O0/O2: las
cuatro ejecuciones devolvieron 42, y los negativos de préstamo activo y dueño
fijado se rechazaron para ambos productores (`39-owned-binary-matrix.log`).
Una biblioteca separada cuyo operador lanza se probó también en las cuatro
combinaciones: el consumidor capturó la excepción, conservó el dueño anterior
y lo destruyó una vez al terminar el ámbito (`39-binary-owned-exception.log`).
Ambas pruebas usan bibliotecas estáticas NativeSharp y caché desactivada; no
acreditan DLL, caché ni excepciones del destructor. El trabajo sigue sin commit.

### Receptor posfijo y destructor que lanza

La regresión fuente ahora comprueba `(value++).Value`: lee el valor anterior,
destruye ese temporal al terminar la expresión y conserva el dueño actualizado
en O0/O2 (`39-owner-receiver-before.log`). La misma comprobación se incorporó a
la prueba binaria y pasó con productor y consumidor O0/O2
(`39-owner-receiver-binary.log`). El nombre del primer log refleja que es la
primera comprobación de esta forma; no registra un fallo previo.

Una reproducción independiente provocó una excepción implícita durante el
destructor del valor anterior en `++value`, mediante un inicializador estático
que lanza. En O0/O2 el `catch` observó el nuevo dueño con valor 11, una entrada
al destructor anterior y una entrada posterior al destructor del nuevo
(`39-prefix-destructor-exception.log`). La prueba observa las entradas a los
destructores, no demuestra que se libere la reserva del objeto cuyo destructor
se interrumpió. Se usó Core binario aislado; no se probó esta excepción con un
operador publicado en una biblioteca separada.

### Transferencia del resultado posfijo

La regresión fuente se amplió para devolver `value++` desde una función con
retorno `owned` y para pasarlo a un parámetro `owned`. En O0/O2 comprobó los
valores y la destrucción una sola vez de los objetos anterior y actualizado
(`39-owner-transfer-probe.log`). La regresión con operador de biblioteca repitió
ambas formas y pasó las cuatro combinaciones productor/consumidor O0/O2
(`39-owner-transfer-binary.log`).

Un control adicional hizo lanzar el argumento siguiente antes de entrar al
receptor `owned`. En O0/O2 el temporal en tránsito se destruyó durante la
excepción y el dueño actualizado permaneció válido hasta salir del ámbito
(`39-postfix-owned-arg-exception.log`). Esta última prueba utiliza operador
fuente y Core binario aislado; no verifica todas las formas de composición ni
la liberación de memoria mediante un contador del asignador.

### Protección del préstamo prefijo durante una llamada

La regresión negativa `Both(++value, ++value)` fue aceptada por el compilador
(`39-prefix-argument-safety-before.log`); no se ejecutó porque el segundo
prefijo destruye el objeto prestado por el primero antes de entrar a la llamada.
El análisis de argumentos registra los préstamos seleccionados solo después de
analizar todos los argumentos. Se añadirá protección temporal anidada para un
resultado prefijo prestado del dueño local mientras se analizan los argumentos
posteriores, reutilizando `hasActiveBorrowOfOwnerLocal`. La protección terminará
al acabar el análisis de esa lista y se aplicará tanto a métodos como a funciones
de namespace. Así las operaciones que ya comprueban préstamos activos rechazarán
reemplazos y movimientos; se probarán también dueños distintos y llamadas
anidadas. Es un alcance concreto y no afirma proteger aún todas las expresiones
compuestas fuera de llamadas.

El build incremental pasó (`39-prefix-argument-build.log`, hash
`7291c595edeca806`). Los argumentos que intentan reemplazar el mismo dueño
ahora se rechazan para método, función de namespace y llamada anidada;
`move(value)` en un argumento posterior también se rechaza
(`39-prefix-argument-expanded.log`). El control con dos dueños diferentes
ejecutó 42 en O0/O2 y permitió volver a actualizar un dueño después de la
llamada, mostrando que la protección se retira. Pasaron además los controles
binarios y de structs (`39-prefix-argument-binary.log`,
`39-prefix-argument-struct.log`).

La protección se registra para un prefijo directo que es argumento, no para
todas las expresiones que puedan transportar indirectamente ese préstamo ni
para expresiones compuestas fuera de llamadas. Esas formas permanecen por
investigar antes de afirmar seguridad general. El cambio está sin commit.

La comprobación posterior encontró que `Both(value, ++value)` e
`Both(Identity(++value), ++value)` también se aceptaban
(`39-indirect-borrow-before.log`); no se ejecutaron. Se probará proteger
durante los argumentos posteriores cualquier resultado ya analizado que
conserve un dueño local como origen y sea un préstamo o lectura sin
transferencia de propiedad. Se conservarán los controles con dueños distintos y
el fin del préstamo al terminar la llamada. Si una llamada intermedia no
transporta la procedencia, esta medida por sí sola no bastará y la regresión
deberá seguir fallando.

El primer ensayo rechazó el dueño directo, pero la llamada `Identity` no
conservó el origen en su resultado y el segundo caso continuó aceptándose
(`39-indirect-borrow-after.log`). Para ese caso se rastrearán de forma
conservadora los prefijos prestados dentro de un argumento cuyo resultado sea
prestado y carezca de origen explícito; se protegerán sus dueños hasta analizar
el resto de la llamada. Se omiten cuerpos lambda porque no se ejecutan al
evaluar el argumento. La regla puede rechazar una llamada que devuelva un
préstamo independiente del prefijo anidado; esa consecuencia se mantendrá
documentada y se comprobarán los controles válidos.

El build incremental pasó (`39-forwarded-prefix-build.log`; la identidad
consta en `39-forwarded-prefix-after.log`). Ambas reproducciones se rechazan
ahora por reemplazo de un dueño prestado, tanto en métodos como en funciones
de namespace (`39-forwarded-prefix-controls.log`). El control válido con un
préstamo reenviado de un dueño y actualización de otro ejecutó 42 en O0/O2;
pasaron también los controles binarios y de structs
(`39-forwarded-prefix-binary.log`, `39-forwarded-prefix-struct.log`).

La protección del primer argumento dueño o prestado es conservadora porque se
aplica antes de resolver la sobrecarga; podría rechazar usos que una resolución
más precisa demostraría seguros. El recorrido de prefijos anidados solo se
activa para un resultado prestado sin procedencia explícita y no visita cuerpos
lambda. No demuestra seguridad de préstamos transportados por todas las formas
de valor, ni de expresiones fuera de llamadas. El trabajo sigue sin commit.
Un control adicional pasó el mismo dueño dos veces sin reemplazarlo y ejecutó
42 en O0/O2 (`39-forwarded-prefix-read-control.log`).

### Protección del receptor durante los argumentos

La comprobación estática aceptó `(++value).Add(++value)`
(`39-prefix-receiver-before.log`); no se ejecutó porque el argumento puede
destruir el objeto receptor. El receptor se analiza antes de la lista de
argumentos y no se incorporaba a la protección temporal. Se probará conservar
su dueño o préstamo durante esa lista, incluida la forma reenviada a función de
extensión, y retirarlo al terminar. Se comprobarán también receptores y
argumentos con dueños distintos y llamadas válidas con lectura del mismo dueño.

La compilación incremental pasó (`39-prefix-receiver-build.log`, hash
`21f11c399a8b9588`). El análisis rechazó la invalidación del receptor prefijo,
del receptor local directo y del receptor de extensión; los controles válidos
con dos dueños diferentes y con lectura del mismo dueño ejecutaron 42 en O0/O2
(`39-prefix-receiver-expanded.log`). También pasaron los controles binarios y
de structs (`39-prefix-receiver-binary-control.log`,
`39-prefix-receiver-struct-control.log`). La protección del receptor es
conservadora y todavía no certifica todos los tipos de llamada o de procedencia.

Como el análisis de llamadas es compartido, se ejecutó la prueba dirigida
`nsc.sdk.shared-public-pointers`: falló en el caso Rc
`struct_pattern_chained_view_read` por rechazo de la vista temporal
(`39-receiver-pointer-control.log`). Las pruebas Rc anteriores en ese mismo
recorrido pasaron hasta ese punto. El rechazo de esa reproducción ya se había
registrado antes en `prestamo-get-punteros.md`; el log actual no demuestra que
el cambio del receptor lo haya introducido ni que el resto de la prueba pase.
La forma de conservar la vista temporal durante el `match` sigue pendiente de
implementación y validación.

### Diagnósticos específicos para propiedad

La regresión `tests/integration/operator_increment_owned.py` ya no considera
suficiente cualquier fallo de análisis en sus 16 casos negativos: exige el
diagnóstico correspondiente para escape, copia de propietario, reemplazo o
movimiento durante un préstamo y selección de sobrecarga. Con el compilador
incremental SHA-256
`b982e107613cfaf48db7e3e94fa81600c17a9ff66f18fc9471d8e4122b7c080f`,
esos controles y los rechazos de préstamo activo y dueño fijado pasaron. Las
ejecuciones positivas para métodos y funciones de namespace devolvieron 42
en O0/O2 (`A:/nsc-fixes-evidencia/39-owned-specific-diagnostics.cmd` y `.log`).
El cambio solo fortalece la regresión existente; no implementa los demás
destinos, resúmenes de escape ni corrutinas. Los cambios continúan sin commit.

Las reproducciones originales de declaración se compilaron y ejecutaron por
separado en O0 y O2 sin caché con ese mismo compilador: las cuatro ejecuciones
terminaron con 0 (`A:/nsc-fixes-evidencia/39-original-emit-current.cmd` y
`.log`). Antes se comprobó con `fc /b` que cada copia de la campaña coincide
con el archivo correspondiente de `code-examples/chapter-07/39_matrix_mxn_class/
operator-catalog/expected-errors`. El registro contiene sus SHA-256. Esos
ejemplos declaran el operador pero su `Main` no lo invoca; no acreditan por sí
solos prefijo, posfijo o liberación. Esos comportamientos tienen controles
separados y la funcionalidad general del caso sigue pendiente.

### Destino de clase en un campo propietario

Con el compilador incremental SHA-256
`b982e107613cfaf48db7e3e94fa81600c17a9ff66f18fc9471d8e4122b7c080f`,
`++holder.Item` y `holder.Item++` se rechazan como actualización no
implementada. Los controles de propietario local y campo numérico sí pasan el
análisis (`A:/nsc-fixes-evidencia/39-owned-field-current.cmd` y `.log`).

La regresión preparada `tests/integration/operator_increment_owned_field.py`
conserva las expectativas de resultado y destrucción: los dos controles
ejecutan 42 en O0/O2, pero los destinos propietarios de campo fallan al
compilar en ambos niveles (`39-owned-field-regression-before.cmd` y `.log`).
El posfijo produce también un diagnóstico de copia de propietario; no se
considera probado el movimiento del valor anterior. `analyzeOverloadedUnary`
admite actualmente esa ruta de clase solo para un identificador local RAII;
extenderla a un campo requiere comprobar el préstamo del campo y del objeto
raíz, evaluar una vez el receptor y conservar el cleanup en reemplazo,
excepción y descarte. No se retiró la barrera ni se ejecutaron accesos
potencialmente inseguros. La prueba continúa fallando y el caso queda abierto.

La comprobación previa de préstamos encontró un defecto independiente que
precede a habilitar el incremento: `match (holder.Item.Get())` conserva una
referencia a un campo de `Item`, pero el análisis acepta reemplazar
`holder.Item` dentro del patrón (`39-owned-field-pattern-before.log`, caso
`pattern-field-assign`). No se ejecutó ese programa. El mismo patrón con `++`
se rechaza por la barrera de operación no implementada. El registro específico
de patrón en `StatementAnalyzer.cpp` reconoce actualmente campos de valores
move-only, pero no un campo de referencia declarado `owned`.

La corrección de seguridad se limitará primero al sujeto sintético de patrón
de una llamada `ref[this]` sobre un campo `owned` de referencia directamente
accedido desde un local RAII. Debe reutilizar la protección existente de campo,
rechazar reemplazo y movimiento mientras vive el préstamo y permitir lectura,
otro campo y reemplazo después del patrón. No habilita todavía `++` sobre ese
campo. Los casos de receptor calculado, anidado o alias requieren evidencia
separada; no se modifica interoperabilidad C/C++ ni la instalación del SDK.

La protección experimental se conectó en `StatementAnalyzer.cpp` para un
campo de referencia declarado `owned`, accedido directamente desde un local
RAII, cuando el sujeto sintético de `match` recibe una referencia ligada a
`this`. El primer ensayo no invocaba el análisis para esa forma y la
regresión seguía aceptando el reemplazo
(`39-owned-field-pattern-regression-after.log`). Tras conectar la condición,
el build incremental pasó (`39-owned-field-pattern-dispatch-build.log`), con
compilador SHA-256
`5c7c51f03daf9ec53a810f74c022178b332d3f17cab76539cca70f578ec7d0f4`.

La regresión dirigida rechazó reemplazo, movimiento, reemplazo por alias,
llamada potencialmente mutante y reemplazo del objeto raíz mientras vive la
referencia; lectura, cambio de otro campo y reemplazo tras el patrón ejecutaron
42 en O0/O2 (`39-owned-field-pattern-expanded.log`). También pasaron el
control de operadores propietarios y la selección de campos de Rc/Arc
(`39-owned-field-pattern-controls.cmd` y sus dos logs). La evidencia usa el
árbol de trabajo conjunto y no cubre receptores calculados o campos anidados.
La corrección sigue sin commit y no habilita el incremento de campo; la
regresión `operator_increment_owned_field.py` permanece pendiente.

La comprobación de operador sobre campo bajo préstamo se añadió antes de la
selección de la sobrecarga en `IdentifierAnalyzer.cpp`: consulta la misma
procedencia usada al reemplazar un campo `owned` y produce el diagnóstico de
préstamo activo. Antes, el caso se rechazaba solo porque la operación completa
no estaba implementada (`39-owned-field-increment-borrow-before.log`). Tras
compilar incrementalmente (`39-owned-field-increment-borrow-build.log`), la
regresión confirmó el diagnóstico específico, las demás protecciones de
campo y las lecturas válidas; el control de operadores locales también pasó
(`39-owned-field-increment-borrow-after.cmd` y sus logs). El compilador tiene
SHA-256 `7c3a663a938cdc3d88c9849cb456028c25e2fe037dca5f89766994076153db7f`.

Esta comprobación no habilita el caso positivo: falta transmitir al descenso
que el destino es un campo `owned`, conservar la procedencia del resultado
prefijo y transferir o destruir el valor anterior en posfijo. La regresión de
campo positivo permanece fallida y no se declara completada la operación.

El control adicional `Both(holder.Item, holder.Item++)` plantea un préstamo del
campo en el primer argumento y su transferencia en el segundo. Antes de
habilitar el posfijo debe rechazarse la invalidación durante la evaluación de
la llamada. Actualmente se rechaza únicamente por la barrera general de
operación no implementada; la regresión preparada exige el diagnóstico de
préstamo y falla (`A:/nsc-fixes-evidencia/39-owned-field-argument-before.cmd`
y `.log`). No se ejecutó el programa. El seguimiento temporal existente de
argumentos usa `ownershipSourceLocalOrdinal`, pero un acceso ordinario al
campo no conserva ese origen salvo que venga de un receptor de solo lectura.
Es necesario rastrear el campo concreto y su raíz durante la llamada, incluida
la forma de función de namespace, antes de retirar la barrera; retener solo el
objeto raíz de manera global podría rechazar otros campos independientes.

Para la siguiente comprobación se conservará durante la evaluación de una
llamada el par formado por local RAII directo y ordinal del campo `owned` de
referencia usado como argumento. El operador sobre ese mismo campo debe
rechazarse antes de la barrera actual; el registro se limita a la llamada y se
restaura para llamadas anidadas. Se probarán llamadas de método y de namespace,
otra instancia y otro campo, sin ejecutar el caso inseguro. Esta etapa no
habilita todavía la operación ni pretende cubrir receptores calculados,
alias o referencias de retorno; esas formas se conservan pendientes. Se
preservan las prohibiciones de la campaña, especialmente interoperabilidad
C/C++, builds ajenos, instalación funcional y batería completa.

El ensayo registra ese par al analizar argumentos directos, para métodos y
funciones de namespace, y restaura la lista al terminar cada llamada. El build
incremental terminó correctamente (`39-field-argument-guard-build.log`); el
compilador aislado tiene SHA-256
`5bd58775478f1df06d1cc92bb5999d824763b3cfc3ae1e3ac51b5cb96e6795f2`.
Ambas reproducciones `Both(holder.Item, holder.Item++)` reciben el diagnóstico
específico de préstamo activo (`39-field-argument-regression.log`); no se
ejecutaron. Las regresiones de patrones y de operadores de dueño local pasaron
(`39-field-argument-pattern-control.log` y `39-field-argument-local-control.log`).

La regresión de campo completo continúa fallando: las cuatro combinaciones
positivas de prefijo y posfijo siguen tras la barrera de implementación. Un
control adicional de solo lectura, `Both(holder.Item, holder.Item)`, también
se rechazó en O0/O2 porque el argumento no conserva una fuente propietaria
comprobada para el parámetro `borrowed`; no se ha comparado ese control con el
binario anterior y no se atribuye aún el rechazo al ensayo. La causa inmediata
del diagnóstico es que el resultado del acceso carece de marca de ownership
reconocida por la validación del parámetro en `CallAnalyzer.cpp`. Es necesario
investigar la procedencia y el ciclo de vida antes de aceptar ese caso. Tampoco
se han verificado receptores alias o calculados, campos anidados ni referencias
reenviadas. El operador de campo continúa bloqueado y no hay commit de esta
etapa.

Antes de admitir argumentos de campo se agregó el control de pasar a una misma
llamada `holder.Item` y `holder`, en ambos órdenes, para métodos y funciones de
namespace. El receptor puede reemplazar el campo; actualmente los cuatro casos
se detienen por la barrera de fuente no comprobada y no reciben el diagnóstico
de llamada potencialmente mutante (`container-argument-before.log`). No se
ejecutaron. Se comprobará, antes de retirar los registros temporales de la
lista, si un argumento identificador es la misma raíz RAII registrada por un
campo directo de esa lista. Se rechazarán ambos órdenes conservadoramente,
incluso si el callee no muta; una raíz distinta debe conservarse. El ensayo no
admite el préstamo del campo ni cubre alias, receptores de extensión o
argumentos transformados. Se mantienen las restricciones de la campaña y la
verificación dirigida con compilación incremental.

El build incremental pasó (`container-argument-build.log`), compilador SHA-256
`c58d931deb75206c0430e88102bd72859b7aedec935b698c4c2c12641736ac7b`.
Los cuatro órdenes y tipos de llamada reciben el diagnóstico de posible
mutación (`container-argument-after.log`). Las pruebas de patrones, operadores
locales y retorno prestado pasaron (`container-pattern.log`,
`container-local.log`, `container-return.log`). El control con `other` no
recibió el diagnóstico de contenedor prestado, pero la llamada sigue detenida
por la fuente del primer argumento (`regression.log`): no demuestra ejecución.
La prueba global continúa fallida en las ocho combinaciones positivas y el
retorno prestado del campo. La protección no cubre alias o receptor implícito
y aún no se habilita la admisión del campo; sin commit.

Se añadieron casos donde un alias prestado del contenedor se pasa junto con su
campo y donde el contenedor o su alias es receptor del método que recibe el
campo. Los cuatro se rechazan actualmente por la fuente del campo, sin el
diagnóstico de invalidación (`container-alias-before.log`); no se ejecutaron.
Se consultará la resolución de raíces y alias de `hasActiveViewOfStorage` al
finalizar el análisis de argumentos, mientras el campo sigue registrado, y se
revisará también el receptor de instancia o extensión. La regla es
conservadora ante llamadas cuya implementación no se sabe inmutable. No se
habilita aún la conversión del campo ni se afirman protegidas expresiones
calculadas o todos los alias; se preservarán los controles independientes.

El build incremental pasó (`container-alias-build.log`), compilador SHA-256
`55d8ecbfbe99910d697fe09b0a0cad12eb50e40f3b974734a090db8e81d966ee`. Los
cuatro controles de alias y receptor reciben el diagnóstico específico, igual
que las variantes previas; los controles dirigidos de patrón, operador local y
retorno prestado pasaron (`container-alias-after.log`, `container-alias-pattern.log`,
`container-alias-local.log`, `container-alias-return.log`). Un alias de otro
contenedor no recibe ese diagnóstico, aunque la fuente de campo sigue rechazada
(`regression.log`); no se demuestra su ejecución. Permanecen los ocho fallos
positivos y el retorno de campo. La ruta de extensión se conectó a la misma
comprobación, pero aún carece de prueba propia; no se afirma cubierta. Ningún
caso inseguro se ejecutó y el ensayo sigue sin commit.

Se probaron también una extensión con el mismo receptor y con un alias, y un
control con otro receptor (`regression.log` tras ampliar la prueba). Las dos
primeras se rechazan como llamadas potencialmente mutantes; el receptor
independiente no recibe ese diagnóstico y continúa rechazado por el argumento
de campo. Los casos inseguros solo se analizan. La extensión informa además un
segundo diagnóstico para el mismo receptor porque `argumentSyntaxes` incluye el
receptor reenviado y se vuelve a comprobar explícitamente. Se excluirá ese
receptor del recorrido de argumentos para conservar únicamente su diagnóstico
específico; no se cambiará la protección ni la regla de campos independientes.

El build incremental pasó (`extension-diagnostic-build.log`), compilador
SHA-256 `1c20cc15a63b76ae312c458d918fadb7da842c41f36ba84420636771e586e5fa`.
La regresión verifica que la extensión con receptor directo o alias produzca
exactamente un diagnóstico de llamada mutante, y que el receptor independiente
no reciba ese diagnóstico (`regression.log`); este último sigue rechazado por
el argumento no comprobado. Los controles de operador local y retorno prestado
pasaron (`extension-diagnostic-local.log`, `extension-diagnostic-return.log`).
La regresión general de campo continúa fallida en las ocho combinaciones
positivas y el escape del campo; no se ejecutaron los casos inseguros y el
cambio sigue sin commit.

### Ensayo de argumento prestado desde campo directo

La reproducción `Both(holder.Item, holder.Item)` se rechaza porque el acceso
al campo no comunica ownership, aunque `Item` es `owned` y `holder` es un
local RAII comprobado. Se ensayará marcar como préstamo la expresión directa
solo dentro del análisis de argumentos de método y namespace, después de
registrar el par de campo y dueño. La expresión debe conservar el ordinal del
dueño en su binding; la validación de retorno deberá detectar también esa
procedencia en un argumento miembro, incluso a través de llamadas anidadas.
Se mantienen las protecciones de movimiento, escritura, contenedor y receptor,
así como el rechazo de argumentos sin contrato prestado. El ensayo no habilita
`++`/`--` sobre campos, alias como raíz, indexadores ni receptores calculados.
Las pruebas deben ejecutar lecturas válidas O0/O2, rechazar escapes y mutaciones
y mantener los controles de propiedad existentes. Si aparece una vía de escape
no cubierta se conserva el resultado y se evita presentar la etapa como cierre.

El build incremental terminó con código 0 (`field-borrow-admission-build.log`),
con compilador SHA-256
`fe6f37602db5a0d295edda5bf5920de24b4250ac877dd51b24b183fe1882572e`.
La lectura doble y la lectura con otro receptor ejecutaron 42 en O0/O2; cinco
controles de dueño, campo, contenedor, alias y extensión independientes también
ejecutaron sus resultados esperados en ambos niveles. Se rechazaron el retorno
directo y anidado, el almacenamiento global mediante llamada, el parámetro sin
contrato prestado y las invalidaciones cubiertas (`regression.log`). Una
asignación independiente usada como argumento sigue rechazada por el segundo
argumento no comprobado. Los controles dirigidos de patrón, operador local y
retorno pasaron (`field-borrow-admission-pattern.log`,
`field-borrow-admission-local.log`, `field-borrow-admission-return.log`).

La regresión completa del caso 39 todavía termina con código 1: las cuatro
combinaciones de prefijo y posfijo sobre campo permanecen rechazadas por la
barrera de implementación. El ensayo no acredita alias de raíz, argumentos
calculados, todas las formas de escape ni consumo binario de esta operación;
los casos inseguros se limitaron a análisis. El cambio sigue sin commit.

Se comprobó el almacenamiento de `Identity(holder.Item)` en un local
`borrowed`: las dos variantes que después reemplazarían el campo se rechazan
al declarar el local, tanto con método como con función de namespace
(`field-borrow-local-negative.log`), por falta de fuente identificadora
directa. No se ejecutaron. El control válido que solo lee mientras `holder`
vive también se rechaza en O0/O2 (`regression.log`), por lo que esta barrera
impide actualmente ese uso seguro. El fixture conserva esa expectativa y la
regresión global registra seis fallos: dos del local y cuatro de operadores.
Para admitir el local será necesario registrar de forma conservadora la raíz y
el campo de la llamada, preservar su vida en el ámbito del local y rechazar el
reemplazo hasta que termine; el simple hecho de que la llamada devuelva
`borrowed` no prueba una vida independiente. No se ha habilitado aún esa ruta.

### Comparación del préstamo directo de campo

Una reproducción mínima sin declaraciones de operadores mantiene intacta la
expresión `Both(holder.Item, holder.Item)`. Se analizó con el compilador
aislado anterior `A:/nsc-fixes-evidencia/integration-incremental-update/bin/nsc.exe`
(build hash `86e163439c236e09`) y con el compilador actual (hash
`2d0bfe18c8f0774c`). Ambos la rechazaron con el mismo diagnóstico de fuente no
comprobada; comandos y resultados en
`A:/nsc-fixes-evidencia/39-field-borrow-baseline/`. Esto confirma que ese
rechazo precede al último ensayo, no que la rama base original tenga idéntico
comportamiento.

El acceso a campo establece `assignmentMemberOrdinal`, pero solo propaga un
origen de ownership cuando el receptor ya es una vista readonly. Además, el
registro temporal `activeArgumentOwnedFields_` introducido en el ensayo se
consulta por ahora exclusivamente en el análisis de `++`/`--`; la asignación,
el movimiento y las llamadas que pudieran modificar el campo siguen usando el
registro de vistas existente. Por ello, marcar el campo como prestado para
permitir la llamada sin ampliar y verificar esa protección podría aceptar una
invalidación durante la evaluación de otro argumento. No se ha habilitado esa
conversión. La siguiente investigación debe cubrir esas rutas y la procedencia
del resultado de llamada antes de cambiar el contrato; la operación de campo
permanece bloqueada.

Se analizaron además tres proyectos mínimos con el compilador actual: lectura
del campo seguida por `0`, lectura seguida por `holder.Reset()` —que reemplaza
el campo— y lectura seguida por `other.Reset()` sobre otra instancia. Los tres
reciben únicamente el diagnóstico de fuente no comprobada del primer argumento
(`A:/nsc-fixes-evidencia/39-field-borrow-baseline/invalidations.log`). El
segundo programa no se ejecutó. Como la llamada exterior no es válida, esto no
demuestra por sí solo que se aceptaría el programa inseguro si se habilitara el
préstamo; confirma que la regresión necesita distinguir una mutación del mismo
receptor y una del receptor independiente antes de habilitarlo. Los proyectos
y el ejecutor de análisis están en la misma carpeta de evidencia.

La regresión permanente `operator_increment_owned_field.py` incluye ahora una
lectura con `other.Reset()` que debe ejecutar 42 y una llamada a `holder.Reset()`
que debe ser rechazada por el diagnóstico específico de mutación durante el
préstamo. La ejecución dirigida `39-field-borrow-baseline/regression.log`
terminó con código 1: ambos niveles del control válido siguen rechazados, al
igual que los dos controles de lectura anterior y las cuatro combinaciones de
incremento/decremento; la llamada potencialmente mutante no produce aún el
diagnóstico exigido. Los negativos de incremento sobre el mismo campo sí lo
produjeron. La prueba insegura solo se analiza; no se ejecuta. Esta ampliación
registra las expectativas pendientes y no modifica la semántica del compilador.

Para comprobar la protección de la llamada posterior se usará el registro de
campos ya activos durante argumentos en `hasActiveViewOfStorage`, únicamente
cuando su consumidor pregunta por cualquier campo del receptor (`anyField`) y
el receptor es un identificador local rastreado. Se compararán las raíces ya
calculadas por esa función con el local propietario del campo; esto conserva la
separación entre instancias independientes y permite al control existente de
llamadas potencialmente mutantes dar su diagnóstico. No se habilita el préstamo
directo ni se afirma cobertura de movimientos, asignaciones, receptores
calculados o retornos. Se ejecutarán la regresión de campo y los controles de
patrones y operadores locales con build incremental. Se mantienen las demás
restricciones de la campaña, sin interoperabilidad ni batería completa.

El build incremental terminó con código 0 (`mutation-build.log`), compilador
SHA-256 `210f185274a1c80907964e7c5966d5437fd6fee798637212e7483fc5c79e8b2f`.
Las regresiones de patrones y de operadores locales pasaron
(`mutation-pattern.log`, `mutation-local.log`). La prueba de campo sigue con
código 1, pero el caso con `holder.Reset()` ahora recibe el diagnóstico de
llamada potencialmente mutante y ya no figura entre sus fallos. El control de
otra instancia sigue rechazado por la falta de fuente prestada comprobada, sin
diagnóstico de mutación; por ello aún no demuestra ejecución válida. Siguen
fallando las dos lecturas directas en O0/O2 y las cuatro combinaciones de
prefijo/posfijo de campo (`mutation-regression.log`). No se ejecutó el caso
inseguro ni se habilitó la conversión del campo. El cambio permanece sin commit.

La regresión añadió `Both(holder.Item, move(holder))` en métodos y funciones de
namespace. Ambas comprobaciones se rechazan actualmente por el primer argumento
no reconocido, pero no emiten el diagnóstico de movimiento del dueño prestado
(`move-root-before.log`); no se ejecutaron. El registro de campo ya conserva el
ordinal del dueño local mientras se analizan argumentos posteriores, y el
movimiento de una raíz completa consulta `hasActiveBorrowOfOwnerLocal` sin
proyección. Se comprobará ese registro únicamente para la raíz completa en ese
punto; el movimiento parcial de campos continuará sujeto a su análisis propio.
La medida conserva el rechazo actual del argumento de campo y no demuestra aún
la admisión segura del préstamo ni el comportamiento de alias o campos anidados.

El build incremental pasó (`move-root-build.log`), con compilador SHA-256
`5aa89d7668934173c39d155c621807bd52f702292bda7f109b60694ead1e4f20`.
Los dos movimientos del mismo dueño se rechazaron con el diagnóstico específico;
los controles de patrones y de operadores locales pasaron
(`move-root-after.log`, `move-root-pattern.log`, `move-root-local.log`). Se
añadió un control que intenta mover `other` y no recibió diagnóstico de
movimiento prestado (`regression.log`), aunque la llamada sigue rechazada por
el primer argumento y por tanto ese control no demuestra ejecución válida.
La regresión de campo mantiene ocho fallos de compilación esperada: dos casos
de lectura y dos de operadores en O0/O2. Movimiento parcial, alias, receptores
calculados y procedencia del resultado siguen pendientes; sin commit.

`Both(holder.Item, move(holder.Item))` se rechaza por el primer argumento no
reconocido, sin diagnóstico de movimiento parcial bajo préstamo
(`move-field-before.log`); no se ejecutó. La expresión del segundo campo ya se
vincula antes de `rejectInvalidPartialMove`, y su comprobación de vista llama
`hasActiveViewOfStorage`. Se comprobará allí el par de raíz y ordinal de campo
activo para miembros directos, reutilizando las raíces de alias ya calculadas y
el binding del campo. Esta comprobación también alcanza la asignación de ese
campo, que usa la misma consulta, pero aún debe probarse por separado. No se
habilita el préstamo directo ni se afirma cobertura de proyecciones anidadas.

El build incremental pasó (`move-field-build.log`), compilador SHA-256
`0b69c37022ef59aad3f58134fc880e3a1b4777640910e844e65afaa09fc64f28`.
El caso de movimiento del mismo campo recibió el diagnóstico específico y los
controles de patrones y operadores locales pasaron (`move-field-after.log`,
`move-field-pattern.log`, `move-field-local.log`). El control de movimiento de
un campo distinto no recibió ese diagnóstico (`regression.log`), aunque sigue
rechazado por el primer argumento y no demuestra ejecución válida. Las ocho
combinaciones positivas de la regresión siguen sin compilar. Asignación,
campos anidados y préstamo directo aún requieren validación; sin commit.

Se incorporaron comprobaciones de asignación como argumento posterior. El
análisis de `Both(holder.Item, holder.Item = new Box(30))` produjo el diagnóstico
de reemplazo del campo prestado; el control que asigna `holder.Other` recibió
el diagnóstico previo de fuente no comprobada, sin diagnóstico de reemplazo
del campo prestado. Los controles negativos de dueño y campo independientes
exigen también ese diagnóstico previo, para que un error de sintaxis no pase
como resultado válido. Evidencia `field-assignment-controls.log`. La regresión
global permanece fallida en las ocho combinaciones positivas ya documentadas;
ninguno de estos análisis demuestra ejecución de los controles independientes.
No se compiló de nuevo el compilador porque solo cambió la prueba.

### Retorno prestado reenviado por llamada

Antes de permitir `Identity(holder.Item)`, se comprobó un caso independiente
con un dueño local ya admitido: `return Identity(owner)` dentro de una función
que devuelve `borrowed`. El análisis aceptó el método y la función de namespace
aunque el dueño local termina al salir; ambos controles que reenvían un parámetro
prestado también fueron aceptados. La regresión
`tests/integration/borrowed_call_owner_return.py` exige rechazar los primeros y
conservar los segundos, y falla en las dos reproducciones
(`borrowed-call-return-before.log`). No se ejecutaron los programas. El caso
equivalente con campo sigue rechazado antes por la fuente del argumento y no
llega a verificar la vida del resultado (`field-return-before.log`).

El análisis de llamada asigna la calificación `borrowed` del resultado, pero
no registra como fuente el dueño local pasado al parámetro; la validación de
retorno solo rechaza el dueño si `ownershipSourceLocalOrdinal` llega informado.
Una solución deberá tratar de forma conservadora todos los argumentos que
podrían prestar el resultado, no elegir arbitrariamente uno cuando hay varias
fuentes, y cubrir métodos, funciones de namespace y llamadas anidadas antes de
permitir el préstamo de campos. El defecto y las pruebas permanecen abiertos;
no se ha cambiado esa semántica en esta etapa.

Para impedir primero el escape demostrado se comprobarán en
`analyzeBorrowedReturn` los argumentos identificadores de un resultado
`borrowed` de llamada, incluidos argumentos de llamadas anidadas. Si uno tiene
un dueño local RAII registrado y no transfiere la propiedad, el retorno se
rechazará de forma conservadora: el resultado podría provenir de ese argumento
aunque el callee devuelva otro. Las llamadas que reciben solamente parámetros
prestados se conservan. Se excluyen cuerpos lambda y no se infiere procedencia
para receptores, expresiones de campo o agregados; no se considera solución
general de vidas ni habilitación del préstamo directo de campo. La regresión
previa y los controles dirigidos de llamadas y préstamos deben contrastarse
después de una compilación incremental, sin ejecutar ejemplos inseguros.

La compilación incremental pasó (`borrow-return-build.log`), compilador
SHA-256 `524f8d34291b49713b31f970137fe18a51c89517e71aec91893959ba0fafb5d3`.
La regresión rechazó con diagnóstico de dueño local los cuatro escapes directos
y anidados de método y namespace, y aceptó los dos controles de parámetro
prestado (`borrow-return-after.log`). Pasaron también los controles dirigidos
de patrones y operadores locales (`borrow-return-pattern.log`,
`borrow-return-local.log`). No se ejecutaron las reproducciones inseguras.
La regla puede rechazar una llamada cuyo resultado sea independiente de un
argumento local; no cubre todos los alias, receptores, agregados ni procedencia
de variables intermedias. El cambio es parcial y permanece sin commit.

La regresión ampliada confirmó que un alias prestado de dueño local ya se
rechaza con el diagnóstico correcto, pero `return Identity(owner)` con un
parámetro `owned` se acepta tanto para método como para función de namespace
(`borrow-return-expanded-before.log`). El parámetro se destruye al salir de la
función; no se ejecutaron las reproducciones. La misma información semántica
del argumento conserva el ordinal del parámetro. Se extenderá la comprobación
anterior solo a parámetros declarados `owned`, conservando el reenvío de
parámetros `borrowed` y omitiendo las transferencias explícitas. El criterio
conservador y las limitaciones de expresiones no identificadoras permanecen.

El build incremental pasó (`borrow-return-param-build.log`), con compilador
SHA-256 `dd8ef173cd74345f8d888052033ec37b7d66dde9ced36fd2cb332d70d0b3785c`.
La regresión ampliada rechaza ambos escapes por parámetros `owned`, mantiene
el rechazo del alias local y acepta los dos reenvíos de parámetros `borrowed`
(`borrow-return-param-after.log`, sin fallos). También pasaron los controles
dirigidos de patrones y operadores locales (`borrow-return-param-pattern.log`,
`borrow-return-param-local.log`). Los programas inseguros no se ejecutaron;
las demás formas de procedencia y el caso de campo siguen abiertos y sin commit.

La regresión de retornos prestados no estaba registrada en CTest. Se añadirá
una entrada dirigida junto a las pruebas existentes de préstamos que use
`NSC_TEST_COMPILER` y no requiera SDK instalado. La prueba binaria de patrones
mantiene una dependencia explícita de un Core externo indicado por argumento;
no se incorporará a CTest con una ruta fija del equipo ni se fingirá que el
SDK preexistente corresponde al compilador actual. Se comprobará el registro
y ejecución de la prueba de retorno mediante la configuración aislada en A.

El registro `nsc.compiler.borrowed-call-owner-return` se añadió a CMake. La
configuración y el build incremental terminaron correctamente
(`borrowed-call-registered-build.log`); CTest ejecutó esa única prueba con
`A:/nsc-fixes-evidencia/build/bin/nsc.exe` y terminó sin fallos
(`borrowed-call-registered-test.log`). El compilador de esa ejecución tiene
SHA-256 `5e3c4b336c5fb6cf7108f4ca3fb0a277a2f06cd315ea002422af36cb01c2f97c`.
Las variantes condicionales siguen rechazándose antes de la protección de
retorno, como detalla su log; el resultado no acredita seguimiento completo de
procedencia ni cierre de la campaña. No se ejecutó la batería completa.

Las variantes `Identity(choose ? owner : other)` no alcanzan hoy el retorno:
el método rechaza la fuente condicional como no comprobada y la función de
namespace no encuentra una sobrecarga viable
(`borrow-return-conditional-before.log`). La regresión permanente acepta
esos diagnósticos o el diagnóstico de escape si se habilita la procedencia, y
pasó (`borrow-return-conditional-controls.log`). Esta comprobación no demuestra
que un condicional se pueda prestar ni que la nueva regla lo rastree.

La reproducción propuesta `owner.Self()` con `Self` devolviendo `this` se
rechaza dentro de `Self`: el retorno de `this` no satisface actualmente el
contrato prestado (`receiver-return.log`). No se ejecutó y no demuestra un
escape vigente del receptor. Debe investigarse una forma válida de devolver
una referencia dependiente del receptor antes de cambiar ese camino.

### Local prestado de un campo reenviado

`borrowed Box view = Identity(holder.Item)` se rechaza incluso si el dueño
`holder` sigue vivo; los controles negativos registran que tampoco se reconoce
una fuente para proteger el campo durante la vida del local. Se investigará
únicamente una llamada resuelta sin receptor, con un argumento que sea campo
owned directo de un local RAII: el préstamo conservador vinculará el resultado
al dueño y al campo. Se mantendrán las barreras para receptores calculados,
llamadas anidadas, varios argumentos y campos no comprobados. Los símbolos
candidatos son `analyzeBorrowedVariableInitializer` y
`configureVariableLocalView` en `StatementAnalyzer.cpp`; la prueba dirigida es
`operator_increment_owned_field.py`, con controles de lectura, fin de ámbito,
reemplazo, movimiento y retorno. No se cambian los ejemplos originales ni se
trabaja en interoperabilidad. El ensayo debe conservar las demás restricciones
de la campaña y se comprobará mediante build incremental y pruebas dirigidas.

El ensayo quedó sin commit en `StatementAnalyzer.cpp`: se registra el dueño
RAII del argumento y se conserva la proyección del campo en el local prestado.
El build incremental terminó con código 0 (`field-local-build.log`), usando
el compilador `A:/nsc-fixes-evidencia/build/bin/nsc.exe`, SHA-256
`390b0c93cbc90517233b91ef0702a4462a887eeef938d580727cebb7f1ccb929`.
En `field-local-expanded.log`, las lecturas por método y función de namespace
y el reemplazo después del fin del ámbito ejecutaron 42 en O0/O2. El análisis
rechazó reemplazo directo y mediante alias, movimiento de campo y dueño,
llamada mutante y retorno del local prestado. Los casos inseguros no se
ejecutaron. Pasaron los controles `field-local-pattern.log`,
`field-local-owner.log` y `field-local-return.log` con el mismo compilador.

La regresión completa dirigida conserva salida 1: las cuatro combinaciones
de prefijo y posfijo sobre campo owned siguen rechazadas, por lo que el caso 39
no está cerrado. El ensayo no prueba llamadas con varios argumentos, receptores
de instancia, proyecciones anidadas ni procedencia arbitraria, y puede limitar
conservadoramente un retorno independiente de su argumento. Tampoco demuestra
el contrato general de las cuatro familias de punteros o del SDK binario.

Un control adicional pasa `holder` a un método que puede reemplazar `Item`
mientras el local `view` sigue vivo. El análisis rechazó la llamada con el
diagnóstico de campo prestado (`field-local-pass-control.log`); no se ejecutó
el programa inseguro. El resultado de la regresión mantiene los mismos cuatro
fallos conocidos de incremento/decremento.

La ruta actual para el operador de clase identifica un destino local RAII en
`IdentifierAnalyzer.cpp`; `finishStorageUnaryFrame` propaga su ordinal en el
nodo `CompoundStore`, y `lowerExpressionFrame035CompoundStore` destruye el
valor anterior de prefijo solo cuando ese ordinal existe. Para un campo, usar
el ordinal del contenedor como si fuera el local del valor mezclaria dos
identidades. Antes de habilitar la operación debe representarse explícitamente
la propiedad del destino, conservando la dirección calculada una sola vez y
la limpieza del posfijo descartado o transferido. La inspección todavía no
demuestra el comportamiento ante excepciones ni justifica retirar la barrera.

Otro control intentó reasignar el local prestado a `Identity(second.Item)` y
después reemplazar ese campo. La reasignación se rechaza antes de llegar al
reemplazo con `borrowed reassignment requires a named RAII owner...`; no se
ejecutó (`field-local-reassign-confirm.log`). La prueba conserva ese diagnóstico
actual: no acredita que una reasignación de campo sea compatible ni que su
procedencia se rastree. Las únicas fallas restantes de esta ejecución son las
cuatro variantes del operador ya indicadas.

### Siguiente ensayo: posfijo sobre campo directo

La ruta del posfijo devuelve el valor anterior como temporal propietario y no
lo destruye durante la sustitución; el cleanup existente debe destruirlo si se
descarta o transferirlo si inicializa otro dueño. Se ensayará únicamente el
campo `owned` directo de un local RAII con operación de clase de un parámetro
por valor no propietario y retorno `owned` del mismo tipo. El rechazo previo de
vistas activas permanece. El posfijo reutiliza `CompoundStore` sin atribuir al
campo un ordinal de local; el marcador de propiedad será necesario para que
el prefijo destruya el valor reemplazado. El prefijo y los
demás destinos conservan la barrera hasta acreditar su procedencia y limpieza.
Archivo candidato para el posfijo: `IdentifierAnalyzer.cpp`;
la prueba existente debe conservar sus negativos, y se agregará una comprobación
del resultado posfijo descartado. Se ejecutarán build incremental y pruebas
dirigidas en A, sin cambiar interoperabilidad ni los ejemplos originales.

El ensayo semántico se aplicó en `IdentifierAnalyzer.cpp`; conserva las
comprobaciones previas de vista activa y reconoce el posfijo en el campo
directo indicado. La ruta HIR/MIR/LLVM existente conserva la dirección y el
temporal anterior. El build incremental terminó con código 0
(`field-postfix-build.log`), con compilador SHA-256
`45003d8071990f7c4ff394935618e2d948661bcdd4d66bdf5ba770d9d698e5e8`.

En `field-postfix-borrow-test.log`, la transferencia a un dueño, el descarte y
el préstamo a un parámetro ejecutaron 42 en O0/O2 y verificaron los recuentos
de destrucción. La prueba mantiene los rechazos de invalidación durante un
préstamo y únicamente falla en prefijo O0/O2. Los controles de patrones,
operadores de local y retorno prestado pasaron (`field-postfix-pattern.log`,
`field-postfix-owner.log`, `field-postfix-return.log`). No se han comprobado
excepciones, receptores calculados, bibliotecas binarias ni todas las formas de
composición; el posfijo sigue siendo un ensayo sin commit y no cierra el caso.

### Preparación del prefijo y procedencia

La prueba dirigida añadió lectura de `++holder.Item` como argumento y cinco
intentos de invalidación o escape: llamada mutante en método y namespace,
reemplazo del campo en argumento posterior, retorno y local prestado seguido
de reemplazo. Con el compilador del ensayo posfijo, los dos controles válidos
fallan en O0/O2 y los cinco negativos se rechazan por la barrera general, sin
los diagnósticos de préstamo exigidos (`field-prefix-safety-before.log`). No se
ejecutaron los negativos. La barrera debe mantenerse hasta propagar el origen
del campo tanto a locales como a argumentos directos y reenviados; registrar
solo el dueño no protege contra sustituir uno de sus campos. El transporte
del indicador de almacenamiento propietario de HIR a MIR se limitará al nodo
del operador; sus consumidores se contrastarán antes de habilitar el prefijo.

`finishStorageUnaryFrame` ya copia el indicador semántico de destino propietario
a `CompoundStore` y MIR propaga su propiedad existente; la ruta LLVM usa ese
indicador para destruir el valor anterior en prefijo sin atribuir el ordinal
del contenedor al campo. El análisis todavía no marca ni admite el prefijo de
campo, por lo que esta ruta nueva no se ha ejercitado para ese caso y no debe
considerarse validada funcionalmente. Build incremental correcto
(`field-prefix-marker-build.log`), compilador SHA-256
`fa91ea0a33908c6d6d4ef2032bcccc3280671bb4d9fec6555f58cfa50b1d33d0`.
Pasaron los controles dirigidos `field-prefix-marker-pattern.log`,
`field-prefix-marker-owner.log` y `field-prefix-marker-return.log`. La prueba
de campo conserva los nueve fallos previstos del prefijo
(`field-prefix-marker-test.log`) y el posfijo probado continúa pasando. Faltan
procedencia y validación real del indicador; los cambios siguen sin commit.

El siguiente ensayo conecta `IdentifierAnalyzer.cpp` con ese indicador y
vincula el resultado prefijo al local RAII; `StatementAnalyzer.cpp` conserva
el campo cuando se declara un local prestado y `CallAnalyzer.cpp` retiene los
campos de prefijos directos o anidados durante llamadas. Se mantiene la forma
directa del receptor y los controles previos. La compilación y los negativos
de escape deben comprobarse antes de considerar segura esta etapa; se agregan
controles de reenvío anidado y no se ejecutarán ejemplos inseguros.

El build incremental pasó (`field-prefix-connect-build.log`), con compilador
SHA-256 `b3fddb27961502cebe9f6df0e822f7929cfdaf313d7243198e43b244d962fbb2`.
La primera ejecución detectó que la expectativa del reemplazo directo pedía
un texto distinto del diagnóstico correcto emitido; se ajustó a ese diagnóstico
específico y la regresión dirigida pasó (`field-prefix-connect-confirm.log`).
Los tres controles positivos de prefijo ejecutaron 42 en O0/O2 y comprobaron
destrucción; los negativos directos, anidados, de retorno y local se analizaron
sin ejecutarse. También pasaron `field-prefix-connect-pattern.log`,
`field-prefix-connect-owner.log` y `field-prefix-connect-return.log`.

Esto acredita solo los destinos directos probados: faltan excepción, binarios,
receptores calculados y otras composiciones. El caso completo permanece abierto
y el cambio sigue sin commit. No se ejecutó batería completa ni se modificó la
interoperabilidad.

Al comprobar `return Identity(++holder.Item)` y su anidación se detectó una
regresión de seguridad: ambas se aceptan aunque termina el dueño RAII. Los
programas no se ejecutaron (`field-prefix-forward-return-before.log`). El
análisis conservador de retorno reenviado ya revisa argumentos identificador
y campo, pero omite el resultado de una expresión prefija, donde sí está
registrado `ownershipSourceLocalOrdinal`. Se ampliará esa revisión únicamente
a `++`/`--` prefijo con resultado prestado y fuente registrada, manteniendo
los controles de retorno de parámetros prestados. Archivos mínimos:
`StatementAnalyzer.cpp` y la regresión de campo existente. Se verificará con
build incremental, análisis de ambos escapes y controles dirigidos.

El build incremental terminó con código 0
(`field-prefix-forward-return-build.log`), compilador SHA-256
`46b70ba2a898622cb80cda26a94a826f21b5b2b620f5cb9c08e0cc9effd7ecdc`.
Los dos escapes ahora se rechazan con el diagnóstico del dueño local y la
regresión de campo completa pasó (`field-prefix-forward-return-after.log`).
También pasaron los controles dirigidos de patrón, operador de dueño local y
retorno prestado (`field-prefix-forward-return-pattern.log`,
`field-prefix-forward-return-owner.log`, `field-prefix-forward-return-control.log`).
Los programas de escape no se ejecutaron. La revisión se limita a los prefijos
probados; excepciones y las demás composiciones siguen pendientes. Sin commit.

### Excepción de operador sobre campo propietario

`tests/integration/operator_increment_owned_field_throw.py` comprueba los
operadores `++` y `--`, en prefijo y posfijo, cuando el operador lanza una
excepción. En cada caso exige que el catch se ejecute, el campo conserve el
valor original, no haya destrucción antes de salir del ámbito y el objeto se
destruya exactamente una vez al finalizarlo. Las ocho combinaciones O0/O2
compilaron y ejecutaron con salida 42 (`field-throw-before.cmd` y `.log`).

El compilador aislado tiene SHA-256
`46b70ba2a898622cb80cda26a94a826f21b5b2b620f5cb9c08e0cc9effd7ecdc`.
La prueba consumió Core binario preexistente en
`A:/nsc-fixes-evidencia/arc-public-sdk/NativeSharpCore.lib` (SHA-256
`f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`) y su
metadata (SHA-256 `f8e4189f7f35f457c61550b015ba001815e76ec0337973add8cc810f9aad669c`).
Ese Core no se construyó con el compilador de esta ejecución. La prueba no
acredita campos en productores binarios, receptores calculados ni propagación
por todas las construcciones de control; el caso completo continúa abierto.

### Campo y operador en productor binario

`tests/integration/operator_increment_owned_field_binary.py` reutiliza el
control binario de clase, añade `Holder.Item` como campo `owned` al productor y
lo consume por `LibraryReference`. El productor y consumidor se compilaron
en las cuatro combinaciones O0/O2 sin caché; las cuatro ejecuciones devolvieron
42 y verificaron valores y destrucciones del prefijo, posfijo transferido y
posfijo prestado como argumento (`field-binary-before.cmd` y `.log`). En cada
productor, el intento de actualizar el campo durante un préstamo activo se
rechazó con el diagnóstico de campo prestado y el control de local fijado
también fue rechazado. Los negativos no se ejecutaron.

Se usó el compilador aislado SHA-256
`46b70ba2a898622cb80cda26a94a826f21b5b2b620f5cb9c08e0cc9effd7ecdc`, que
compiló tanto productor como consumidor. Los artefactos de la prueba son
temporales en A y no se cambió la instalación. La ejecución no cubre DLL,
excepciones del operador definido en el binario, receptores calculados, ni todas
las formas de escape. Los cambios continúan sin commit y el caso no se cierra.

`tests/integration/operator_increment_owned_field_throw_binary.py` cubre ahora
el operador que lanza desde una biblioteca estática NativeSharp separada. El
productor se compiló en O0 y O2; para cada uno, el consumidor probó prefijo y
posfijo de `++` y `--` en O0 y O2. Las 16 ejecuciones devolvieron 42: capturaron
la excepción, conservaron el campo original y observaron una destrucción al
salir del ámbito (`A:/nsc-fixes-evidencia/field-throw-binary-current.cmd` y
`.log`). Se reutilizó Core binario y se compiló sin caché con el árbol local
conjunto. La prueba no cubre DLL ni receptores calculados y permanece sin
registrar mientras la implementación de los operadores sigue sin commit.

La misma regresión admite `--dll` y conserva la variante estática por defecto.
Los 16 casos de la biblioteca estática y los 16 de la DLL NativeSharp
devolvieron 42 al cruzar optimizaciones de productor y consumidor O0/O2:
`field-throw-static-current.log` y `field-throw-dll-current.log`. La DLL
permaneció cargada mientras se usó; no se probó descarga dinámica. Esta
evidencia sigue usando el árbol conjunto y no cubre receptores calculados ni
consolida por sí sola el cambio de operadores.

### Receptor prestado y argumentos

Con el compilador aislado existente, `++alias.Item` se rechaza con
`user-defined increment/decrement update is not implemented` cuando `alias`
es un préstamo de un `Holder` local RAII. La reproducción original y la salida
están en `A:/nsc-fixes-evidencia/operator-alias-field/`.

Se comprobó además `Both(alias.Item, ++holder.Item)`, sin ejecutar el programa:
el análisis rechaza el primer argumento con `borrowed parameter 'first'
requires a checked owner or borrowed source`. La reproducción, el comando y la
salida están en `A:/nsc-fixes-evidencia/operator-alias-argument/`. Este rechazo
precede a cualquier conclusión sobre una invalidación durante la evaluación
del segundo argumento; no demuestra que ese supuesto pase ni que falle.

En el árbol actual, `rememberDirectOwnedFieldArgument` solamente registra
receptores locales RAII; `analyzeOverloadedUnary` también exige esa condición
para aceptar el campo del operador. `hasActiveViewOfStorage` compara raíces
posibles de alias, pero esas rutas por sí solas no demuestran procedencia del
campo ni protección de argumentos para el receptor prestado. Habilitar el
operador requiere seguir ambas rutas, preservar la identidad del almacenamiento
y verificar escapes e invalidaciones. El caso permanece pendiente; no se modificó
el compilador ni se ejecutaron los ejemplos potencialmente inseguros.

El ensayo posterior permite el operador cuando el receptor es un local prestado
cuya procedencia demuestra un único dueño local RAII. El resultado prefijo queda
prestado de ese dueño; su campo se conserva al pasarlo como argumento o al
declarar una vista local. El máximo ámbito de esa vista queda limitado por el
local receptor. Para un origen ambiguo se mantiene el rechazo. No se modifica
la regla general de pasar `alias.Item` como argumento independiente.

El build incremental `operator-alias-build.cmd` terminó con código 0. La prueba
`tests/integration/operator_increment_owned_field_alias.py`, ejecutada con el
runtime aislado en `operator-alias-directed.cmd`, pasó cinco controles válidos
en O0 y O2 y siete controles semánticos de invalidación o escape; los casos
inseguros no se ejecutaron. La regresión anterior para campo directo también
pasó (`operator-alias-controls.log`). La primera ejecución posterior al build
no indicó el runtime aislado y falló al enlazar: se conserva como
`operator-alias-directed-after.log`, sin atribuirla al cambio semántico.

El trabajo está sin commit y no acredita todos los receptores calculados,
propietarios condicionales, productores binarios ni la corrección completa del
caso 39. Falta revisar de manera dirigida la interacción con aliases de campos
y con controles binarios antes de consolidar el conjunto.

La prueba `operator_increment_owned_field_alias_binary.py` compiló por separado
una biblioteca estática NativeSharp y su consumidor con el compilador aislado
SHA-256 `f5e659573faa3aae0c0b271eadcca682dc237df3fe0e022c79ea2d227fee267f`.
Las cuatro combinaciones de optimización O0/O2 entre productor y consumidor
ejecutaron con salida 42 y comprobaron prefijo prestado, posfijo transferido,
posfijo como argumento y destrucción. Los controles de campo prestado y local
fijado fueron rechazados por el análisis. Comandos y salida:
`A:/nsc-fixes-evidencia/operator-alias-binary.cmd` y `.log`.

La prueba `operator_increment_owned_field_alias_throw_binary.py` utilizó la
misma matriz de productor y consumidor O0/O2 para prefijos y posfijos `++` y
`--` que lanzan. Pasaron 16 ejecuciones con biblioteca estática y 16 con DLL
NativeSharp: cada una captura la excepción, observa el campo original y exige
una destrucción al final. Los comandos y salidas están en
`operator-alias-throw-static.cmd`/`.log` y `operator-alias-throw-dll.cmd`/`.log`
dentro de `A:/nsc-fixes-evidencia/`. El Core utilizado es el binario previo de
`pointers-sdk-refresh`, no un Core reconstruido para esta comprobación. La DLL
permanece cargada durante su uso; no se verificó descarga dinámica.

El runtime aislado de las pruebas tiene SHA-256
`3235a612c2dc62bb05c244885bc135ec76bf847f4f9a700badd9017c1f913a53`.
Estas verificaciones no cubren procedencia condicional, receptores calculados,
todos los escapes ni los demás pendientes del caso; el cambio continúa sin
commit y el caso permanece abierto.

La regresión dirigida añadió reasignaciones del alias. Reasignarlo al mismo
dueño y actualizar su campo ejecuta correctamente en O0 y O2; reasignarlo a
otro dueño después de guardar una vista no permite reemplazar el campo del
primer dueño mientras la vista siga viva. Se confirmó además que, cuando el
alias puede proceder de dos dueños distintos, `++alias.Item` todavía se rechaza
con la barrera de implementación: no se considera resuelto ese caso válido.
La ejecución completa de esta selección y sus diagnósticos están en
`A:/nsc-fixes-evidencia/operator-alias-directed-runtime.log`.

El ensayo posterior conserva todos los dueños locales posibles del receptor al
retener el campo en los argumentos y al crear un local prestado. La identidad
principal sigue usándose para el diagnóstico de retorno y el origen alternativo
se conserva en el binding local. El código acepta esta ruta sólo si todos los
dueños posibles registrados son locales RAII y no hay un parámetro posible.

El build incremental `operator-alias-build.cmd` terminó con código 0; el nuevo
`nsc.exe` tiene SHA-256
`ed7429ec1a77803b3124874912312fbf390763015df16338b8c9151c48b7d0f7`.
`operator-alias-directed-runtime.log` registra ejecución O0/O2 con el primer y
segundo dueño seleccionado y controles negativos de vista, reemplazo, movimiento,
mutación directa y anidada, y retorno reenviado. Los negativos sólo se analizaron.
También pasaron de nuevo `operator-alias-controls.cmd`,
`operator-alias-binary.cmd`, `operator-alias-throw-static.cmd` y
`operator-alias-throw-dll.cmd`. Las pruebas binarias anteriores no ejercitan por
sí mismas múltiples dueños; Core sigue siendo el binario preexistente señalado
arriba. El cambio permanece sin commit y no cubre receptores calculados o dueños
de tipo parámetro.

La regresión binaria de alias ahora incluye dos dueños locales: el consumidor
reasigna el alias, comprueba prefijo y posfijo sobre el segundo objeto y exige
la destrucción de ambos. Las cuatro combinaciones O0/O2 de biblioteca estática
y consumidor ejecutaron con salida 42; el análisis rechazó actualizar el campo
si una vista del segundo dueño seguía activa. Véase
`A:/nsc-fixes-evidencia/operator-alias-binary.log`.

La prueba `operator_increment_owned_field_multi_alias_throw_binary.py` comprueba
el alias con dos dueños cuando el operador lanza. Pasaron 16 combinaciones de
biblioteca estática y 16 de DLL, cruzando O0/O2 y los cuatro prefijos o posfijos:
ambos campos conservan su valor y ambos objetos se destruyen al salir. Comandos
y salidas `operator-multi-alias-throw-static` y `operator-multi-alias-throw-dll`
en `A:/nsc-fixes-evidencia/`. Estas pruebas usan el Core binario previo indicado
arriba y no verifican descarga de DLL.

Se añadió también un control válido con receptor parámetro `borrowed Holder`.
Con el compilador actual falla su compilación en O0 y O2, mientras los demás
controles de `operator_increment_owned_field_alias.py` pasan; el resumen está
en `operator-alias-directed-runtime.log`. El soporte de parámetros sigue
pendiente, y la regresión permanece deliberadamente fallida hasta implementarlo;
no se interpreta esa selección como aprobada.

El ensayo posterior habilita el operador sobre el campo de un parámetro
`borrowed` directo. Los préstamos temporales de ese campo se registran aparte
de los procedentes de locales; se consideran potencialmente alias otros
parámetros o `this`. También se conserva la procedencia de una vista local.
No se admite devolver esa vista, directamente o a través de los alias y llamadas
probados, hasta poder expresar y verificar el campo en el contrato de retorno.
Ese rechazo es una limitación pendiente, no una solución completa del contrato.

El compilador incremental SHA-256
`bd0255c8790e8aa449b7a7f3b9d5070c332c726378ea3702f25f95a1982a7893`
pasó `operator-alias-directed.cmd`: los dos controles válidos del parámetro se
ejecutaron en O0/O2 y los casos inseguros se rechazaron sin ejecutarse, incluyendo
argumentos en ambos órdenes, posibles alias entre parámetros, reemplazo de una
vista local y retornos directos, reenviados o mediante otro alias. Una llamada
con vista local se rechaza antes del retorno por posible mutación; se conserva ese
diagnóstico. Pasaron `operator-alias-controls.cmd` y `operator-alias-binary.cmd`;
este último compiló una biblioteca estática y consumidor O0/O2 y ejecutó el
operador sobre el parámetro. Evidencias en `A:/nsc-fixes-evidencia/`.

Las rutas de alias de parámetro, retorno con contrato de campo, receptores
calculados y las combinaciones binarias no cubiertas permanecen pendientes.
El cambio y las pruebas siguen en el árbol sin commit; los resultados se refieren
al árbol conjunto, no a una revisión limpia certificada.

El ensayo siguiente admite un alias local cuyo origen comprobado consiste sólo
en parámetros `borrowed`. Las mismas reglas de campo activo protegen el objeto
por su nombre directo o por el alias; una vista local mantiene el parámetro como
origen. Un campo pasado como argumento puede quedar prestado durante la llamada.
Mientras no exista un contrato verificable de campo, se rechaza su retorno
prestado a través de una llamada, con o sin el operador. Se preserva también el
rechazo de los retornos de actualizaciones a través de alias encadenados.

Con el compilador incremental SHA-256
`26b699f12a155a938bc5dd631f4c753886fead7e3fd2be26bbfb7945ab30765b`,
`operator-alias-directed.cmd` pasó los controles positivos O0/O2 y los negativos
de invalidación y escape sin ejecutar estos últimos. Pasó también
`operator-alias-controls.cmd`. `operator-alias-binary.cmd` compiló productor
y consumidor con O0/O2 y ejecutó el operador a través de un alias del parámetro;
las cuatro combinaciones devolvieron 42. Los comandos y logs están en
`A:/nsc-fixes-evidencia/`. El árbol continúa sin commit; receptores calculados,
orígenes mixtos local/parámetro y el contrato para devolver la vista siguen
pendientes. Las pruebas binarias no acreditan todos los caminos de error.

El ensayo posterior admite un alias con dueños posibles mixtos, siempre que los
locales sean RAII y los parámetros sean `borrowed`. Se conservan ambas clases de
procedencia para proteger los campos durante la llamada y durante una vista local.
Se mantiene el rechazo del retorno de la vista hasta disponer de un contrato de
campo verificable en el llamador.

La compilación incremental terminó con código 0. Con `nsc.exe` SHA-256
`3e99a7f409ae7eac918a246be4c66904f8f9ff5a0886ef3a1c9994de85128eea`
pasó `operator-alias-directed.cmd` en O0/O2 para ambos órdenes de origen y vistas
locales. El análisis rechazó los intentos probados de reemplazo, mutación o
retorno mientras existía un préstamo; no se ejecutaron esos programas. Pasaron
también `operator-alias-controls.cmd` y las cuatro combinaciones de productor y
consumidor de `operator-alias-binary.cmd`, ahora con un alias de origen mixto.
Los logs están en `A:/nsc-fixes-evidencia/`. El cambio sigue sin commit; no se
han verificado todas las formas de alias, receptores calculados, ni el contrato
de retorno entre bibliotecas.

Los dos archivos originales `increment_operator.ns` y `decrement_operator.ns`
se analizaron directamente, sin modificarlos, mediante
`A:/nsc-fixes-evidencia/39-original-recheck-now.cmd`: ambos devolvieron código 0
y verificación LLVM correcta. Son declaraciones sin uso del operador; no prueban
por sí solas su comportamiento en ejecución. Las regresiones dirigidas de struct
y clase propietaria pasaron con `39-regression-struct-now.cmd` y
`39-regression-owner-now.cmd`.

Un control adicional encontró que pasar la propia vista local a `Read(view)` se
rechazaba como si `view` fuera el contenedor cuyo campo se protege. La corrección
en `hasActiveViewOfStorage` distingue, sólo para una vista creada directamente
por prefijo `++`/`--`, esa vista del contenedor. Con compilación incremental
y `nsc.exe` SHA-256
`e486f7786bf580a06b9ec41a41ca92d639666000875bef74ec0f5c1adb07fb27`,
`operator-alias-directed.cmd`, `operator-alias-controls.cmd` y
`39-regression-owner-now.cmd` pasaron; la lectura también pasó mediante la
biblioteca estática en `operator-alias-binary.cmd`. Los controles negativos siguen
rechazando invalidación del contenedor y retorno de la vista sin contrato. Los
resultados corresponden al árbol conjunto y no cierran el caso completo.

Una vista local creada por el operador también podía copiarse a un alias
prestado, pero pasar ese alias a una función de lectura se rechazaba como si
fuera el contenedor. `hasActiveViewOfStorage` sigue ahora la cadena de préstamos
locales sin recursión y solo reconoce la vista cuando ni ella ni sus alias se
han reasignado. Se conservan los rechazos para el caso donde la vista o el alias
se reasignan al contenedor y una llamada podría reemplazar el campo.

El build incremental pasó. Con `nsc.exe` SHA-256
`8c8f2fae04fc5321d9d82facacb556927c06f4de6fb519409fd8d83bc5fd30c0`,
`operator-alias-directed.cmd`, `operator-alias-controls.cmd` y
`operator-alias-binary.cmd` pasaron en sus configuraciones O0/O2. Este último
incluye lectura a través del alias con el productor binario. Los casos inseguros
se analizaron, sin ejecutarse. `field-view-alias-analyze.cmd` terminó con código
0 y produjo cinco advertencias de padding en encabezados; no se estableció una
comparación de baseline para ellas. El análisis se limita al archivo dirigido y
no certifica el conjunto de cambios provisionales.

Se detectó un escape en el propio ensayo: un local `borrowed` inicializado con
un parámetro podía reasignarse a la vista creada por `++holder.Item` y devolverse;
el análisis lo aceptó. No se ejecutó el programa inseguro. El recorrido de
retorno inspeccionaba el origen inicial del local, pero no sus posibles fuentes
locales de reasignación. Ahora recorre iterativamente ambas relaciones con un
conjunto de visitados, y rechaza la devolución cuando alcanza esa vista de campo.
La política de retorno de un préstamo ordinario de parámetro se conserva.

El build incremental pasó con `nsc.exe` SHA-256
`c8eba6a8bf7b22fb26ce25d588d061c2c73b06d6e56d92fa64ac59e6c23053f8`.
`operator-alias-directed.cmd` rechazó el escape directo y el encadenado tras
reasignaciones; su control de retorno ordinario y las lecturas válidas pasaron
en O0/O2. También pasaron `operator-alias-controls.cmd` y
`operator-alias-binary.cmd`. `field-view-reassigned-return-analyze.cmd` devolvió
0 y registró cinco advertencias de padding en encabezados sin comparación de
baseline. Los comandos y logs están en `A:/nsc-fixes-evidencia/`. Esta corrección
es provisional y no acredita todos los posibles escapes ni el contrato de campo
entre bibliotecas; sigue sin commit.
# Retorno de vista reasignada con tipos precompilados

La regresión `operator_increment_owned_field_alias_binary.py` comprueba que
una vista obtenida con `++holder.Item` no puede escapar por un alias que antes
refería otro parámetro, ni por un segundo alias derivado. El productor `Holder`
y `Box` se compiló como biblioteca NativeSharp en O0 y O2; para ambas bibliotecas
el análisis del consumidor rechazó las dos formas con `borrowed field update
cannot be returned without a field lifetime contract`. Los controles positivos
de consumidor O0 y O2 ejecutaron 42 para cada productor. Comando y salida:
`A:/nsc-fixes-evidencia/operator-alias-binary.cmd` y `.log`; compilador aislado,
build hash `f9fb4e7320c5d3ce`, con otros cambios locales sin commit.

También pasó `operator-alias-controls.cmd`, con sus controles de campos desde
fuente en O0/O2 y diagnósticos negativos esperados. La prueba binaria acredita
estas formas de alias y esas combinaciones, no un contrato general de retorno
de campos ni la corrección completa de `++` y `--` de usuario.

La regresión de alias incorpora ahora un prefijo `--` sobre un campo `owned`:
la lectura de la vista y el reemplazo posterior a su ámbito ejecutaron 42 en
O0/O2; el análisis rechazó reemplazar el campo mientras la vista está viva y
devolverla desde un parámetro prestado. La variante con productor precompilado
también verificó el valor y las destrucciones, y rechazó el retorno de vista,
para productor y consumidor O0/O2. Comandos y salidas:
`A:/nsc-fixes-evidencia/operator-alias-decrement-current.cmd` y `.log`, y
`operator-alias-decrement-binary-current.cmd` y `.log`. Estas pruebas usan
el árbol provisional conjunto, no cubren todas las formas de receptor ni
demuestran el contrato general de retorno de campos.

Verificación posterior, 2026-11-11: la prueba dirigida
`nsc.compiler.borrowed-call-owner-return` volvió a pasar 1/1 sin omisión con
el compilador incremental de la copia; `FAILURES []` y diagnósticos en
`A:/nsc-fixes-evidencia/borrowed-call-owner-current-20261111.log`. No se
ejecutaron las reproducciones inseguras. La implementación continúa mezclada
con cambios locales y esta repetición no valida un commit aislado ni las
formas de procedencia indicadas como pendientes más arriba.
