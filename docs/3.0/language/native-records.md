# Registros nativos / Native records

## Español

Un `record struct` de NativeSharp 3.0 es un `struct` nativo cuyo comportamiento generado se elige
explícitamente. No crea objeto administrado, descriptor, vtable, boxing, reflexión ni wrapper de
layout:

```nativesharp
public record struct Pair derive(Equality, Hash, Print, Clone)
{
    public int X;
    public int Y;
}
```

`derive(...)` admite cuatro opt-ins independientes:

- `Equality` habilita `Equals(T)`, `==` y `!=` exclusivamente contra el mismo tipo.
- `Hash` habilita `GetHashCode()` sobre las mismas hojas físicas que usa igualdad.
- `Print` habilita `ToString()` como el nombre corto y estático del tipo.
- `Clone` habilita una copia por valor del receptor.

Un derive ausente no se supone. Los derives parciales son válidos y un miembro escrito por el
usuario siempre gana: el compilador consulta la expansión sólo después de que falla el lookup
ordinario.

Igualdad y hash recorren hojas escalares, incluidos structs y arreglos fijos anidados, y excluyen
padding. La comparación es de bits por campo; referencias y punteros comparan su portador, no el
contenido apuntado. Un record anidado debe derivar el mismo contrato y una unión no es elegible,
porque su valor no codifica qué miembro está activo. Todos los records vacíos son iguales; su hash
es la base FNV-1a plegada de forma determinista.

`Clone` no asigna heap ni llama un protocolo runtime: es exactamente una copia del valor. Por eso
se rechaza cuando algún campo, incluso anidado, es `owned`; duplicarlo produciría dos propietarios.
Los structs con destructor ya son inválidos en la frontera de tipos valor. `Print` produce una
cadena literal estática y no inspecciona campos.

Layout, alineación, clasificación de retorno Win64 y paso de parámetros son idénticos a los de un
`struct` con los mismos campos. NSMETA 1.26 publica la marca `record` y cada derive para que una
`LibraryReference` reconstruya exactamente la misma superficie sin exportar helpers binarios.

## English

A NativeSharp 3.0 `record struct` is a native `struct` whose generated behavior is selected
explicitly. It adds no managed object, descriptor, vtable, boxing, reflection, or layout wrapper.

`derive(...)` has four independent opt-ins: `Equality` enables same-type `Equals`, `==`, and `!=`;
`Hash` enables `GetHashCode`; `Print` enables a static short type-name `ToString`; and `Clone`
enables a by-value receiver copy. Missing derives are never inferred, partial derive lists are
valid, and a user-declared member wins because synthesized lookup runs only after ordinary lookup
fails.

Equality and hashing walk scalar leaves, including nested structs and fixed arrays, while ignoring
padding. The contract is per-field bit equality: references and pointers compare their carriers,
not pointed-to contents. A nested record must derive the same contract, and unions are rejected
because the live member is not encoded in the value. Empty records all compare equal and use the
deterministic folded FNV-1a basis as their hash.

`Clone` performs no heap allocation and invokes no runtime protocol; it is exactly a value copy.
It is therefore rejected if any direct or nested field is `owned`, which would create two owners.
Value types cannot declare destructors at all. `Print` returns a static string literal and does not
walk fields.

Layout, alignment, Win64 return classification, and parameter passing are identical to an ordinary
`struct` with the same fields. NSMETA 1.26 publishes record identity and every derive so a
`LibraryReference` reconstructs the same compile-time surface without exported binary helpers.
