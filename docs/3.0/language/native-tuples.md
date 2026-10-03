# Tuplas nativas / Native tuples

## Español

Una tupla es un agregado estructural nativo. `()` es la tupla unidad, `(T,)` es una tupla de un
elemento y `(T1, T2, ...)` cubre cualquier aridad mayor. La coma de `(T,)` es obligatoria para
distinguir el tipo de una agrupación entre paréntesis.

Los nombres son metadatos de fuente y no forman parte de la identidad:

```nativesharp
(int x, long y) first = (20, 22);
(int left, long right) second = first;
```

Ambas declaraciones usan la misma clave estructural. La clave canónica codifica aridad y cada
tipo con longitud explícita, por lo que las tuplas anidadas no son ambiguas. La misma identidad se
usa en firmas, genéricos cerrados, HIR, MIR y LLVM.

El layout sigue las reglas de agregados de Windows x64: cada elemento se alinea naturalmente, se
conserva padding interno y el tamaño final se redondea a la alineación máxima. La tupla unidad usa
un carrier inerte de 1 byte y alineación 1; así siempre es direccionable y no depende de una
extensión de tamaño cero. Una tupla de un elemento tiene el layout natural de ese elemento.

En llamadas Windows x64, carriers de 1, 2, 4 u 8 bytes pasan y regresan directamente como enteros
del mismo ancho. Los demás parámetros viajan mediante una copia propiedad del caller y los
retornos usan `sret`. Esta es una convención NativeSharp/Win64, no una promesa de compatibilidad
con una `struct` anónima de C: C no tiene tipos tupla ni un agregado vacío estándar. Una firma
`extern C` debe usar un struct nominal cuyo contrato C sea explícito.

La construcción y el transporte admiten aridades 0–16 y mayores, nesting, nombres y tuplas como
argumentos genéricos. Los elementos con lifetime trivial cruzan todo el backend. Las operaciones
que separarían una tupla con elementos `owned` o move-only permanecen rechazadas hasta que T06
provea moves por elemento y drop flags parciales; el frontend falla antes de emitir HIR inseguro.

## English

A tuple is a native structural aggregate. `()` is the unit tuple, `(T,)` is a one-element tuple,
and `(T1, T2, ...)` covers every larger arity. The trailing comma in `(T,)` is mandatory so the
type cannot be confused with parenthesized grouping.

Element names are source metadata and do not participate in identity. Canonical identity encodes
arity and every element type with explicit lengths, making nested tuples unambiguous. Signatures,
closed generics, HIR, MIR, and LLVM all use that same identity.

Layout follows Windows x64 aggregate rules: natural element alignment, explicit internal padding,
and final size rounded to maximum alignment. The unit tuple has an inert one-byte, align-one
carrier, so it remains addressable without relying on a zero-size extension. A one-element tuple
has its element's natural layout.

At Windows x64 call boundaries, 1-, 2-, 4-, and 8-byte carriers pass and return directly as an
integer of equal width. Every other parameter uses a caller-owned copy and every other return uses
`sret`. This is a NativeSharp/Win64 convention, not compatibility with an anonymous C struct: C
has neither tuple types nor a standard empty aggregate. An `extern C` signature must use a named
struct with an explicit C contract.

Construction and transport support arities 0–16 and larger, nesting, names, and tuple generic
arguments. Trivial-lifetime elements cross the complete backend. Operations that would split an
`owned` or move-only tuple remain rejected until T06 provides per-element moves and partial drop
flags; the frontend fails before unsafe HIR can be emitted.
