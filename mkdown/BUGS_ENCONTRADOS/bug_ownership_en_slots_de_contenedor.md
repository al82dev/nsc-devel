# Ownership en slots de contenedor -- estado real (campo / elemento de array / indexer)

Corresponde a los items 1, 2 y 9 del Bloque 1 de la auditoria externa. Sondeado por ejecucion real
el 2026-08-29.

## CORRECCION de una version anterior de este documento

Una version previa afirmaba que mover un owner a un elemento de array **"pierde el objeto en
silencio"** y lo clasificaba como defecto del compilador. **Eso era incorrecto y queda retractado.**

El error fue mio: la sonda movia el owner al array y **nunca descargaba el array**. Los arrays en
este lenguaje son de **gestion manual por diseño** -- `new[]` / `delete[]`, y `delete` por elemento,
igual que en C. Con la descarga que el modelo pide, el conteo es exacto:

```csharp
raii Res a = new Res(1);
raii Res b = new Res(2);
Res[] items = new Res[2];
items[0] = move(a);
items[1] = move(b);
delete items[0];
delete items[1];
delete[] items;
// destrucciones observadas: 2  (correcto, en -O0 y -O2)
```

Es exactamente lo que hace `~List()` en la propia libreria (`delete items[i]` en un loop y despues
`delete[] items`). Sin la descarga el objeto no se destruye -- pero eso es la misma omision que
olvidarse un `delete[]` en cualquier otro lado, no un defecto del compilador.

**Item 2 (owning array-element storage): FUNCIONA** bajo el modelo manual. La transferencia se
acepta, la fuente moved-from correctamente no vuelve a destruir, y `delete` descarga.

Anoto la retractacion en vez de reescribir el documento en silencio porque el error es
precisamente el que este proyecto viene corrigiendo en comentarios ajenos: afirmar una causa sin
haber ejecutado el caso completo.

## Lo que SI es un hueco real

### Item 1 -- campo `owned`: ARREGLADO (2026-08-29)

Era declarable e inasignable. Ahora funciona, verificado por ejecucion real en `-O0` y `-O2`
(`tests/unit/Stage50_11OwnershipExecutionTests.cpp`, seis escenarios):

- `holder.Slot = move(local)` se acepta;
- el contenedor **libera** lo que posee desde su entry de destruccion generado, recorriendo la
  cadena de herencia (un campo heredado de una base tambien se libera);
- un campo `owned` nunca asignado no intenta liberar nada (guard por estado nulo);
- el objeto sigue vivo y legible a traves del campo mientras el contenedor viva;
- **sobreescribir un campo que ya posee libera el valor desplazado**, con la misma regla que
  `a = move(b)` sobre un local siempre siguio -- sin eso el overwrite perdia el valor viejo en
  silencio (medido: 201 en vez de 212 antes del fix).

Implementacion: `ProjectOopMirDispatchTable::ownedFields` (offsets resueltos en el frontend, como
todo acceso a campo en este pipeline), travesia emitida en la entry de destruccion, y el flag de
"liberar el valor previo" reutilizando el mecanismo que el `Store` ya tenia para locals.

Lo que sigue es el texto original del hallazgo, conservado como registro:

### Item 1 -- campo `owned`: declarable e inasignable (falla RUIDOSA) [HISTORICO]

```csharp
class Holder { public owned Res Slot; }
...
h.Slot = move(a);
```

```
error: move(...) is only valid for a direct RAII local or owned parameter in RAII initialization,
       move assignment, owned argument transfer, or return
```

`public owned Res Slot;` **parsea y pasa el analisis** (el test de sintaxis
`Stage50_1OwnershipSyntaxTests.cpp` lo aserta explicitamente), pero no existe forma de poner algo
adentro. La caracteristica es declarable y completamente inutilizable. Al menos falla ruidosamente.

Para cerrarlo hacen falta dos mitades:

1. **Semantica**: admitir un campo `owned` como destino de `move(...)`, con la misma semantica de
   sobreescritura que ya tienen los locals -- destruir el valor viejo antes de guardar el nuevo
   (verificado por ejecucion en `Stage50_11OwnershipExecutionTests.cpp`, escenario
   `MoveAssignmentOverLiveOwner`; no hay que inventar nada).
2. **Travesia de destruccion**: `__nsc_destroy_<Clase>` hoy llama a los cuerpos de destructor
   declarados y libera el objeto; no recorre campos. Un campo `owned` tiene que destruirse cuando
   su contenedor muere, exactamente una vez.

### Item 9 -- indexers y propiedades `owned`: ARREGLADO (2026-08-29)

Cerrado. `owned` sobre una propiedad o un indexer ya se puede declarar, y significa una cosa
**asimetrica** que conviene fijar por escrito porque la asimetria ES la decision de diseño:

```csharp
class Holder
{
    private owned Res backing;
    public owned Res Slot { get { return backing; } set { backing = move(value); } }
}
...
h.Slot = move(a);          // TRANSFIERE: a queda moved-from, el contenedor destruye
Res p = h.Slot;            // PRESTA: leer no transfiere nada
```

- El **setter toma propiedad**. Su parametro implicito `value` ya venia siendo `owned` (el parser
  le pasa el `TypeSyntax` de la propiedad), asi que `backing = move(value)` siempre compilo dentro
  del accesor. Lo que faltaba era el **call site**: `h.Slot = move(a)` se rechazaba porque el
  destino de asignacion no era ni un local RAII ni un elemento de array ni un campo `owned`.
- El **getter devuelve una referencia plana no-owner**, no propiedad -- y (precision de la
  auditoria) tampoco un `borrowed` formal: no lleva provenance de lifetime. Es la tercera
  categoria de referencia (plana/raw); un alias que sobrevive al contenedor queda colgante por
  diseño.

Por que el getter no puede transferir, y por que eso no es una limitacion de lowering: si leer la
propiedad diera propiedad al llamador, el objeto seguiria teniendo ese mismo puntero en un campo
`owned` que su propia entry de destruccion libera -- dos owners, una asignacion, double free
garantizado. La unica alternativa (vaciar el campo al leer) haria que una **lectura mutara** el
objeto, que no es lo que un getter es. Asi que `owned` describe **donde vive el almacenamiento**
(este objeto lo posee), no lo que hace cada accesor con el.

Y no es una forma inventada para la ocasion: es exactamente la que la libreria ya habia adoptado a
mano mientras el calificador era indeclarable -- `List<T>` expone un indexer de solo lectura que
devuelve un `T` plano y escribe con `SetAt(int, owned T)`. El item 9 hace que esa forma se pueda
expresar en **una** declaracion en vez de dos miembros.

Detalles de implementacion:

- `ClassMemberSymbol::ownsAccessorStorage` marca los dos accesores; en el getter, ademas, se
  neutraliza `declaredOwnership` a `None` en el mismo punto. Sin eso el getter chocaba contra
  *"owned return requires an ownership-transfer expression"*, que era el motivo real por el que la
  declaracion era imposible de escribir.
  - Deliberadamente **no** `Borrowed`: un return prestado en este lenguaje exige una vinculacion de
    lifetime probada contra un owner vivo, y una lectura de campo no tiene esa procedencia que
    ofrecer (medido: `borrowed Res Peek() { return backing; }` se rechaza con *"borrowed return
    requires a borrowed value whose lifetime outlives the call"*).
- En `analyzeAssignment` la busqueda del binding del getter se **adelanta** a antes de analizar el
  RHS -- antes vivia despues, que era justo el problema: para cuando algo sabia que habia un setter,
  el `move(...)` del RHS ya habia sido rechazado. Se guarda como indice y no como puntero porque
  analizar el RHS puede reallocar `calls_`.
- La propiedad **automatica** (`{ get; set; }`) genera `backing = move(value)`, no
  `backing = value`. Ambos son owners, asi que la asignacion plana entre ellos es un copy-owner:
  medido, `set { backing = value; }` compilaba y destruia el objeto **dos veces** (22 donde 21 es
  correcto). Ese hallazgo abrio el bug documentado en
  `bug_copia_de_parametro_owned_en_store_doble_free.md`.

Verificado por ejecucion real (`-O0` y `-O2`) en `Stage50_11OwnershipExecutionTests.cpp`:
propiedad explicita, propiedad automatica, indexer, getter plano (tres lecturas -> una sola
destruccion), sobreescritura que libera lo desplazado, y cuatro escenarios negativos (copia sin
move, use-after-move, getter que no se puede capturar como owner, y move sobre una propiedad sin
calificar). Generico cerrado sobre tipo de valor y sobre tipo de referencia, y virtual/override,
tambien verificados por sonda.

**Lo que NO se toco**: la API publica de `List<T>`. Ahora se podria escribir
`public owned T this[int i] { get {...} set {...} }` y quedarse con `list[i] = move(item)`, pero eso
cambia la superficie publica de la libreria y esa decision es de Alberto, no mia. La capacidad esta;
el cambio de API no.

### Campo `owned` estatico: rechazado en la declaracion (2026-08-29)

Efecto lateral de cerrar el item 9. Un campo `static owned` no tiene contenedor que muera, y nada
corre al salir del proceso, asi que solo podia ser una fuga con una anotacion de propiedad encima.
Antes se aceptaba la declaracion y el error salia recien en el primer `Slot = move(x)` como
*"move(...) is only valid for a direct RAII local or owned parameter..."* -- que apunta a la
transferencia y no menciona el `static`, mandando a corregir la linea equivocada. Ahora:

```
error: static field 'backing' cannot be owned because no destruction ever releases it;
       ownership is discharged by the containing object's destruction
```

### Hueco de soundness verificado -- alias plano hacia un slot que descarga

La libreria tambien lo nombra ("a genuine gap in the current compiler's array-assignment ownership
checking; ... a good target for a future stage's hardening pass"). Confirmado por ejecucion:

```csharp
Res r = new Res(1);      // local PLANO (no owner)
Res[] items = new Res[1];
items[0] = r;            // ACEPTADO, sin ningun chequeo de transferencia
delete items[0];         // el slot descarga: el objeto se destruye
// r sigue apuntando a memoria liberada
```

Desde un owner (`raii`/`owned`) la misma asignacion **si** se rechaza
("RAII owner 'r' cannot be copied by assignment"). El agujero es exclusivamente para referencias
planas: quedan como alias colgante de algo que otro slot ya destruyo. Es seguro para las
relocalizaciones internas que hace la libreria (el puntero solo se mueve entre slots del MISMO
array), y es inseguro en cuanto una API publica lo expone -- que es por lo que `List<T>` no expone
un setter.

## Estado y orden sugerido

| Item | Estado |
|---|---|
| 2. owning array-element storage | **funciona** (modelo manual) |
| 1. owning field storage | **arreglado** -- transferencia, travesia, herencia, sobreescritura |
| 9. ownership-aware indexers/propiedades | **arreglado** -- setter transfiere, getter da referencia plana |
| escritura no-transferencia a campo owned (`null`/`new`/call/alias) | **cerrado** (ronda 35): release siempre + RHS solo transferencia |
| `delete` de campo owned | **prohibido en compilacion** (ronda 35; antes double free medido) |
| overwrite de campo `owned string` | **arreglado** (ronda 35; antes exit 5, leia descriptor de un string) |
| move-out de campo owned (`move(h.slot)`) | **implementado** (ronda 35; antes el campo era un sink) |
| orden de destruccion | **contrato fijado y ejecutado** (ronda 35): inverso de construccion |
| campo `static owned` | **rechazado en la declaracion** |
| alias plano -> slot de ARRAY que descarga | **abierto** -- decision de API (AddRange/InsertRange) |

Para campos el alias plano quedo CERRADO en la ronda 35 (todo RHS de campo owned exige
transferencia). El caso de array es el unico abierto y es decision de diseño de Alberto.

El resto del Bloque 1 quedo cerrado y cubierto por ejecucion real en
`tests/unit/Stage50_11OwnershipExecutionTests.cpp` (57 escenarios / 176 checks). Ver
`claude/33_bloque1_ownership_soundness.md`, `claude/34_bloque1_cierre_item9_y_copy_owner.md` y
`claude/35_bloque1_owning_slots_cierre.md`.
