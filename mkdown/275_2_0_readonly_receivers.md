# 275 — Receptores readonly / Readonly receivers

## Resumen / Summary

NativeSharp extiende la inmutabilidad de structs a toda operación efectuada mediante un receptor.
Parámetros `in`, campos readonly y `this` readonly admiten lectura y llamadas readonly, pero no
stores ni llamadas a miembros mutadores. No se introducen copias defensivas ocultas.

NativeSharp extends struct immutability to every operation performed through a receiver. `in`
parameters, readonly fields, and readonly `this` allow reads and readonly calls, but not stores or
calls to mutating members. No hidden defensive copies are introduced.

## Archivos y funciones / Files and functions

- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp` — distinguen contexto struct y
  permiten `readonly` en métodos, properties get-only e indexers get-only de instancia.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` — agrega la procedencia readonly al
  resultado de expresiones y el predicado del receptor implícito.
- `src/Frontend/ProjectMemberModel.cpp` — fija el contrato en métodos/accessors explícitos y en
  todos los miembros de instancia de un readonly struct; marca backings automáticos readonly.
- `src/Frontend/ProjectMethodBodyModel.cpp` — propaga readonly por `in`, `this`, campos y cadenas de
  miembros; rechaza stores, setters y selección de miembros mutadores.
- `src/Driver/Driver.cpp` — reconstruye campos, métodos y accessors readonly desde NSMETA, incluidos
  campos privados conservados sólo para layout.
- `tests/unit/ReadonlyReceiverTests.cpp` — agrega once comprobaciones dirigidas.
- `CMakeLists.txt` — registra la suite independiente.
- `docs/1.0.1/` — actualiza especificación, libro, estado, changelog, mapas y roadmap.

## Implementación y decisiones / Implementation and decisions

`ExpressionResult::readonlyReceiver` transporta una propiedad de la ubicación, no del tipo nominal.
Esto distingue un `Counter` local mutable del mismo `Counter` recibido mediante `in`. La propiedad
se conserva al encadenar campos, por lo que `holder.Value.Inner.Mutate()` no recupera mutabilidad si
la raíz o algún campo de la ruta es readonly.

`ExpressionResult::readonlyReceiver` carries a property of the storage location, not the nominal
type. This distinguishes a mutable local `Counter` from the same `Counter` received through `in`.
The property survives field chains, so member access cannot regain mutability from a readonly root.

La resolución normal de métodos y la resolución sintética de properties/indexers aplican la misma
regla después de seleccionar la sobrecarga exacta. Sólo un miembro de instancia de struct necesita
el filtro; pasar una referencia de clase mediante `in` no vuelve inmutable el objeto referenciado.

Normal method resolution and synthetic property/indexer resolution apply the same rule after exact
overload selection. Only a struct instance member needs the filter; passing a class reference with
`in` does not make the referenced object immutable.

Los miembros de un readonly struct se marcan readonly en el modelo, salvo el constructor. El
constructor sigue inicializando campos del receptor actual. Los backings de auto-properties se
marcan sin exigir una sintaxis imposible al usuario. Una property o indexer explícitamente readonly
de un struct mutable debe ser get-only.

Readonly-struct members are marked readonly in the model except for the constructor, which retains
construction writes. Auto-property backing fields receive the invariant without requiring
impossible user syntax. Explicit readonly properties and indexers on mutable structs are get-only.

NSMETA conserva el bit de miembro ya existente y la fuente pública materializada vuelve a escribir
el modificador. Para campos privados omitidos de la superficie nominal se usa la bandera física del
campo o la del tipo, evitando producir un readonly struct consumidor con layout mutable inválido.

NSMETA preserves the existing member bit and public-source materialization writes the modifier
again. Private fields omitted from the nominal surface use their physical field flag or type flag,
avoiding an invalid mutable reconstruction of a readonly struct.

## Evidencia dirigida / Focused evidence

- `in Counter` puede invocar método, property e indexer readonly.
- `ref Counter` conserva stores y llamadas mutadoras.
- Un método readonly no puede escribir un campo ni llamar un método mutador implícitamente.
- Un parámetro `in` rechaza tanto `value.Field = ...` como `value.Mutate()`.
- Un campo readonly rechaza una llamada mutadora sobre el struct almacenado.
- Los miembros de un readonly struct reciben `this` readonly implícito.
- La propagación alcanza elementos de arreglos inline y el backing de eventos.
- Clases, miembros estáticos y accessors con setter rechazan el modificador.
- Las tres suites relacionadas pasan y una biblioteca mínima O0 conserva campos privados, property
  y método a través de `.lib + .nsmeta + consumidor`.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade receptores readonly / Add readonly receivers`

**Descripción / Description**

`Propaga receptores readonly desde in, campos y miembros de structs; bloquea stores y llamadas mutadoras, y conserva el contrato en NSMETA sin copias defensivas. / Propagates readonly receivers from in parameters, fields, and struct members; blocks stores and mutating calls, and preserves the contract in NSMETA without defensive copies.`
