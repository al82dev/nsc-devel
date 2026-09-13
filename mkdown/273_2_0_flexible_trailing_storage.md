# 273 — Almacenamiento trailing/flexible / Trailing and flexible storage

## Resumen / Summary

NativeSharp incorpora `T[*]` para representar el flexible array member final de un struct C. La
cola fija offset y alineación, agrega cero elementos a `sizeof`, se indexa como memoria raw y se
publica sin pérdida en NSMETA y headers C.

NativeSharp adds `T[*]` to represent the final flexible array member of a C struct. The tail fixes
offset and alignment, contributes zero elements to `sizeof`, uses raw indexing, and is preserved in
NSMETA and C headers.

## Archivos y funciones / Files and functions

- `include/nativesharp/AST/TypeSyntax.h`, `src/Parser/Parser.cpp` y `src/AST/AstDumper.cpp` — añaden
  el modificador de tipo `FlexibleArray` y la sintaxis `T[*]`.
- `src/Frontend/ProjectGenericModel.cpp` y `src/Frontend/ProjectGenericConstraintModel.cpp` —
  conservan el spelling e identidad al clonar tipos.
- `include/nativesharp/Frontend/ProjectMemberModel.h` y
  `src/Frontend/ProjectMemberModel.cpp` — canonicalizan `T[*]`, exponen su elemento y rechazan su
  paso o retorno por valor.
- `include/nativesharp/Target/TargetDataLayout.h` y `src/Target/TargetDataLayout.cpp` — permiten un
  único campo de tamaño cero que todavía participa en alineación y offset.
- `src/Frontend/ProjectObjectLayoutModel.cpp` — exige último campo de struct no-union, elemento de
  lifetime trivial y ausencia de ownership; clases y posiciones contradictorias se rechazan.
- `src/Frontend/ProjectMethodBodyModel.cpp` y `src/Frontend/ProjectOopHirModel.cpp` — habilitan un
  índice integral raw y rechazan locales incompletos.
- `src/Interop/CHeaderGenerator.cpp` — materializa la cola como `T name[]`.
- `tests/unit/FlexibleArrayTests.cpp` y `CMakeLists.txt` — agregan la evidencia dirigida.
- `docs/1.0.1/` — actualiza especificación, libro, estado, changelog, mapas y roadmap.

## Implementación y decisiones / Implementation and decisions

`T[*]` es un cuarto modificador, no un alias de `T[]`. `T[]` continúa siendo una referencia a un
array nativo con cabecera y lifetime; `T[N]` continúa siendo un valor inline completo. La estrella
indica que el almacenamiento también es inline, pero la extensión la gobierna un contrato externo.
El tipo canónico conserva `[*]`, evitando que metadata o genéricos lo confundan con cualquiera de
las otras dos representaciones.

`T[*]` is a fourth modifier, not an alias for `T[]`. `T[]` remains a native array reference with a
header and lifetime; `T[N]` remains a complete inline value. The star says storage is inline while
its extent belongs to an external contract. Canonical type identity preserves `[*]`, preventing
metadata and generics from confusing the three representations.

El layout asigna al campo tamaño cero y la alineación del elemento. El offset alineado sí participa
en `sizeof`, pero ningún elemento implícito lo hace. Sólo el último campo de un struct no-union puede
usar esta regla. Los elementos con destructor o campos owned se rechazan: sin una longitud conocida,
la destrucción automática sería una promesa falsa.

Layout gives the field zero size and the element alignment. Its aligned offset participates in
`sizeof`, but no implicit element does. Only the final field of a non-union struct may use this rule.
Elements with a destructor or owned fields are rejected: automatic destruction without a known
length would be a false promise.

El acceso comparte `RawElementAddress` con punteros, fixed arrays y VLAs. No se agrega longitud,
bounds check, asignación ni liberación. El propietario calcula y valida el tamaño total antes de
reservar; esta entrega expresa el layout, no inventa un allocator variable para structs.

Access shares `RawElementAddress` with pointers, fixed arrays, and VLAs. No length, bounds check,
allocation, or release is added. The owner computes and validates total size before allocation;
this delivery expresses layout rather than inventing a variable-sized struct allocator.

## Evidencia dirigida / Focused evidence

- `byte Tag; int[*] Values;` produce tamaño/alineación 4/4 y cola en offset 4 con tamaño cero.
- `packet->Values[index]` produce una dirección raw de elemento.
- Se rechazan cola no final, union, clase, local, firma por valor y elemento con lifetime no trivial.
- La suite dirigida pasa 10 comprobaciones.
- Una biblioteca mínima O0 genera `int32_t Values[]` y supera
  `--verify-library-artifacts` con `.lib + .nsmeta`.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade almacenamiento trailing flexible / Add flexible trailing storage`

**Descripción / Description**

`Introduce T[*] para colas inline de structs, valida ubicación y lifetime, y conserva el contrato en indexado raw, NSMETA y headers C. / Introduces T[*] for inline struct tails, validates placement and lifetime, and preserves the contract in raw indexing, NSMETA, and C headers.`
