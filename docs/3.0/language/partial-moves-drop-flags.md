# NativeSharp 3.0 — Moves parciales y drop flags / Partial moves and drop flags

## Contrato normativo / Normative contract

Este contrato aplica a unidades con `Language=3`. Un struct es move-only salvo `copy`; un move
parcial consume una ruta de campos de un local o parámetro por valor sin consumir sus campos
hermanos. La identidad semántica de la ruta es el root estable (`L<n>` o `P<n>`) seguido por cada
campo nominal, por ejemplo `L0/Header/Handle`.

This contract applies to `Language=3` units. A struct is move-only unless declared `copy`; a
partial move consumes one field path rooted at a by-value local or parameter without consuming its
siblings. A path has stable semantic identity (`L<n>` or `P<n>`) followed by every nominal field.

### Reglas de disponibilidad / Availability rules

- `move(value.Field)` exige un root move-only, un field asignable y un destino consumidor.
- La ruta movida y todos sus descendientes quedan no disponibles; un ancestro tampoco puede leerse
  ni moverse completo mientras contenga un descendiente movido.
- Los campos hermanos continúan disponibles.
- Asignar nuevamente la ruta completa elimina sus estados `moved` y `maybe-moved` descendientes.
- Un join de control conserva `moved` si todos los caminos lo prueban y produce `maybe-moved` si
  sólo algunos lo hacen. Leer o mover un `maybe-moved` es error.
- Un loop no puede llevar un move parcial vivo por un back-edge sin reponer antes la ruta.
- Un segundo move de la misma ruta es error y no genera un diagnóstico genérico adicional.

- `move(value.Field)` requires a move-only root, an assignable field and a consuming destination.
- The moved path and its descendants become unavailable; an ancestor cannot be read or moved as a
  whole while it contains a moved descendant.
- Sibling fields remain available.
- Reassigning the complete path clears its descendant `moved` and `maybe-moved` states.
- CFG joins retain `moved` when every path proves it and produce `maybe-moved` when only some do.
- A loop cannot carry a partial move across a back-edge unless the path is reinitialized first.

### Destructores y drop glue / Destructors and drop glue

- Un tipo valor con destructor personalizado no permite moves parciales: el destructor observa el
  valor completo. El valor sí puede moverse completo y su responsabilidad de destrucción se
  transfiere al destino.
- Un tipo `copy` y un `union` crudo no pueden declarar destructor de struct. Edición 2 conserva su
  diagnóstico y semántica histórica.
- El drop glue ejecuta primero el destructor propio y después destruye fields en orden inverso.
  Fields `owned`, structs anidados y fixed arrays se recorren recursivamente.
- Locales y parámetros por valor con drop participan en las regiones de cleanup ordinarias:
  salida léxica, `return`, `break`, `continue` y unwind nativo.
- El flag explícito existe sólo cuando un destructor personalizado necesita distinguir el valor
  vivo del movido. Empieza en `false`, pasa a `true` tras inicialización completa y vuelve a
  `false` al mover el valor completo.
- Cuando el drop sólo contiene fields `owned`, cada ranura vaciada a null es su propio drop flag;
  no se reserva un booleano adicional.

- A value type with a custom destructor forbids partial moves because its destructor observes the
  complete value. A whole-value move is allowed and transfers the drop responsibility.
- A `copy` type and a raw `union` cannot declare a struct destructor. Edition 2 keeps its historical
  diagnostic and behavior.
- Drop glue runs the value's own destructor first, then drops fields in reverse order. Owned fields,
  nested structs and fixed arrays are traversed recursively.
- Drop-bearing locals and by-value parameters participate in lexical, structured-exit and native
  unwind cleanup.
- An explicit boolean flag exists only for custom-destructor values; owner-only aggregates use
  cleared/null field slots as optimized guards.

## Coste observable / Observable cost

El análisis de rutas vive únicamente en el compilador. No cambia layout, mangling ni ABI Win64.
En runtime, un tipo con destructor personalizado puede añadir un `i1` por local o parámetro que
requiera guard; LLVM puede eliminarlo cuando el flujo demuestra un único estado. Los agregados de
owners no añaden ese storage.

Path analysis is compile-time only and changes neither layout, mangling nor the Win64 ABI. A local
or parameter whose value has a custom destructor may require one optimizable `i1` guard; aggregates
whose drop consists only of owner fields add no separate storage.

## Diagnósticos mínimos / Required diagnostics

- uso o move de ruta ya movida;
- uso de ruta `maybe-moved` después de join;
- move completo de agregado con descendiente movido;
- move parcial que cruza back-edge;
- move parcial de un tipo con destructor personalizado;
- destructor en struct `copy`, `union` o edición 2.

Los diagnósticos señalan la expresión fuente y conservan un código estructurado cuando existe el
catálogo `NS3xxx`; ninguna decisión depende de comparar texto localizado.

## Relación con otras tandas / Relationship to later work

F02 cubre rutas nominales de fields y drop recursivo de structs/fixed arrays. El refinamiento de
préstamos disjuntos pertenece a F03; `defer` y construcción parcialmente fallida a F05; drop de
payload activo de variants y glue indirecto cross-module se cierran con T03/F08. Esas tandas no
pueden relajar las invariantes anteriores.

F02 covers nominal field paths and recursive struct/fixed-array drop. Split-borrow precision belongs
to F03; `defer` and partially failed construction to F05; active variant payload and indirect
cross-module glue close with T03/F08. Those features may extend but not weaken this contract.
