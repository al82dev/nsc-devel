# Bloque 1 -- Ownership soundness: sonda, arreglos y cierre

Ronda completa (Fases A, B y C en un solo bloque, como pidio Alberto) sobre los 10 items del
Bloque 1 de la auditoria externa.

## Por que se sondeo antes de implementar

Stage 50 (Ownership Types) figura como cerrado, pero el propio roadmap dice "pendiente verificacion
directa contra codigo", y la auditoria de la suite de tests mostro que esa area esta cubierta
**exclusivamente por asserts de forma de IR**: `Stage50_8OwnershipIrLoweringTests.cpp` aserta que
los cleanups de parametros `owned` "reutilizan la maquinaria de cleanup excepcional de Win64"
buscando texto en el IR; `Stage49RaiiMoveSemanticsTests.cpp` congela conteos literales de owners en
HIR/MIR; `Stage50_1OwnershipSyntaxTests.cpp` aserta que un campo `owned` **parsea**.

Ninguna de esas cosas puede detectar que un objeto no se destruye nunca. Implementar a ciegas sobre
un area asi es exactamente como se llega a "dos fixes previos con comentarios afirmando ser la
causa raiz, sin haber corrido nada".

Asi que primero se midio: sondas ejecutables (PE real bajo Wine, `-O0` y `-O2`) que **cuentan
destrucciones**, mas sondas de diagnostico para los items que deben ser errores de compilacion.

## Estado real encontrado (Fase A)

| Item | Estado medido |
|---|---|
| 1. owning field storage | **roto** -- declarable, inasignable (arreglado en esta ronda) |
| 2. owning array-element storage | **funciona** (modelo manual `delete`/`delete[]`) -- ver correccion mas abajo |
| 3. move-only transfer rules | ok |
| 4. moved-from state | ok |
| 5. use-after-move error | ok |
| 6. copy-owner error | **roto** -- aceptado para locals `owned` |
| 7. return ownership | **roto** -- no compila |
| 8. destruction traversal | ok para locals; en contenedores implementada en esta ronda |
| 9. ownership-aware indexers | limitacion de forma de API, ya documentada por la libreria |
| 10. negative compile tests | parcial (5 si, 6 no) |

Y un hallazgo que no estaba en la lista: **un local `owned` se declaraba, compilaba, corria y nunca
se destruia**.

## Defectos arreglados (Fase B)

### 1. Un local `owned` se perdia en silencio

`owned T x = new T()` parseaba, chequeaba tipos, compilaba y corria -- y el objeto **nunca se
destruia**. Causa: el calificador `owned` sobre un local se calculaba en analisis semantico (en
`declaredOwnership`) y se **descartaba**; solo se consultaba su hermano `Borrowed`. El local
quedaba tratado como un binding ordinario sin propiedad.

Fuga silenciosa de una caracteristica declarada del lenguaje, con `raii` funcionando correctamente
en la misma posicion -- que es justo lo que hacia el hueco invisible: las dos grafias estaban
documentadas, solo una poseia algo.

### 2. Copiar un owner `owned` estaba permitido

`owned T b = a;` era **aceptado**, creando dos owners de un mismo objeto -- un agujero de
double-free. El programa identico con `raii` siempre habia sido rechazado.

### 3. Un local `owned` no servia como fuente de `move(...)`

Con lo cual el propio ejemplo de sintaxis del proyecto
(`owned Resource local = new Resource(); return move(local);`, en `Stage50_1OwnershipSyntaxTests`)
parseaba pero era rechazado por el analisis semantico. No se podia devolver propiedad desde una
funcion que usara la grafia `owned`.

**Los tres se arreglan con un cambio**: enrutar el calificador `owned` de un local por la misma
bandera `isRaii` que ya gobierna todo lo demas -- es esa bandera la que abre la region de cleanup
(destruccion al salir de scope), la que admite un local como fuente de `move`, la que habilita el
rechazo de copia y la que maneja el estado moved-from.

Con un alcance deliberadamente mas angosto que el de `raii`: `owned` hace propietario al local solo
para los tipos que realmente poseen una asignacion separada (referencia a clase/interfaz o string
nativo), y sigue siendo el no-op que siempre fue para el resto. **Esa es la misma regla que ya
siguen los parametros `owned`** (`requiresOwnedDestruction`: "un value type no posee ninguna
asignacion de heap separada que `owned` deba liberar"); hacer que las dos grafias discrepen seria
lo sorprendente. Y mantiene compilando el idiom `owned T[] scratch = new T[1]` de la propia
libreria (`LinkedList<T>.AddFirst`), que usa el array solo como buffer de reubicacion.

### 4. Move-assignment sobre un owner vivo perdia el objeto movido -- defecto propio

`a = move(b)` destruia correctamente el valor viejo de `a` y despues **nunca destruia** el valor
movido adentro. Este era **mio**: el flag "ya destruido" por slot que habia introducido para el bug
de EH B-006. Un slot aloja legitimamente una **secuencia** de owners a lo largo de su vida, asi que
un flag de una sola via sobre el slot tiene la forma equivocada -- destruir el primer valor lo
dejaba puesto para siempre.

El arreglo correcto no fue parchear el flag sino **eliminarlo**: B-006 ya se habia arreglado
*estructuralmente* despues (el catchswitch unwindea to caller y los cleanups que encierran corren
explicitamente en el camino "ninguna clausula matcheo"), lo que elimino la doble entrada en su
origen y dejo al flag sin nada que proteger. Verificado desactivandolo y remidiendo: B-006 sigue
arreglado sin el. El estado nulo del propio slot del owner es el guard correcto, precisamente
porque guardar un owner nuevo lo re-arma solo.

Efecto secundario: el compilador quedo mas simple (se fueron `cleanupGuards_`,
`ensureCleanupGuard`, `markGuardAccessVolatile` y cuatro bindings de la C API de LLVM).

## Cobertura nueva (Fase C)

`tests/unit/Stage50_11OwnershipExecutionTests.cpp` -- 15 escenarios, 50 checks, **-O0 y -O2**
(despues de la ronda 34: 42 escenarios, 128 checks):

- destruccion exactamente-una-vez de locals `owned` y `raii` (y que las dos grafias coincidan);
- fuente moved-from que no vuelve a destruir;
- cadena de move a traves de tres parametros `owned`;
- return de propiedad desde un local `owned`;
- move-assignment sobre un owner vivo, codificado como `tag*10 + count` para que **212 = correcto,
  211 = fuga del objeto movido, 112 = sobrevivio el objeto equivocado** sean tres numeros distintos
  y no "fallo";
- orden de destruccion inverso, observado por ejecucion y no leyendo MIR;
- la mitad negativa: copia de owner (`owned` y `raii`), use-after-move (ambas grafias), y
  `owned local requires an initializer` -- este ultimo verifica ademas que el diagnostico **nombre
  la grafia que el usuario escribio**, porque reportar "raii local" para una declaracion `owned`
  manda a mirar la palabra clave equivocada.

## Item 1 (owning field storage) -- ARREGLADO en esta misma ronda

Un campo `owned` era declarable e inasignable: la sintaxis parseaba (Stage 50 lo aserta), pero
ningun destino de asignacion que no fuera un local RAII o un elemento de array admitia un `move`,
asi que nunca se le podia poner nada.

Cerrado en tres partes:

1. **Transferencia**: un campo `owned` es ahora destino valido de `move(...)`.
2. **Travesia de destruccion**: la entry de destruccion generada libera los campos que el tipo
   posee, recorriendo la cadena de herencia (base primero), cada uno exactamente una vez y
   guardado por su estado nulo. Los offsets se resuelven en el frontend
   (`ProjectOopMirDispatchTable::ownedFields`), como todo acceso a campo en este pipeline -- el
   backend no consulta el modelo de layout.
3. **Sobreescritura**: mover a un campo que YA posee libera el valor desplazado. Sin esto el
   overwrite perdia el valor viejo en silencio (medido: 201 en vez de 212). La regla no se invento:
   es la que `a = move(b)` sobre un local siempre siguio.

Seis escenarios nuevos, por ejecucion real en `-O0` y `-O2`.

## CORRECCION -- el caso de los arrays estaba mal reportado

Una version anterior de este documento y del bug doc afirmaba que mover un owner a un elemento de
array **"pierde el objeto en silencio"**, clasificandolo como defecto P0 del compilador. **Era
incorrecto y queda retractado.**

Los arrays son de gestion **manual por diseño** (`new[]`/`delete[]`, `delete` por elemento -- que es
exactamente lo que hace `~List()`). Con la descarga que el modelo pide, el conteo es exacto en -O0 y
-O2. Mi sonda simplemente nunca descargaba. El item 2 **funciona**.

Lo anoto en vez de reescribirlo en silencio porque el error es justo el que este proyecto viene
corrigiendo en comentarios ajenos: afirmar una causa sin haber ejecutado el caso completo.

## Lo que queda abierto

> **ACTUALIZACION (misma fecha, ronda siguiente)**: el item 9 y el item 10 quedaron cerrados, y al
> cerrarlos aparecio un P0 preexistente (copiar un parametro `owned` en un store era un double
> free aceptado por el compilador). Ver `claude/34_bloque1_cierre_item9_y_copy_owner.md`. Lo que
> sigue en esta seccion se conserva como el estado que tenia esta ronda.

**Item 9 -- indexers**: los setters de propiedad/indexer tienen un parametro `value` implicito que
no puede declararse `owned`, asi que no se puede expresar un setter que tome propiedad. Por eso
`List<T>` expone el indexer solo de lectura y escribe con `SetAt(int, owned T)`. Es una limitacion
de forma de API documentada, no un bug encontrado aca.

*(Resulto ser mas angosto que eso: el `value` implicito **ya** era `owned`, porque hereda el
`TypeSyntax` de la propiedad. Lo que bloqueaba la declaracion era el **getter**, forzado a devolver
propiedad. Ver doc 34.)*

**Alias plano hacia un slot que descarga** (verificado por ejecucion): una referencia PLANA (no
owner) asignada a un slot que despues se descarga se acepta **sin ningun chequeo de transferencia**,
y queda colgando. Desde un owner la misma asignacion si se rechaza. La libreria lo nombra
explicitamente como "a genuine gap in the current compiler's array-assignment ownership checking".
Es el hueco de soundness real que queda en esta area -- y sigue abierto despues de la ronda 34.

## Verificacion

- Build completo con `-DNSC_WARNINGS_AS_ERRORS=ON`: limpio.
- `ctest` completo: **230/230, 100%**.
- Suites de ejecucion real: ownership 50 checks, EH cleanup 120 checks, EH raii/reuse 130 checks.
  *(La suite de ownership crecio despues a 128 checks en la ronda 34.)*

## Recomendacion

La siguiente ronda natural no es Bloque 2 sino cerrar **ownership en slots de contenedor** como
bloque propio, empezando por la regla de escape (que es la decision de diseño) y no por el lowering.

*(Se hizo: doc 34. Lo unico que sobrevive de esa recomendacion es la regla de escape para el alias
plano, que sigue siendo decision de API de Alberto.)*
