# Arreglos accesibles por un préstamo de solo lectura

Un objeto de clase extraído mediante `Rc.Get()` o `Arc.Get()` es un préstamo de
solo lectura. La implementación anterior conservaba esa condición al acceder a
campos de clase, pero la perdía si el campo era un arreglo nativo. En ambas
familias el análisis aceptó `alias.Data[0] = 10`; con `Rc` aceptó además guardar
`alias.Data` en un local fuera del alcance del dueño. Los comandos y resultados
anteriores están en `A:/nsc-fixes-evidencia/public-readonly-array-field.log`,
`public-readonly-array-field-Arc.log` y `public-readonly-array-escape-Rc.log`.
Las reproducciones se limitaron al análisis semántico, sin ejecutar el escape.

La corrección conserva el préstamo al leer un campo de arreglo desde ese alias,
impide escribir sus elementos y conserva el préstamo en elementos de referencia.
También rechaza guardar el arreglo en destinos sin seguimiento del préstamo y
mantiene la marca al convertir explícitamente a un arreglo nativo. El análisis de
la conversión previamente aceptó `((int[])alias.Data)[0] = 10`; la salida de
esa primera ejecución se observó, pero su archivo fue sobrescrito al repetir la
prueba. La nueva regresión conserva esa expresión como caso negativo.

La regresión independiente `nsc.compiler.readonly-borrowed-array` usa los
controles local y atómico ya existentes. Comprueba escritura, inicialización y
reasignación de un destino, referencia mutable a elemento, condicional,
coalescencia, conversión explícita y acceso a elementos de clase; verifica la
lectura por el préstamo y la escritura ordinaria en O0/O2. Pasó 1/1 en
`A:/nsc-fixes-evidencia/readonly-array-ctest.log`. Los controles de las familias
públicas registran también rechazos para retorno y argumento y pasaron en
`public-readonly-array-regressions.log`, `public-readonly-array-escape-regressions.log`
y `public-readonly-array-class-escape.log`. Las pruebas existentes de referencias
locales y retornos pasaron 2/2 (`readonly-array-reference-tests.log`).

La compilación del compilador fue incremental y está documentada en
`readonly-array-build.log`; el sistema regeneró la configuración y la identidad
de versión, por lo que reconstruyó más objetivos que los cinco archivos de
frontend modificados. Las pruebas de punteros públicos se hicieron desde
fuentes; no acreditan consumo binario, seguridad concurrente, todos los caminos
de conversión ni acceso mediante código `unsafe`.

Una selección posterior comprobó `+=`, incremento prefijo y posfijo sobre un
elemento prestado, y escrituras después de mezclar un arreglo prestado y otro
ordinario en un condicional o `??`. El compilador rechazó las seis formas para
los controles local y atómico; un arreglo ordinario ejecutó las tres operaciones
aritméticas y devolvió 42 en O0/O2. Los resultados están en los archivos
`readonly-array-ctest-compound_write,postfix_write,prefix_write,ordinary_compound_write.log`
y `readonly-array-ctest-mixed_*.log` de `A:/nsc-fixes-evidencia`. Las primeras
ejecuciones individuales de prefijo y posfijo fallaron en la expectativa de la
prueba: el compilador sí los rechazaba, pero con `requires a modifiable` en vez
de `not assignable`. Se ajustó la expectativa al diagnóstico observado y la
selección posterior pasó. No hubo cambio del compilador en esta comprobación.

## Tuplas que contienen un préstamo de solo lectura

El análisis aceptaba `(Payload, int) saved = (alias, 42)` y la forma que usa
`owner.Get()` directamente para `Rc` y `Arc`; al construir la tupla descartaba
la marca de préstamo del elemento. Las reproducciones se limitaron al análisis
semántico, sin ejecutar un alias potencialmente inválido. Resultados anteriores
en `A:/nsc-fixes-evidencia/public-tuple-borrow-tuple_alias_escape*.log` y
`public-tuple-borrow-tuple_direct_escape*.log`.

Como la tupla no conserva una vida por elemento, el análisis rechaza ahora un
elemento que ya lleva la marca de préstamo de solo lectura. La regla también
puede rechazar una tupla local cuyo uso sería seguro; no implementa seguimiento
general de vidas dentro de tuplas. La selección dirigida comprueba alias y
llamada directa, tupla anidada y arreglo prestado, para controles local y
atómico; ejecuta tuplas ordinarias y lecturas escalares en O0/O2
(`readonly-array-ctest-tuple_alias,tuple_direct,tuple_nested,tuple_array,tuple_scalar,tuple_ordinary.log`).
La prueba pública desde fuentes rechazó ambas formas para `Rc` y `Arc`
(`public-tuple-borrow-tuple_alias_escape,tuple_direct_escape.log`). El build fue
incremental y cuatro controles de tuplas y referencias pasaron
(`readonly-tuple-existing-controls.log`). No se verificó el SDK binario.

### Inicializadores con llaves

La tupla `(Payload, int) saved = { alias, 42 }` y una estructura con un campo
`Payload` sin calificador también aceptaban el alias prestado en `Rc` y `Arc`.
El análisis de entradas agregadas ahora rechaza esa pérdida en tuplas y en
campos sin ownership de estructuras. No modifica otros destinos ni implementa
seguimiento de vida por campo. Un intento separado de declarar el campo como
`borrowed` ya era rechazado en su declaración por Stage 50; no demuestra que
ese tipo de campo esté soportado (`public-tuple-borrow-borrowed_field_aggregate.log`).

Las regresiones dirigidas rechazaron ambas formas y una tupla anidada para los
controles local y atómico; los controles con un campo ordinario y lectura escalar
ejecutaron con 42 en O0/O2. Las cuatro reproducciones públicas de tuplas y
estructuras pasaron después de la corrección. El build fue incremental y tres
controles existentes de agregados y tuplas pasaron 3/3. Los comandos y salidas
están en `readonly-array-ctest-tuple_aggregate_alias,struct_aggregate_alias,tuple_aggregate_nested,tuple_aggregate_scalar,struct_aggregate_ordinary.log`,
`public-tuple-borrow-tuple_alias_escape,tuple_direct_escape,tuple_aggregate_escape,struct_aggregate_escape.log`
y `readonly-aggregate-existing-controls.log`, todos en `A:/nsc-fixes-evidencia`.
El caso público de uso normal de `Rc` y `Arc` también pasó desde fuentes
(`public-tuple-borrow-safe.log`).

## Elementos de un arreglo nuevo

`new Payload[] { alias }` aceptaba un alias de clase procedente de `Get()`;
el arreglo no conserva la vida ni la restricción del elemento. Los análisis
semánticos para `Rc` y `Arc` lo aceptaban antes del cambio, según
`public-tuple-borrow-array_element_initializer_escape-Rc.log` y
`public-tuple-borrow-array_element_initializer_escape-Arc.log`. No se ejecutó
el acceso potencialmente inválido.

`analyzeNativeArrayAllocation` rechaza el elemento cuando ya está marcado como
préstamo de solo lectura. Es conservador: puede rechazar un arreglo local seguro,
pues no implementa seguimiento de vida por elemento. La prueba dirigida con
los controles internos rechazó el alias y la llamada directa para ambas familias;
los controles de elementos escalares y objetos ordinarios se ejecutaron en O0/O2.
La prueba pública dirigida rechazó el caso para `Rc` y `Arc`, desde fuentes.
El control existente `nsc.stage41.arrays-strings-core` pasó 1/1 tras compilar su
objetivo incrementalmente. La evidencia se encuentra en
`readonly-array-ctest-array_initializer_alias,array_initializer_direct,array_initializer_scalar,array_initializer_ordinary.log`
y `public-tuple-borrow-array_element_initializer_escape.log`, en
`A:/nsc-fixes-evidencia`.

La primera compilación incremental del compilador heredó `TEMP=C:\WINDOWS\TEMP`
por error: el directorio de build fue el aislado de A, pero no se verificó si se
crearon archivos temporales en C. La compilación posterior del control usó
variables temporales explícitas en A mediante `build-array-initializer-controls.cmd`.

## Estructuras anidadas

Tras `764137d50`, una regresión con un arreglo de estructuras que contienen un
campo de clase mostró que `Child saved = alias.Data[0].Item` seguía siendo
aceptado (`readonly-array-struct-before.log`). La condición de préstamo se
perdía en el elemento struct antes de llegar al campo de clase. Se propagó al
elemento y a los campos struct anidados, así como a una conversión explícita;
los destinos no rastreados rechazan copiar esos valores prestados. Esta regla es
conservadora: también puede rechazar la copia por valor de una estructura que
solo contiene escalares. No cambia la lectura de campos escalares: la prueba
ejecuta una lectura de esa forma en O0/O2 para ambos controles.

La regresión dirigida verifica escape mediante campo, inicialización,
reasignación y conversión de la estructura, además de las comprobaciones
anteriores. El resultado final está en `readonly-array-ctest.log`. Los casos
de referencia a través de otras construcciones de valores no se consideran
cubiertos por esta prueba.

## Carga struct con campo de clase

Un control posterior usó `SharedLocalHandle<Box>.Get()` con retorno
`ref readonly Box`. La escritura directa `Get().Item.Value = 10` se rechazaba,
pero `Child saved = Get().Item` se aceptaba, perdiendo el préstamo del dueño
(`readonly-struct-payload-before.log`). Se conserva ahora el préstamo para un
retorno struct de instancia por referencia de solo lectura sin vida estática;
el campo de clase derivado ya no puede guardarse en un local sin préstamo.
La comprobación recorre los campos de estructuras anidadas; si solo encuentra
escalares conocidos, permite copiarlas por valor. Para tipos desconocidos,
precompilados o que pueden contener referencias conserva el préstamo de manera
conservadora.

La prueba dirigida cubre acceso directo y mediante `SharedBorrow`, tanto con el
control local como el atómico; además comprueba lectura escalar O0/O2. La
selección anterior de uso público pasó y las dos pruebas de referencias
existentes pasaron (`readonly-array-ctest.log`,
`public-readonly-array-existing-controls.log`, `readonly-array-reference-tests.log`).
La regresión ejecuta copias de estructuras escalares directas y anidadas en
O0/O2 y rechaza la copia del struct que contiene una clase. No acredita todos los
tipos de campos, modos de retorno ni la vida de módulos precompilados.

## Iteración sobre arreglo prestado

La prueba dirigida posterior encontró una limitación de las restricciones de
esta tanda: `foreach (Child child in nested.Data)` se rechaza incluso si el
cuerpo solo lee `child.Value`. El local sintético `$__foreach_0_source` se
declara sin calificador y la validación impide almacenar allí el arreglo
prestado (`A:/nsc-fixes-evidencia/readonly-array-foreach-before.log`). En el
caso que intenta escribir, también se diagnostica que el elemento no es
asignable. Esta observación es anterior a la corrección siguiente.

Para la iteración por valor con origen local rastreado, el análisis conserva
ahora el vínculo al dueño en el origen sintetizado y en el elemento cuando este
puede contener referencias. La regresión ejecuta lectura de clase, lectura
escalar, bucles anidados y un control con arreglo ordinario en O0/O2; rechaza
mutación, escape y movimiento del dueño. Resultados en
`readonly-array-ctest.log`, `readonly-array-ctest-nested_foreach_nested.log`,
`readonly-array-ctest-scalar_foreach_read.log` y
`readonly-array-ctest-ordinary_foreach_read.log`.

Al concluir esa tanda seguían pendientes la lectura mediante `foreach ref` y
las expresiones de origen cuyo dueño no se pueda rastrear. La primera se aborda
en la continuación siguiente; la segunda sigue pendiente. Una prueba de `break` pasó el análisis pero
falló en generación con `Stage 34 cannot map identifier/member expression`;
el mismo diagnóstico se reprodujo con un arreglo ordinario sin punteros en
O0/O2 (`readonly-array-foreach-break.log`, `foreach-break-control/results-before.json`).
Ese hallazgo se corrige en la continuación documentada más abajo.

El control existente `nsc.stage52-3.foreach` pasó
(`readonly-foreach-existing-control.log`); su resultado CTest en Windows no
garantiza por sí solo ejecución de todos sus programas. La prueba existente
`nsc.stage52-5.foreach-semantic-gaps` falló dos de 24 comprobaciones
(`readonly-foreach-existing-test.log`); los mismos dos mensajes aparecen en la
base y en la pasada anterior de la rama. Ninguna de estas observaciones demuestra
que todas las rutas de foreach estén cubiertas.

### Lectura por referencia

En la revisión anterior, `foreach (ref Child child in nested.Data)` se
rechazaba antes de analizar el cuerpo por intentar guardar el elemento en un
local sin calificador (`readonly-array-ctest-nested_foreach_ref_read.log`,
salida inicial). El local de referencia sintetizado conserva ahora el préstamo
ya rastreado y su dueño, sin cambiar las demás referencias locales. La lectura
de clases, estructuras y escalares pasó para ambos controles en O0/O2; se
rechazaron la escritura, el escape, el movimiento del dueño y `ref mut`. La
regresión final está en `readonly-array-ctest.log`; los controles de referencias
locales y de `foreach` ordinario pasaron en
`readonly-array-reference-tests.log` y `readonly-foreach-existing-control.log`.
Las rutas sin procedencia rastreada siguen abiertas. La prueba que compara con la base para los dos fallos de
`foreach-semantic-gaps` no se convierte en aprobada por esta corrección.

### Fuente obtenida directamente del dueño

La forma `foreach (Child child in owner.Get().Data)` aún se rechazaba porque el
resultado derivado no llevaba el ordinal del dueño local
(`readonly-array-ctest-direct_foreach_read.log`, resultado anterior). Una
llamada de instancia que devuelve una referencia de solo lectura con vida
`this` conserva ahora el dueño si el receptor es un local move-only. La fuente
sintetizada del bucle utiliza ese origen; las expresiones temporales y otras
vidas explícitas no reciben esta autorización.

Las regresiones dirigidas aceptaron las lecturas por valor y por referencia en
O0/O2 para los controles local y atómico, y rechazaron escape, movimiento del
dueño y dueño temporal (`readonly-array-ctest.log`). La selección con `Rc` y
`Arc` públicos desde fuentes ejecutó lectura O0/O2 y rechazó escape y
movimiento (`public-direct-foreach-regressions.log`). Los controles de
referencias y de foreach ordinario pasaron; no se verificó el consumo binario
ni todos los tipos de receptor.

La variante `owner.Borrow().Get().Data` también se rechazaba porque el receptor
de `Get` es una vista en lugar del dueño directo
(`readonly-array-ctest-direct_view_foreach_read.log`, resultado anterior). El
análisis usa ahora el rastreador de raíz de vistas existente cuando el retorno
tiene vida `this`, y solo acepta como dueño un local move-only que no sea otra
vista. La selección dirigida ejecutó las formas con vista temporal y nombrada
por valor y referencia, y rechazó escape, movimiento y dueño temporal
(`readonly-array-ctest.log`). La variante pública de `Rc` y `Arc` también pasó
desde fuentes (`public-direct-view-foreach-regressions.log`). No se demuestra
con ello la procedencia de vistas arbitrarias ni el consumo binario.

### Salida incondicional del bucle

Con `break` incondicional, el análisis no producía los enlaces del incremento
sintético de `foreach`, aunque HIR todavía lo visita. La comparación de
`foreach-break-control/sema-before.log` y `foreach-no-break-control/sema-dump.log`
mostró dos enlaces del índice frente a cuatro; después se observaron cuatro en
`foreach-break-control/sema-dump.log`. El análisis del incremento
sintético se ejecuta ahora en un estado descartable si no hay borde de vuelta,
sin convertir ese borde en alcanzable ni alterar el tratamiento de `for`
escritos directamente.

La reproducción mínima original, conservada sin cambios, compila y termina con
42 en O0 y O2 (`foreach-break-control/results.json`). Las pruebas dirigidas
comprueban `break` y `return` en arreglos prestados, `break` ordinario y los
controles de `continue` y `break` condicional, para ambas familias y niveles de
optimización (`readonly-array-ctest-nested_foreach_break,ordinary_foreach_break,nested_foreach_return.log`
y `readonly-array-ctest-ordinary_foreach_continue,ordinary_foreach_conditional_break.log`).
El control existente `nsc.stage52-3.foreach` pasó en
`readonly-foreach-existing-control.log`. Esto no cubre todas las formas de
salida ni corrige los dos fallos previos de `foreach-semantic-gaps`.
