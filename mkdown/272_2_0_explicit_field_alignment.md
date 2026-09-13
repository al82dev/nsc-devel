# 272 — Alineación explícita por campo / Explicit per-field alignment

## Resumen / Summary

`align(N)` ya puede aplicarse a un campo de instancia. Eleva la alineación efectiva del campo, su
posición y el contrato del contenedor, y sobrevive la generación de NSMETA y headers C.

`align(N)` can now be applied to an instance field. It raises the field's effective alignment,
placement, and container contract, and survives NSMETA and C-header generation.

## Archivos y funciones / Files and functions

- `src/Parser/Parser.cpp` — `parseClassMember()` resuelve el `align(N)` contextual como modificador
  de campo cuando la declaración no resulta ser un tipo anidado.
- `include/nativesharp/Frontend/ProjectMemberModel.h` y
  `src/Frontend/ProjectMemberModel.cpp` — `ClassMemberSymbol::explicitFieldAlignment` conserva la
  intención y rechaza globals estáticos.
- `include/nativesharp/Target/TargetDataLayout.h` y `src/Target/TargetDataLayout.cpp` —
  `StructFieldRequest` lleva la alineación mínima; `layoutStruct()` la combina con layout natural,
  pack y offset explícito.
- `src/Frontend/ProjectObjectLayoutModel.cpp` — aplica la misma regla a structs y clases y eleva la
  alineación final del contenedor.
- `src/Driver/Driver.cpp` — la fuente reconstruida desde NSMETA declara el offset y la alineación
  física de cada campo.
- `src/Interop/CHeaderGenerator.cpp` — emite `NS_ALIGNAS(N)` en el campo correspondiente.
- `tests/unit/ExplicitFieldAlignmentTests.cpp` y `CMakeLists.txt` — prueban struct, pack, clase,
  conflicto con `at(N)` y rechazo de campo estático.
- `docs/1.0.1/` — actualiza especificación, libro, estado, changelog, mapas y roadmap.

## Implementación y decisiones / Implementation and decisions

La alineación de campo es un mínimo. Primero se calcula la alineación natural limitada por
`pack(N)` y después se eleva con el `align(N)` explícito. Esto permite describir campos SIMD o ABI
sobrealineados dentro de un paquete sin convertir `align` en una forma de debilitar el tipo. El
valor participa en el offset siguiente, la alineación del contenedor y su padding final.

Field alignment is a minimum. Natural alignment is capped by `pack(N)` first and then raised by the
explicit `align(N)`. This describes over-aligned SIMD or ABI fields inside a packed record without
turning `align` into a way to weaken the type. The value participates in the next offset, container
alignment, and final padding.

Cuando `at(offset)` y `align(N)` aparecen juntos, el offset debe satisfacer la alineación efectiva.
Aceptar una contradicción permitiría que LLVM marcase una carga con una alineación que la dirección
no cumple. Los campos estáticos se rechazan porque su almacenamiento no pertenece a la instancia.

When `at(offset)` and `align(N)` appear together, the offset must satisfy effective alignment.
Accepting a contradiction could make LLVM mark a load with alignment the address does not provide.
Static fields are rejected because their storage is not part of the instance.

NSMETA ya congelaba la alineación física de cada campo. La reconstrucción ahora la vuelve a escribir
como `align(field.alignment)`, incluso para clases, de modo que una biblioteca consumida no degrade
su contrato. El header C usa `NS_ALIGNAS` en el mismo miembro.

NSMETA already froze every field's physical alignment. Reconstruction now writes it back as
`align(field.alignment)`, including class fields, so consuming a library cannot weaken its contract.
The C header uses `NS_ALIGNAS` on the same member.

## Evidencia dirigida / Focused evidence

- `align(16)` coloca un `int` en offset 16 y eleva un struct a tamaño/alineación 32/16.
- La alineación explícita 16 prevalece sobre `pack(1)`.
- Una clase conserva header, offset 16 y contrato de asignación 32/16.
- `at(2) align(4)` se rechaza por contradicción.
- `static align(16)` se rechaza porque no describe un campo de instancia.
- La suite dirigida pasa 9 comprobaciones.
- Una biblioteca mínima O0 emite `NS_ALIGNAS(16)` en su header y supera
  `--verify-library-artifacts` con su `.lib + .nsmeta`.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade alineación explícita por campo / Add explicit per-field alignment`

**Descripción / Description**

`Aplica align(N) a campos de instancia, conserva el contrato en layout, NSMETA y headers C, y rechaza offsets contradictorios. / Applies align(N) to instance fields, preserves the contract in layout, NSMETA, and C headers, and rejects contradictory offsets.`
