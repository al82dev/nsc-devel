# Tagged variants / Variants etiquetados

## Español

### Construcción e identidad

Un `variant` define un dominio nominal y cerrado de casos. Cada caso es unitario o contiene un
único payload; un tuple expresa varios valores sin crear una segunda regla de layout.

```nativesharp
public variant Result<T, E>
{
    Ok(T),
    Error(E)
}
```

El orden declarado forma parte de la identidad observable: `Ok` tiene tag 0 y `Error` tag 1.
NSMETA 1.27 publica la marca nominal `variant` y el catálogo ordenado `(nombre, tipo de payload)`.
Un consumidor precompilado reconstruye la declaración, no intenta deducirla de una tabla de
métodos ordenada alfabéticamente.

### Layout observable en Windows x64

- El tag explícito usa `byte` hasta 256 casos, `ushort` hasta 65 536 y `uint` después.
- El payload es una unión in-place con tamaño y alineación máximos entre sus alternativas.
- El offset del payload es el primer offset alineado después del tag.
- El tamaño final se redondea a la alineación máxima del agregado.
- No hay boxing, heap ni dispatcher de runtime.
- Un pointer nulo no se usa como niche implícito: sin una autorización de lenguaje, el tag sigue
  presente. Por ello `variant Optional { None, Some(int*) }` mide 16 bytes en Windows x64.

`pack`, `align`, `size`, `readonly` y `borrowed` no pueden cambiar este contrato canónico. En
NativeSharp 3.0 no existe una autorización alternativa de niche: todos los variants usan este tag
explícito y estable.

### Recursión y lifetime

La recursión indirecta mediante pointer o referencia es válida. Un payload que contiene el mismo
variant por valor forma un layout infinito y se rechaza. Los payloads `owned` y move-only son
productivos: el cambio de caso destruye el payload desplazado después de evaluar el reemplazo, y la
salida normal o excepcional destruye exactamente una vez el caso que permanezca activo.

Un `variant` posee su almacenamiento activo. Por ello `borrowed variant` y un payload calificado
directamente como `borrowed` no forman parte del contrato 3.0. Cuando un caso necesita transportar
una vista, ésta se encapsula en un `borrowed struct` con lifetime nombrado. `readonly` también sigue
siendo un modificador exclusivo de structs no-union y no altera la construcción interna del variant.

Los tipos genéricos variant son productivos dentro del proyecto que los declara. La importación
nominal de un tipo genérico desde `LibraryReference` pertenece al frente general de genéricos
precompilados; NSMETA 1.27 sí conserva la identidad de todo variant concreto publicado.

## English

### Construction and identity

A `variant` defines a nominal, closed case domain. A case is either unit-like or carries one
payload; use a tuple to carry multiple values without introducing another layout rule.

Declaration order is observable: `Ok` has tag 0 and `Error` has tag 1. NSMETA 1.27 publishes the
nominal `variant` flag and the ordered `(name, payload type)` catalogue. A precompiled consumer
reconstructs the declaration instead of guessing it from an alphabetically sorted method table.

### Observable Windows x64 layout

- The explicit tag is `byte` for up to 256 cases, `ushort` for up to 65,536, then `uint`.
- Payloads share one in-place union sized and aligned for the largest alternative.
- The payload starts at the first properly aligned offset after the tag.
- Final size is rounded to the aggregate alignment.
- No boxing, heap allocation, or runtime dispatcher is introduced.
- A null pointer is not an implicit niche. Without an explicit language authorization the tag
  remains present, so `variant Optional { None, Some(int*) }` is 16 bytes on Windows x64.

`pack`, `align`, `size`, `readonly`, and `borrowed` cannot override this canonical contract.
NativeSharp 3.0 defines no alternate niche authorization: every variant keeps this explicit,
stable tag.

### Recursion and lifetime

Indirect recursion through a pointer or reference is valid. Carrying the same variant by value
would create an infinite layout and is rejected. `owned` and move-only payloads are productive:
changing the active case destroys the displaced payload after evaluating the replacement, while
normal or exceptional scope exit destroys the remaining active case exactly once.

A `variant` owns its active storage. Consequently, `borrowed variant` and directly qualified
`borrowed` payloads are outside the 3.0 contract. A case that transports a view wraps it in a
`borrowed struct` with a named lifetime. `readonly` also remains exclusive to non-union structs and
does not alter a variant's internal construction.

Generic variants are productive in their declaring project. Nominal import of a generic type from
a `LibraryReference` belongs to the general precompiled-generics frontier; NSMETA 1.27 preserves
the identity of every published concrete variant.
