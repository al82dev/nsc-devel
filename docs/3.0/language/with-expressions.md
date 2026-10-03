# Expresiones `with` / `with` expressions

## Español

`source with { Field = value }` construye un valor nuevo. Primero evalúa `source` exactamente una
vez, copia el carrier completo y después aplica cada update en el orden textual. El valor fuente
no se modifica.

```nativesharp
Pair changed = Source() with { Y = MarkY(), X = MarkX() };
```

La equivalencia conceptual para un struct trivial es:

```text
temporary_source = Source();
result = bitwise_copy(temporary_source);
result.Y = MarkY();
result.X = MarkX();
```

El lowering real no introduce un wrapper. HIR publica `aggregate-spread`, MIR publica
`aggregate-spread-store` con offsets separados de origen/destino y LLVM escribe directamente en el
slot final cuando existe. El costo es una copia completa del tamaño del struct más un store por
campo actualizado; no hay heap implícito.

Son elegibles structs con lifetime trivial, incluidos structs genéricos cerrados. Un método
`Clone()` explícito puede producir la fuente —y se evalúa una vez—, pero `with` no convierte por sí
solo un tipo move-only en copiables. Agregados con campos `owned` permanecen rechazados hasta que
T06 aporte moves parciales y drop flags capaces de cerrar un update que falle a mitad.

Los nombres de campo respetan accesibilidad. Un campo privado puede actualizarse desde código del
tipo declarante, pero no desde otro tipo. Los campos `readonly` pueden recibir un valor durante la
construcción del nuevo resultado; después conservan su inmutabilidad. Miembros desconocidos o
duplicados y conversiones inválidas se diagnostican antes de HIR. Las clases se rechazan porque
copiar una referencia no crea un valor independiente.

## English

`source with { Field = value }` constructs a new value. It evaluates `source` exactly once, copies
the complete carrier, and then applies updates in textual order. The source value is unchanged.

For a trivial struct the conceptual expansion is a source temporary, one complete value copy, and
one store per update. The real lowering adds no wrapper: HIR uses `aggregate-spread`, MIR uses
`aggregate-spread-store` with distinct source/destination offsets, and LLVM writes directly to a
final slot when one is available. No implicit heap allocation occurs.

Eligible inputs are trivial-lifetime structs, including closed generic structs. An explicit
`Clone()` method may produce the source and is evaluated once, but `with` does not by itself make a
move-only type copyable. Aggregates with `owned` fields remain rejected until T06 supplies partial
moves and drop flags that can clean up a failed update.

Field names obey ordinary accessibility. A private field can be updated from its declaring type,
not from unrelated code. A `readonly` field may receive a value while the new result is being
constructed and remains immutable afterward. Unknown or duplicate members and invalid conversions
are diagnosed before HIR. Classes are rejected because copying a reference would not create an
independent value.
