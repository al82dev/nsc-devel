# Opaque result types / Tipos de resultado opacos

## Español

Una función o método de edition 3 puede declarar `some Trait` como retorno:

```nativesharp
trait IScore { int Score(); }

class Number : IScore
{
    public int Score() { return 42; }
}

some IScore Make()
{
    return new Number();
}
```

`some IScore` no es `dyn IScore`. El compilador crea un tipo nominal distinto para esa declaración,
guarda el único concreto inferido directamente dentro de él y reenvía la superficie del trait. No
hay boxing, allocation implícita ni despacho dinámico. Todas las rutas no recursivas deben devolver
el mismo tipo concreto y ese tipo debe implementar nominalmente el trait.

El consumidor puede usar métodos del trait, inferir la identidad como argumento genérico y usar
sus asociados demostrados, pero no puede escribir ni construir el wrapper generado. Dos funciones
con el mismo concreto y trait siguen devolviendo identidades opacas diferentes.

`some Trait` sólo es válido como retorno directo. No es válido en fields, parámetros, locales,
argumentos genéricos, punteros, arrays o tuplas. `some` y `dyn` son contratos distintos y no se
combinan.

Una API pública publica en NSMETA 1.30 la identidad, layout in-place, trait y asociados necesarios.
El nombre de la representación concreta puede permanecer internal; el ABI del resultado sí queda
congelado y cualquier cambio incompatible aparece en el diff API/ABI.

## English

An edition 3 function or method may declare `some Trait` as its return type. This is not
`dyn Trait`: the compiler creates one declaration-scoped nominal type, stores the single inferred
concrete representation directly in it, and forwards the trait surface. There is no boxing,
implicit allocation, or hidden dynamic dispatch.

Every non-recursive return path must produce the same concrete type, and that type must nominally
implement the trait. A consumer may call trait methods, infer the anonymous identity as a generic
argument, and use proven associated items, but cannot spell or construct the generated wrapper.
Separate declarations always have separate opaque identities.

`some Trait` is valid only as a direct function or method return. It is invalid in fields,
parameters, locals, generic arguments, pointers, arrays, and tuples. `some` and `dyn` describe
different contracts and cannot be combined.

Public APIs carry identity, in-place layout, trait, and associated contracts in NSMETA 1.30. The
concrete representation name may remain internal, while ABI-relevant changes remain visible to
metadata validation and API/ABI diff tooling.
