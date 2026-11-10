# Llamadas mientras existe una vista de campo

Una vista de `holder.value.Borrow()` podía seguir viva mientras
`holder.Replace()` reemplazaba ese mismo propietario. El caso
`overwrite_field_via_method` del SDK público fue aceptado por el análisis antes
del cambio; `A:/nsc-fixes-evidencia/shared-method-borrow-before.log` conserva el
comando y la salida. No se ejecutó la lectura posterior potencialmente inválida.

Se reutiliza el seguimiento de raíces y alias de vistas para rechazar llamadas
de instancia potencialmente mutadoras sobre un receptor nominal cuando un campo
de ese receptor tiene una vista viva. Los métodos de clase carecen de un
contrato de solo lectura comprobado y se tratan conservadoramente: la regla
puede rechazar un método que no modifica el campo. Se conservan los métodos
readonly de estructura y los receptores independientes o cuyo préstamo terminó.

Las pruebas dirigidas desde fuentes y desde el SDK binario rechazaron el ejemplo
y una variante con alias para `Rc` y `Arc`. Los controles de receptor
independiente y de fin de ámbito ejecutaron 42 en O0 y O2. Los controles
existentes de reemplazo, movimiento y vistas anidadas también pasaron.
Evidencia en `shared-method-borrow-after.log`, `shared-method-borrow-binary.log`
y `shared-method-borrow-controls.log`, en `A:/nsc-fixes-evidencia`.
`shared_field_method_borrow.py` se invoca directamente porque el archivo CMake
compartido contiene cambios concurrentes sin registrar.

El análisis cubre el receptor nominal y los alias que rastrea el modelo actual;
no demuestra seguridad para receptores calculados, llamadas indirectas ni
argumentos que puedan mutar el objeto. El SDK binario usado es el ya generado
en el build aislado; no se reconstruyó para esta prueba. El build incremental
del objetivo `nsc` actualizó 206 pasos a causa del encabezado compartido y,
como dependencia del propio compilador, compiló también
`src/Interop/CHeaderGenerator.cpp` en el build aislado. No se modificó ese
archivo ni se utilizaron carpetas de build de interoperabilidad. La evidencia
no acredita el árbol limpio: coexistieron cambios concurrentes sin registrar.

## Receptor de clase anidado

`shared-nested-method-before.log` muestra que `holder.child.Replace()` se
aceptaba mientras una vista de `holder.child.value` seguía viva. Se amplió la
comprobación de raíces existente a receptores de clase expresados mediante
miembros. Es conservadora: puede rechazar métodos de ramas independientes del
mismo objeto cuando el análisis solo identifica una raíz común.

Un primer ensayo incluyó receptores de estructura y rechazó erróneamente la
lectura `holder.child.value.Get()`; los diagnósticos se conservan en
`shared-nested-method-readonly.log` y `shared-nested-method-binary.log`. El
retorno `ref readonly` no demuestra que cualquier método sea readonly sobre su
receptor, por lo que no se usa como excepción general. El cambio final deja
pendientes los métodos de estructuras en receptores anidados.

`shared-nested-method-final.log` y `shared-nested-method-binary-final.log`
registran el rechazo del reemplazo inseguro y la ejecución de controles con otro
objeto y con `Get()` en O0/O2 para Rc y Arc, desde fuentes y con el SDK binario.
También pasaron los controles del receptor directo y de fin de ámbito. Las
llamadas mediante índices, receptores calculados, métodos de estructuras
anidadas y llamadas indirectas no quedan acreditadas por esta corrección.

## Receptor de estructura anidada

En la revisión posterior, `shared-nested-struct-before.log` muestra que el
análisis aceptaba un método mutable de estructura que reemplaza el dueño de
una vista viva. Se aplicó la misma comprobación a receptores de estructura;
un método declarado readonly queda permitido conforme a la regla existente.

Los métodos `Get()` de `Rc` y `Arc` y los de sus ayudas internas declaran ahora
readonly sobre el receptor, además del retorno `ref readonly`. La comprobación
`readonly-receiver-contract/results.json` rechazó una escritura directa dentro
de un método readonly; esta evidencia no demuestra ausencia de mutación
indirecta a través de referencias arbitrarias. Los cuerpos de Get modificados
leen el estado y no asignan al receptor. La prueba en copias temporales precedió
al cambio real (`readonly-shared-get-probe/result.log`).

Con fuentes y con un SDK binario generado en A, los casos dirigidos rechazaron
el reemplazo por método de estructura anidada para Rc y Arc y ejecutaron 42 en
O0/O2 para objetos independientes y lecturas `Get()` con vista viva. También
pasaron los controles de métodos de clase, fin de ámbito, lectura y escritura
readonly (`shared-nested-struct-source.log`, `shared-nested-struct-binary.log`,
`shared-nested-struct-controls.log`). Los programas inseguros se analizaron sin
ejecutarlos. El SDK se compiló con caché incremental, pero el conjunto no se
encontró en caché y compiló 71 fuentes; el log incluye
`lib/NativeSharp.Core/DeterministicMath.ns`, con cambios concurrentes ajenos.
El resultado no acredita una revisión limpia ni seguridad de índices, receptores
calculados o llamadas indirectas.

## Receptor indexado

`shared-index-method-before.log` reproduce que `holders[0].Replace()` era
aceptado durante una vista de `holders[0].value`. La comprobación de llamadas
consulta ahora la raíz del receptor indexado usando el seguimiento de vistas
existente. Se aproxima el arreglo completo como una unidad: el control
`method_on_other_index_conservative` confirma que también se rechaza una
llamada a `holders[1]` mientras existe una vista de `holders[0]`
(`shared-index-conservative.log`).

Para Rc y Arc, las pruebas dirigidas desde fuentes y SDK binario rechazaron el
reemplazo inseguro y ejecutaron 42 en O0/O2 con un arreglo independiente o tras
el fin del préstamo (`shared-index-method-source.log` y
`shared-index-method-binary.log`). No se ejecutó el programa inseguro. El SDK
binario es el generado para la tanda anterior, que incluyó un archivo ajeno
con cambios sin registrar. La evidencia no acredita indexadores de usuario,
receptores calculados sin raíz, llamadas indirectas ni mutaciones por argumentos.

## Llamada implícita o explícita en this

Los controles `this-method-before-*` confirman que `Replace()` y
`this.Replace()` eran aceptados dentro del objeto que tenía una vista viva de
su campo, aunque el método reemplazara el dueño. El análisis de llamadas
considera ahora la raíz del receptor actual y rechaza esas llamadas de instancia
potencialmente mutadoras. La aproximación es conservadora y puede rechazar un
método de clase que no modifica el campo prestado.

Las pruebas dirigidas rechazaron ambas reproducciones para Rc y Arc y
ejecutaron 42 en O0/O2 después de terminar la vista, tanto desde fuentes como
con el SDK binario (`shared-this-method-source.log`,
`shared-this-method-binary.log`). Una llamada estática independiente también
ejecutó 42 (`shared-this-method-static.log`). Los programas inseguros solo se
analizaron. El SDK binario es el generado para una tanda anterior y contiene un
archivo ajeno con cambios sin registrar; la evidencia no certifica árbol limpio
ni cubre llamadas indirectas, argumentos mutables o alias arbitrarios.

## Objeto entregado como argumento

Los logs `argument-before-*` muestran que el análisis aceptaba pasar un objeto
con vista de campo activa a `Replace`, tanto por valor como con parámetro
`borrowed`; el método podía reemplazar el dueño. En llamadas seleccionadas se
comparan ahora las raíces rastreadas de argumentos de tipo referencia contra
las vistas activas. Sin un contrato de efectos comprobado se considera que la
función puede mutar el objeto, lo que también puede rechazar funciones que
solamente lo leen.

Para Rc y Arc, desde fuentes y con SDK binario, las pruebas rechazaron los
parámetros por valor, prestados, mediante alias y nombrados en distinto orden; los controles de objeto
independiente y fin de ámbito ejecutaron 42 en O0/O2
(`shared-argument-final.log` y `shared-observer-binary.log`). Los casos
inseguros solo se analizaron. El SDK binario reutilizado incluye cambios ajenos
sin registrar y no acredita un árbol limpio. No se demuestra la seguridad de
delegados, parámetros de estructuras, estado global ni fuentes calculadas sin
raíz rastreada; las llamadas de extensión no tienen una prueba específica.

## Argumento de delegado administrado

La invocación `action(holder)` se analizaba por una ruta distinta de las
llamadas seleccionadas y aceptaba el objeto con una vista viva, aunque el
delegado pudiera reemplazar el campo (`shared-delegate-before.log`). La ruta de
delegados administrados aplica ahora la misma comparación conservadora de
raíces a argumentos de referencia. Puede rechazar delegados que solo leen.

Para Rc y Arc, desde fuentes y SDK binario, la reproducción se rechaza y los
controles con otro objeto o después del préstamo ejecutan 42 en O0/O2
(`shared-delegate-source.log` y `shared-delegate-binary.log`). También pasó el
control público `safe`; los programas inseguros no se ejecutaron. El SDK
reutilizado contiene un archivo concurrente sin registrar, por lo que la
evidencia no certifica un árbol limpio. Los eventos, punteros de función,
delegados externos y argumentos calculados sin raíz no quedan acreditados por
esta corrección; la ruta de interoperabilidad no se modificó.
