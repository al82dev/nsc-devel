# Alias prestado de un campo de solo lectura

Un `borrowed` obtenido de `Rc.Get()` o `Arc.Get()` puede contener un campo de
clase que también debe conservar el préstamo y el solo lectura. Antes del cambio,
`borrowed Child child = parent.Item` se rechazaba incluso cuando `parent` y su
dueño seguían vivos (`A:/nsc-fixes-evidencia/arc-readonly-nested-local-probe-before.log`).
La lectura directa `parent.Item.Data` ya era aceptada. El análisis del acceso al
campo ya marcaba el resultado como prestado y de solo lectura; la declaración
local exigía, sin embargo, que el inicializador fuera un identificador o una de
las llamadas especiales admitidas.

El ensayo en `analyzeBorrowedVariableInitializer` acepta el campo de un local
prestado identificado, siempre que el acceso sea de solo lectura y el local
tenga un vínculo de préstamo válido. El nuevo local conserva el dueño del
receptor y limita su región a la del propio receptor. No extiende esta regla a
propiedades, temporales ni llamadas arbitrarias. Los cambios existentes de ese
archivo y del análisis de vistas permanecen sin registrar; este ensayo depende
del árbol conjunto y aún no constituye un commit aislado.

La compilación incremental `readonly-nested-local-build.cmd` terminó con código
0. En la matriz `readonly-nested-local-regression-expanded.log`, Rc y Arc leen
42 en O0/O2; se rechazan la escritura, la llamada mutadora, el reemplazo del
dueño, la reasignación del receptor seguida de reemplazo, la escritura global y
el retorno del alias. Pasaron también controles de mutación y de vistas en
tuplas. El primer intento de la matriz tuvo una colisión del nombre `Child` con
el fixture común y se corrigió el nombre del caso nuevo; el fallo original está
en `readonly-nested-local-regression-first-failed.log`.

Con el Core binario aislado, `readonly-nested-local-binary.cmd` comprobó la
lectura válida en O0/O2 y los rechazos de escritura y llamada mutadora, además
de los controles previos. Tres pruebas existentes de resúmenes de escape y
vida de préstamos pasaron 3/3 tras recompilar sus objetivos incrementalmente
(`readonly-nested-local-controls-configured.log`). La primera ejecución de esos
controles omitió el runtime aislado y falló 1/3 por buscar una instalación
distinta; quedó en `readonly-nested-local-controls.log` y no cuenta como éxito.

La evidencia cubre esos casos, no todos los alias, reasignaciones o tipos de
campos. El compilador enlazado contiene otros cambios provisionales y el Core
binario procede del prototipo documentado en `arc-send-sync-verificacion.md`.
No se ejecutó una batería completa ni se modificó interoperabilidad C/C++.

## Campo directo de un resultado ligado al receptor

La forma `borrowed Child child = owner.Get().Item` también se rechazaba antes
por exigir un inicializador directo (`readonly-nested-direct-before.log`). El
ensayo se amplió a un campo real de una llamada de instancia cuyo retorno es
por referencia y declara su vida ligada a `this`. El receptor debe ser un local
move-only y coincidir con el dueño que el análisis ya atribuye al resultado;
el campo debe conservar el marcador de solo lectura. Así se conserva el dueño
como origen del nuevo préstamo. La regla no admite un receptor temporal ni
generaliza a propiedades o llamadas sin ese contrato.

Después de la compilación incremental `readonly-nested-direct-build.cmd`, la
selección `readonly-nested-direct-regression-expanded.log` pasó para Rc y Arc:
lectura válida en O0/O2 y rechazo de mutación, llamada mutadora, dueño temporal,
reemplazo del dueño, almacenamiento global y retorno. Los controles de la
forma con local intermedio también pasaron. Con el Core binario aislado,
`readonly-nested-direct-binary.log` registra lectura de ambas formas en O0/O2
y rechazo de mutación y llamada mutadora del alias directo. Tras actualizar
incrementalmente sus objetivos, los tres controles existentes de vida y escape
pasaron (`readonly-nested-direct-controls.log`, 3/3).

Estas pruebas no demuestran todas las procedencias, combinaciones con alias o
los contratos de bibliotecas binarias que contienen vistas compuestas. El
compilador sigue incluyendo cambios locales no registrados y el ensayo aún no
se ha separado en un commit verificable sobre árbol limpio.

## Campo a través de una vista local

El acceso `borrowed Child child = view.Get().Item`, con `view` obtenida de un
dueño local, se rechazó antes del ajuste (`readonly-nested-view-before.log`).
El ensayo lo admite cuando la vista conserva exactamente un origen local
identificable, coincidente con el dueño atribuido al resultado, y limita el
alias a la vida de la vista. Las procedencias múltiples o desconocidas se
mantienen conservadoras. El cambio sigue en el árbol provisional.

El control con otro dueño homónimo en un ámbito interior mostró un rechazo
indebido (`readonly-nested-view-shadow.log`): `CallAnalyzer.cpp` resolvía el
nombre de la raíz en el ámbito de la llamada. Para un receptor que es un
identificador de vista local, el ensayo resuelve esa raíz en el ámbito donde se
declaró la vista. Se conservó la resolución anterior para otros receptores.

Tras las compilaciones incrementales `readonly-nested-view-build.cmd` y
`readonly-nested-view-shadow-build.cmd`, la selección
`readonly-nested-view-final-source.log` pasó con Rc y Arc: lectura válida y
lectura con dueño homónimo en O0/O2, rechazo de reemplazar el dueño real,
mutación, almacenamiento global, retorno y el ejemplo de procedencia múltiple.
En la lectura con sombra se reemplaza el dueño independiente y se obtiene 42.
`readonly-nested-view-final-binary.log` confirma la lectura con Core binario en
O0/O2 y el rechazo de mutación; los tres controles existentes de préstamos
pasaron (`readonly-nested-view-final-controls.log`). Los registros están en A.
Esta evidencia no demuestra todos los alias, reassignaciones, fuentes múltiples
ni bibliotecas binarias con contratos de vistas compuestas. El compilador y SDK
incluyen cambios locales provisionales y aún no hay commit aislado.

## Selección entre dueños locales

Una selección condicional entre dos vistas con ambos dueños vivos se rechazaba
al extraer el campo anidado (`readonly-nested-multiple-before.log`). El ensayo
ahora resuelve cada origen registrado en el ámbito de la vista y transfiere al
préstamo todos los ordinales de dueño, no solo el primero. Se acepta únicamente
si todos los orígenes son locales move-only identificables, sobreviven a la
vista y el dueño principal coincide con el origen atribuido al resultado. Los
orígenes desconocidos, parámetros y campos siguen sin acreditarse por esta vía.

Tras `readonly-nested-multiple-build.cmd`, la selección dirigida pasó con Rc y
Arc en O0/O2 para ambas ramas; los intentos de reemplazar cualquiera de los
dos dueños se rechazaron (`readonly-nested-multiple-regression.log`). Una vista
predeterminada sin dueño identificable también fue rechazada para ambas familias
(`readonly-nested-multiple-unknown.log`). Con el Core binario aislado, la lectura
de la segunda rama devolvió 42 en O0/O2 y los dos intentos de reemplazo se
rechazaron (`readonly-nested-multiple-binary.log`). Tres controles existentes
de duración y escape pasaron (`readonly-nested-multiple-controls.log`, 3/3).

Esto no acredita todas las rutas por las que se puede construir o reasignar una
vista, ni el contrato de un binario que recibe vistas compuestas. El ensayo se
probó con el árbol conjunto y sigue sin commit aislado.

## Dueño recibido por valor

Con un `Rc` o `Arc` recibido por valor, la lectura de un campo anidado se
rechazaba por no reconocer el parámetro como fuente del préstamo
(`readonly-nested-parameter-before.log`). El ensayo conserva ahora el ordinal
del parámetro move-only en el resultado por referencia ligado al receptor y lo
propaga al acceder al campo. Se forma el préstamo tanto con `owner.Get().Item`
como con una vista local de origen único `view.Get().Item`. La segunda forma
queda limitada a la vida de la vista. No se habilitan por esta regla parámetros
por referencia ni vistas de procedencia mixta o desconocida.

Después de la compilación incremental `readonly-nested-parameter-build.cmd`,
las dos lecturas ejecutaron 42 en O0 y O2 para Rc y Arc; la escritura, el retorno
y el reemplazo del parámetro se rechazaron en ambas formas
(`readonly-nested-parameter-regression-expanded.log`). Los controles de vistas
con dueños locales, múltiples y nombres sombreados también pasaron en esa
selección. Las lecturas directa y mediante vista ejecutaron 42 en O0 y O2 con
Core binario (`readonly-nested-parameter-binary.log`); tres controles existentes
de préstamos pasaron tras reconstrucción incremental de sus objetivos
(`readonly-nested-parameter-controls.log`).

La propagación del ordinal añade un cambio en `MemberAccessAnalyzer.cpp`; este
ensayo continúa sin commit y se probó con el árbol conjunto. No acredita todos
los alias ni la procedencia mixta entre parámetros y locales, ni modifica los
límites de consumo binario de vistas compuestas.

## Procedencias mixtas

La selección entre un dueño recibido por valor y un dueño local se rechazaba
aun si ambos seguían vivos (`readonly-nested-mixed-before.log`). El ensayo
combina ahora los ordinales locales y de parámetros en los conjuntos existentes
del préstamo. Cada parámetro debe ser move-only y pasarse por valor; cada local
debe ser move-only y sobrevivir a la vista. Un origen desconocido mantiene el
rechazo. La raíz principal debe coincidir con la procedencia del resultado de
la llamada, y todos los orígenes se resuelven en el ámbito de la vista.

Tras `readonly-nested-mixed-build.cmd`, los casos con parámetro primero, local
primero, segunda rama y dos parámetros ejecutaron 42 en O0/O2 para Rc y Arc.
Los intentos de reemplazar cualquiera de los dueños y de devolver el alias se
rechazaron (`readonly-nested-mixed-final-source.log`). La lectura mixta con
Core binario ejecutó 42 en O0/O2 (`readonly-nested-mixed-final-binary.log`),
y tres controles existentes de vida y escape pasaron después de compilar sus
objetivos incrementalmente (`readonly-nested-mixed-final-controls.log`).

El caso binario anterior verifica lectura, no una biblioteca que publique una
firma con vista compuesta. Los cambios permanecen en el árbol conjunto y no
constituyen todavía un commit aislado ni evidencia de todas las procedencias.

Los controles `readonly-nested-alias-scope.log` añadieron una copia del alias
de procedencia mixta. La lectura con ambos dueños vivos devolvió 42 en O0/O2
para Rc y Arc; reemplazar cualquiera de los dos dueños y devolver la copia se
rechazaron. Un intento de asignar a un local exterior un alias cuyo dueño y vista
están en un ámbito interior también se rechazó; el control que mantiene el
dueño vivo devolvió 42. La selección no ejecutó accesos inválidos y no acredita
todas las formas de reasignación.

## Reasignación de la vista

Se reprodujo que una vista obtenida de un `Rc` o `Arc` no podía reasignarse a
otro dueño aunque ambos siguieran vivos (`readonly-view-reassignment.log`). Un
primer ensayo que eliminaba la restricción sin más pasó la selección de Rc y
Arc, pero dejó mover el segundo dueño de una vista construida mediante
`BorrowedReference`: se observó la ausencia del diagnóstico esperado. Los
extractos de esa salida y del primer intento condicional se conservaron en
`readonly-view-reassignment-first-observed.txt` porque los logs originales
fueron reutilizados. No se ejecutó el acceso inválido.

El ensayo actual admite el cambio de dueño solo si todas las fuentes acumuladas
identifican locales move-only o parámetros move-only por valor, con la misma
identidad en los ámbitos de la vista y de la asignación. La fuente desconocida
del constructor vuelve a rechazarse. Se conserva la primera raíz y la unión de
fuentes: puede rechazarse conservadoramente una operación aunque el dueño ya no
sea el de la vista actual. No se afirma que la regla resuelva todas las formas
de vistas o alias.

`readonly-view-reassignment-expanded.log` pasó los casos de ambos tipos de
puntero: lectura antes y después de la reasignación en O0/O2, protección del
dueño original y del nuevo, escape de ámbito y uso con parámetro. También
pasaron los dos casos de selección condicional en
`readonly-view-reassignment-conditional.log`, los controles de Core binario en
`readonly-view-reassignment-binary.log` y tres controles dirigidos de vida y
escape en `readonly-view-reassignment-controls.log`. Una selección condicional
que vuelve a llamar `Borrow()` sobre el primer dueño se rechazó antes por una
regla conservadora separada; el extracto se conservó en el archivo indicado y
no se considera corregido.

Inicialmente `nsc.ns3.t31.array-borrow` falló una de sus 13 comprobaciones:
el ejecutable devuelve 5 cuando espera observar dos destrucciones de objetos
en un arreglo. La misma comprobación falló en el registro anterior
`A:/nsc-fixes-evidencia/full-suite-run/unit-failures/nsc.ns3.t31.array-borrow.log`;
la comprobación nueva del constructor sí obtuvo el rechazo esperado. El código
de salida preciso se añadió al diagnóstico sin cambiar el resultado exigido.
Se identificó una contradicción entre esa expectativa histórica y la decisión
explícita de que un arreglo plano de referencias no posee sus elementos. La
prueba conserva y ejecuta el ejemplo original con su resultado observado y
verifica por separado la secuencia con un arreglo `owned`; ahora pasan 16
comprobaciones. Ver `array-plain-ownership-test.md`. La regla de reasignación
entre dueños identificables y sus controles directos se registró en
`f582cfbbb`; la extracción de campos anidados y sus demás cambios permanecen
provisionales y no representan el cierre del contrato. El compilador y Core de
prueba contienen otros cambios locales provisionales.

Actualización: la propagación de procedencia para estos campos y los 58 casos
dirigidos por familia quedaron registrados en el commit local `6767fc4d0`;
véase `readonly-nested-field-borrows.md` para el alcance y las limitaciones de
la validación. Las notas anteriores sobre el carácter provisional describen
el estado que tenían al realizarse los respectivos ensayos.
