# Part 5 Conformance — Stage 52 (cierre formal, 2026-08-28)

Este documento reemplaza la version anterior, escrita antes de que Wine 9.0 estuviera disponible
en el entorno y antes de las rondas de foreach (52.x, 52.x.2). Cada seccion marca explicitamente
si el PASS es **estructural** (compila, verifica LLVM, emite objeto COFF — nunca se corrio) o
**de ejecucion real** (se corrio el binario bajo Wine 9.0 y/o Windows real, y se comparo el
resultado contra un calculo a mano hecho ANTES de correr). Esta distincion es el pedido explicito
de Alberto en el checklist de cierre — no se resume en "PASS" generico en ningun lado de este
documento.

## Contenedores genericos como superficie de primera clase

**PASS (ejecucion real)** — `List<T>`, `Queue<T>`, `Stack<T>`, `LinkedList<T>`/
`LinkedListNode<T>`, `Dictionary<TKey,TValue>`, `HashSet<T>`, `SortedList<TKey,TValue>`,
`SortedDictionary<TKey,TValue>`, `SortedSet<T>`, `PriorityQueue<TElement,TPriority>`,
`KeyValuePair<TKey,TValue>` compilan, verifican LLVM, emiten objeto COFF Y corren de verdad bajo
Wine 9.0 (`Stage52Collections.exe`, checksum determinista 149, 3/3 corridas identicas) con `T`
valor (`int`) y `T` referencia (una clase de usuario), via monomorfizacion real. Esto reemplaza
el PASS estructural original — la ejecucion real fue la que destapo el bug real de doble-free en
`owned`/`move()` (ver seccion siguiente), invisible en la verificacion puramente estructural.

## Bug real destapado por ejecucion real (no estaba en el checklist original)

**Encontrado y corregido, verificado de nuevo por ejecucion real** — `List<T>.Add(owned T
value)` (y el mismo patron en `Queue`, `Stack`, `Dictionary`, `SortedList`, `PriorityQueue`,
`LinkedListNode` — 14 sitios en total) guardaba el parametro `owned` con asignacion plana
(`items[count] = value;`) en vez de `items[count] = move(value);`. Eso deja el storage del
parametro sin anular, asi que el cleanup automatico de owned-parameter al retornar destruye el
mismo objeto que el arreglo acababa de guardar — doble-free real, cuyo sintoma (page fault)
aparecia lejos del sitio real del bug. Bisectado en 12 pasos con ejecucion real bajo Wine en cada
uno; corregido en los 14 sitios; `HashSet`/`SortedSet`/`SortedDictionary` ya estaban correctos
(delegan con `move()`). Este es exactamente el tipo de bug que la verificacion solo-estructural
(el estado original de este documento) no puede detectar — solo aparece corriendo el binario.

## `PriorityQueue<TElement,TPriority>` y `Comparer<T>`/`EqualityComparer<T>` (Seccion 16)

**PASS (ejecucion real)** — `PriorityQueue` es un binary min-heap real sobre dos arreglos
`owned` paralelos (`elements`/`priorities`), con el mismo patron de indices 2i+1/2i+2 que la
implementacion interna de C#. Verificado con ejecucion real: orden de extraccion correcto contra
un calculo a mano, incluido en `nsc.stage52-6.foreach-collections-enumerators` (su enumerador,
agregado en la ronda de foreach) y en el ejemplo academico `tutorial_23_huffman_priority_queue`.

**Divergencia honesta, documentada (sin cambios respecto a la version anterior)**:
`Comparer<T>`/`EqualityComparer<T>` se exponen como constructores en vez de miembros estaticos —
verificado directamente contra el compilador: acceso a miembro estatico sobre una clase generica
con argumento de tipo explicito no parsea como expresion hoy.

## Metodos de alto valor agregados a los contenedores existentes

**PASS (estructural + ejecucion real parcial)** — ver `TEST_MATRIX_52.md` para el listado
completo. La mayoria de estos metodos quedan cubiertos transitivamente por la ejecucion real de
`Stage52Collections.exe`, que los ejercita a todos dentro de sus 13 funciones de checksum; los
casos runtime-observables especificos de foreach tienen su propio test de ejecucion real
dedicado (`nsc.stage52-5.foreach-semantic-gaps`, `nsc.stage52-6.foreach-collections-enumerators`).

## `foreach` (agregado en las rondas 52.x y 52.x.2, posteriores al cierre original de Stage 52)

**PASS (ejecucion real)** — `foreach` sobre los diez contenedores, con higiene de temporales
(`$__foreach_*`, colision con codigo de usuario categoricamente imposible), fuente
ownership-aware (incluyendo `foreach (var x in Factory.CreateOwnedList())`), elemento de
iteracion no-reasignable, y `Current` como propiedad get-only (no metodo). Se encontro y
corrigio, DURANTE esta ronda, un segundo bug real: `analyzeForeach` nunca cerraba la cleanup
region de la fuente cuando esta se vuelve `raii`, causando un error de compilador en cualquier
return/break/continue posterior en el mismo metodo. `nsc.stage52-5.foreach-semantic-gaps`
(24/24) y `nsc.stage52-6.foreach-collections-enumerators` (16/16), ambos con ejecucion real bajo
Wine para los casos runtime-observables, verificando ORDEN correcto por contenedor (FIFO,
tope-a-fondo, sorted, etc.), no solo "compila".

## `var` — inferencia de tipo local (bloque separado, pedido explicito de Alberto)

**PASS (estructural + ejecucion real transitiva)** — sin cambios de diseño respecto a la version
anterior de este documento. Keyword reservada, sin `parseType()`, restringida a los puntos donde
`parseVariableDeclaration` la intercepta explicitamente. `owned var`/`borrowed var` siguen sin
soportarse en v1 (limite conocido, documentado, no un descuido).

## Regresion de la suite completa

**PASS, sin gaps** — **210/210** (subio de 206/206 con las dos rondas de foreach, mas el test de
ejecucion real de owned-cleanup agregado al cerrar el bug de doble-free). Cero fallas conocidas.

## Ejecucion real en Windows (NO solo Wine) — item que este documento marcaba como bloqueado

**Actualizado 2026-08-28**: la version anterior de este documento decia "ejecucion real en
Windows (bloqueada por ausencia de entorno Windows/Wine en este sandbox)". Eso ya no es cierto —
Alberto tiene Windows real accesible y este sandbox aprendio a preparar `.exe` para que el los
corra directamente (usado con exito para confirmar, contra Windows real, que un crash que se
veia bajo Wine 9.0 no existia en Windows de verdad — era un bug de Wine, no de `nsc`; ver
`claude/13_bug_raii_trycatch_confirmado_bug_de_wine.md` en el proyecto). La suite especifica de
Stage 52 (colecciones + foreach) todavia no se corrio en Windows real como ronda dedicada — eso
se entrega en este mismo cierre como paquete de verificacion aparte (ver
`WINDOWS_REAL_CHECKLIST_STAGE52.md` / carpeta de `.exe` en el zip final).

## Host ABI / superficie previa

**PASS (sin cambios)** — esta etapa no modifica la frontera C ABI de Stage 48 ni el mecanismo
RAII/move de Stage 49/50/51. El limite de struct-value ABI > 8 bytes (Stage 40) sigue sin tocar,
deliberadamente — ver `52_nativesharp_generic_collections.md` seccion 5.

## Scope boundary

Stage 52 congela: `NativeSharp.Core.Collections.Generic` completo (12 clases genericas + 2
utilitarias + delegates) CON `foreach` real sobre las diez (agregado en 52.x/52.x.2, no en el
cierre original); `var` como inferencia de tipo local. Quedan explicitamente para etapas
futuras: interfaces `IEnumerable<T>` (foreach ya funciona via desugaring estructural, sin
necesitar la interfaz); `ref`/`out` reales; generic constraints (`where T : ...`); struct ABI >
8 bytes. La verificacion de ejecucion real en Windows YA NO esta bloqueada por el entorno — solo
falta correr la suite especifica de Stage 52 (ver seccion anterior).
