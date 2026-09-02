# P0 -- copiar un parametro `owned` en un store: double free aceptado por el compilador

Encontrado el 2026-08-29 mientras se cerraba el item 9 del Bloque 1. Preexistente desde Stage 52.
Arreglado en la misma ronda.

## El programa

```csharp
class Holder
{
    private owned Res slot;
    public void Take(owned Res value) { slot = value; }   // sin move()
}
```

`slot = value` **compilaba**. El parametro `owned Res value` conserva su storage no-nulo al salir
de `Take`, asi que su cleanup automatico de parametro `owned` destruye el objeto -- y el campo (o
el array, o el slot que sea) guarda una copia del mismo puntero y lo vuelve a liberar cuando el
contenedor muere.

**Double free.** No "podria ser": medido.

## La medicion

La forma exacta del fixture `Box<T>` de Stage 52, hecha ejecutable, con dos elementos:

```csharp
class Box<T> {
    private owned T[] items; private int count;
    public void Add(owned T value) { items[count] = value; count = count + 1; }
    public void Clear() { int i = 0; while (i < count) { delete items[i]; i = i + 1; } count = 0; }
}
// dos Payload agregados y despues Clear()
```

| forma | destrucciones observadas (2 objetos) | -O0 | -O2 |
|---|---|---|---|
| `items[count] = value;` | **4** | si | si |
| `items[count] = move(value);` | 2 (correcto) | si | si |

Cuatro destrucciones de dos objetos, identico en `-O0` y `-O2`. No es un artefacto de
optimizacion ni de Wine: son las dos descargas legitimas corriendo sobre la misma asignacion.

## Por que sobrevivio

Dos capas, y las dos son instructivas.

**1. La regla existia para todos los destinos menos uno.** Copiar un owner sin `move` ya se
rechazaba en todos lados:

| origen -> destino | antes |
|---|---|
| local `raii`/`owned` -> campo | rechazado |
| parametro `owned` -> local `raii` | rechazado |
| parametro `owned` -> argumento `owned` | rechazado |
| **parametro `owned` -> store (campo / elemento de array)** | **ACEPTADO** |

El chequeo de copy-owner en `analyzeAssignment` solo miraba `ScopeNameKind::Local` con `isRaii`.
Nunca miro el caso `Parameter`. Es exactamente la forma de hueco que deja abierta una
caracteristica nueva: el Bloque 1 abrio el campo `owned` como destino, y la regla que guarda todos
los otros destinos no lo siguio hasta ahi.

**2. Los tests aseveraban la forma del IR, no el comportamiento.** `Stage52GenericCollectionsCompilerFixesTests.cpp`
tenia **cinco** fixtures con esta forma exacta (`Box<T>.Add`, `RefSlot<T>.Put`, `Holder<T>.Put`,
`Grower<T>.Add`, `Box<T>.SetValue`) y las cinco pasaban, porque lo unico que se asertaba era
*"lowers to OOP MIR"*. Un assert de forma de IR no distingue un store correcto de uno que libera el
objeto dos veces -- que es literalmente el patron que la auditoria de tests marco.

Peor: `Stage52_4OwnedCleanupRuntimeTests.cpp` tenia un escenario dedicado a este bug cuyo comentario
afirmaba que era

> "a genuine runtime double-free, **not something semantic analysis can reject in general** -- a
> plain array-element assignment is legal NativeSharp"

y asertaba que el programa **debia seguir compilando**. La premisa estaba mal, y el error era de
encuadre: se leyo el peligro como *"una asignacion a elemento de array"*, que efectivamente es
legal, cuando en realidad es *"un parametro `owned` como lado derecho de un store"*, que crea dos
owners de una asignacion sin importar cual sea el destino.

## El arreglo

Extender el chequeo de copy-owner de `analyzeAssignment` al caso `Parameter`, con el mismo
angostamiento de tipo de valor que usa el resto (`owned` sobre algo que no posee una asignacion
separada sigue siendo el no-op que siempre fue):

```
error: cannot copy owned value 'value' by assignment; use move(value) to transfer ownership
```

## Barrido

Se compilaron los **97 proyectos** `.nsproj` del repo (`lib/`, `tests/`, `examples/`) con la regla
activa. **La libreria esta limpia**: los siete call sites que motivaron Stage 52.4
(`List<T>.Add`, `Queue<T>.Enqueue`, `Stack<T>.Push`, los tres de `Dictionary`, `SortedList`,
`PriorityQueue.Enqueue`, `LinkedListNode.SetValue`/`SetNext`) ya usaban `move(...)`. Los unicos
hits fueron los cinco fixtures de test citados arriba, corregidos en esta ronda.

Es decir: el bug estaba en el compilador y en los tests, no en la libreria. La libreria se habia
arreglado a mano en Stage 52.4 sin que nada impidiera volver a escribirlo mal.

## Cambios en la suite

- Los cinco fixtures de `Stage52GenericCollectionsCompilerFixesTests.cpp` pasan a `move(value)`,
  con el comentario explicando que la forma anterior era un double free medido y no un detalle de
  estilo. Se corrigio el fixture, **no** se aflojo el assert.
- `Stage52_4OwnedCleanupRuntimeTests.cpp`: la mitad negativa de "Bug B" pasa de "debe compilar y
  crashear en ejecucion real" a "debe ser rechazada en compilacion", asertando el diagnostico
  exacto. Es una garantia estrictamente mas fuerte: Alberto habia confirmado contra Windows real
  (2026-08-28) que esta forma efectivamente crashea, y ahora la clase entera de programa es
  **inescribible** en vez de estar solamente documentada como peligrosa. La salvedad sobre la
  divergencia de Wine que vivia ahi queda sin objeto: ya no hay ejecutable que correr.
- `Stage50_11OwnershipExecutionTests.cpp`: escenario negativo
  `CopyingAnOwnedParameterByAssignmentRejected`, para que la regla no se pueda volver a perder la
  proxima vez que se abra un destino nuevo.

## Como se encontro

No leyendo codigo: **mutando un test**. El comentario del escenario de la propiedad automatica
afirmaba que sin `move` el conteo daria 22 en vez de 21. Antes de dejar esa afirmacion escrita se
la midio -- exactamente lo que este proyecto viene exigiendo -- y dio 22. Eso obligo a preguntar
por que `set { backing = value; }` compilaba, y de ahi salio el hueco.
