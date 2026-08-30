# Bloque 1 -- cierre: items 9 y 10, y un double free P0 encontrado por el camino

Continuacion directa de `33_bloque1_ownership_soundness.md`, que dejo abiertos el item 9
(indexers/propiedades con ownership) y el item 10 (tests negativos, "parcial: 5 si, 6 no").
Los dos quedan cerrados aca. Por el camino aparecio un P0 preexistente.

## Item 9 -- `owned` sobre propiedades e indexers

### Que estaba roto y por que era invisible

`public owned Res Slot { get {...} set {...} }` no se podia escribir. Dos bloqueos, uno detras del
otro:

1. **El getter**: `error: owned return requires an ownership-transfer expression such as
   move(owner), new T(...), or an owned-returning call`. El parser le da al accesor `get` el
   `TypeSyntax` de la propiedad como tipo de retorno, asi que un `owned Res Slot` llegaba al
   analisis como un metodo con retorno `owned`, y **ningun getter correcto puede satisfacer eso**:
   lo unico que podria mover afuera es el campo que el objeto sigue poseyendo y sigue destruyendo.
2. **El call site**: `h.Slot = move(a)` -> `error: move(...) is only valid for a direct RAII local
   or owned parameter...`.

La libreria ya lo tenia documentado como limitacion de forma de API. Lo que **no** estaba dicho es
que el setter ya funcionaba: su parametro implicito `value` hereda el `TypeSyntax` de la propiedad,
asi que ya era un parametro `owned` de verdad y `backing = move(value)` siempre compilo dentro del
accesor. Verificado aislando el caso (un getter que devuelve `new Res(9)` hace compilar el setter
limpio). Faltaba solo el call site -- y el getter, que bloqueaba la declaracion entera.

### La decision de diseño

`owned` sobre una propiedad/indexer es **asimetrico**, y esa asimetria es la decision, no un
compromiso:

- el **setter toma propiedad** (`h.Slot = move(a)` transfiere de verdad: la fuente queda
  moved-from);
- el **getter entrega una referencia plana no-owner** (`Res p = h.Slot` da un alias; leer no
  transfiere).

  > **PRECISION (auditoria, §4/§41)**: una version anterior de este documento decia "el getter
  > presta". Es impreciso y queda corregido: el retorno del getter NO es un `borrowed` formal --
  > un borrow en este lenguaje lleva provenance de lifetime probada contra un owner vivo, y una
  > lectura de campo no la tiene. Es la tercera categoria de referencia del lenguaje:
  > plana/raw, no-owner, sin lifetime chequeado. Un alias que sobrevive a su contenedor **queda
  > colgante por diseño**, como una referencia cruda de C++; no debe presentarse como borrow
  > safety.

Un getter que entregue propiedad no es una limitacion de lowering, es **insano**: el objeto seguiria
teniendo ese puntero en un campo `owned` que su entry de destruccion libera. Dos owners, una
asignacion, double free. Y la unica alternativa -- vaciar el campo al leer -- haria que una lectura
mutara el objeto, que no es lo que un getter es.

O sea: `owned` describe **donde vive el almacenamiento**, no lo que hace cada accesor con el. Y no
es una forma inventada: es exactamente la que la libreria adopto a mano mientras el calificador era
indeclarable (`List<T>` expone indexer de solo lectura + `SetAt(int, owned T)`). El item 9 permite
expresar esa forma en una declaracion en vez de dos miembros.

Se eligio dejar el retorno del getter **sin calificar** y no `borrowed`: un return prestado exige
una vinculacion de lifetime probada contra un owner vivo, y una lectura de campo no la tiene.
Medido: `borrowed Res Peek() { return backing; }` se rechaza.

### Implementacion

- `ClassMemberSymbol::ownsAccessorStorage` -- marcado en los **dos** accesores. En el getter, en el
  mismo punto, `declaredOwnership` se neutraliza a `None`. En el setter se deriva del ownership del
  ultimo parametro (el `value` implicito va siempre al final, despues de los indices).
- `analyzeAssignment`: la busqueda del binding del getter se **adelanta** a antes de analizar el
  RHS. Antes vivia despues, que era el problema exacto -- para cuando algo sabia que habia un
  setter, el `move(...)` ya habia sido rechazado. Se guarda como **indice**, no como puntero:
  analizar el RHS puede reallocar `calls_`.
- Propiedad automatica: `{ get; set; }` sobre `owned` genera `backing = move(value)`, no
  `backing = value`.

### Verificacion

Ejecucion real, PE bajo Wine, `-O0` y `-O2`, contando destrucciones: propiedad explicita,
propiedad automatica, indexer, getter que entrega referencia plana (tres lecturas -> **una** destruccion),
sobreescritura que libera lo desplazado. Mas cuatro negativos: copia sin `move`, use-after-move,
getter que no se puede lavar en owner (`raii Res r = h.Slot;` rechazado), y `move` sobre una
propiedad **no** calificada (para que el ensanchamiento no se derrame a todos los setters del
lenguaje).

Por sonda tambien: generico cerrado sobre tipo de valor y sobre tipo de referencia, y
virtual/override.

### Efecto lateral: campo `static owned` rechazado en la declaracion

Un `static owned` no tiene contenedor que muera y nada corre al salir del proceso: solo podia ser
una fuga con anotacion. Antes se aceptaba y el error salia recien en el primer `move`, apuntando a
la transferencia y sin mencionar el `static` -- mandando a corregir la linea equivocada. Ahora el
diagnostico nombra la causa en la declaracion.

## P0 encontrado: copiar un parametro `owned` en un store era un double free

Detalle completo en `BUGS_ENCONTRADOS/bug_copia_de_parametro_owned_en_store_doble_free.md`.
Resumen:

`slot = value` con `value : owned T` **compilaba**. El cleanup del parametro destruia el objeto al
salir del metodo y el contenedor lo volvia a liberar despues. Medido sobre el fixture `Box<T>` de
Stage 52 hecho ejecutable: **4 destrucciones de 2 objetos**, `-O0` y `-O2` por igual; con `move`,
2.

La regla ya existia para todos los destinos **menos uno**:

| origen -> destino | antes |
|---|---|
| local `raii`/`owned` -> campo | rechazado |
| parametro `owned` -> local `raii` | rechazado |
| parametro `owned` -> argumento `owned` | rechazado |
| **parametro `owned` -> store** | **ACEPTADO** |

Sobrevivio porque cinco fixtures de `Stage52GenericCollectionsCompilerFixesTests.cpp` tenian esa
forma exacta y las cinco pasaban asertando *"lowers to OOP MIR"*. Y porque
`Stage52_4OwnedCleanupRuntimeTests.cpp` tenia un escenario dedicado cuyo comentario afirmaba que el
bug "no es algo que el analisis semantico pueda rechazar en general", y asertaba que el programa
debia seguir compilando.

Esa premisa era incorrecta, por un problema de encuadre: se leyo el peligro como *"una asignacion a
elemento de array"* (que si es legal) en vez de *"un parametro `owned` como lado derecho de un
store"* (que crea dos owners sea cual sea el destino).

**Barrido**: 97 proyectos `.nsproj` compilados con la regla activa. La **libreria esta limpia** --
los siete call sites de Stage 52.4 ya usaban `move`. Los unicos hits fueron los cinco fixtures. El
bug estaba en el compilador y en los tests, no en la libreria: la libreria se habia arreglado a mano
y nada impedia volver a escribirlo mal.

**Como se encontro**: no leyendo codigo, **mutando un test**. El comentario del escenario de la
propiedad automatica iba a afirmar que sin `move` el conteo daria 22 en vez de 21. Antes de dejarlo
escrito se lo midio, dio 22, y eso obligo a preguntar por que `set { backing = value; }` compilaba.

## Item 10 -- la mitad negativa del contrato de ownership

Cerrado. Once reglas que estaban implementadas y **asertadas por nada mas que la implementacion que
las aplica** ahora tienen escenario propio, cada uno asertando el **mensaje** y no solo el rechazo
(un rechazo por otra razon -- un error de parseo, un diagnostico ajeno -- no es la regla
funcionando, y un assert que no distingue eso sigue pasando mientras la regla se pudre):

campo `borrowed`; return prestado que sobrevive a su owner local; valor prestado guardado en un
campo; valor prestado guardado a traves de un setter; devolver un owner `raii` sin `move`; devolver
un parametro `owned` sin `move`; self move-assignment; sobreescribir un owner con un prestamo vivo;
return `owned` sin expresion de transferencia; valor prestado escapando por un return sin calificar;
`move` desde un local plano.

Mas los cinco del item 9 y el del `static owned`.

## Estado de la suite

`tests/unit/Stage50_11OwnershipExecutionTests.cpp`: **43 escenarios, 128 checks** al cierre de
esta ronda.

> **CORRECCION (auditoria, §10/§41)**: una version anterior decia 42 escenarios. El `main()`
> registraba 43; el conteo de checks (128 = 21 runtime x 4 + 22 negativos x 2) si era correcto.
> Tras la ronda 35 (cierre de owning slots) la suite queda en **57 escenarios / 176 checks**.

- Build completo con `-DNSC_WARNINGS_AS_ERRORS=ON`: limpio.
- `ctest` completo: **230/230, 100%**.

## Nota de proceso -- un binario de test truncado

Durante esta ronda `ctest` reporto `nsc.stage43.lambdas-closures` como `[permission denied]`. El
binario estaba en **0 bytes**: un `cmake --build` anterior habia sido matado a mitad del link. Vale
anotarlo porque el modo de falla siguiente era peor que el error: darle `chmod +x` a un ejecutable
vacio lo hace "pasar" (corre nada, devuelve 0). Se relinkeo y se barrio el arbol de build entero
buscando otros archivos de 0 bytes -- ninguno. Es el mismo error de confiar en verde que ya habia
pasado en este proyecto con un ctest stale.

## Lo que queda abierto en esta area

Uno solo, y es de diseño, no de implementacion:

**Alias plano hacia un slot que descarga.** Una referencia PLANA (no owner) asignada a un slot que
despues se descarga se acepta sin ningun chequeo, y queda colgando. Desde un owner la misma
asignacion se rechaza. La libreria lo nombra explicitamente como *"a genuine gap in the current
compiler's array-assignment ownership checking"*. Endurecerlo rompe `AddRange`/`InsertRange` tal
como estan escritos, asi que es una decision de API de Alberto y no un fix que me corresponda hacer
solo.

**Y una capacidad nueva sin usar**: ahora se podria escribir
`public owned T this[int i] { get {...} set {...} }` en `List<T>` y quedarse con
`list[i] = move(item)`. Eso cambia la superficie publica de la libreria. La capacidad esta; el
cambio de API espera decision.
