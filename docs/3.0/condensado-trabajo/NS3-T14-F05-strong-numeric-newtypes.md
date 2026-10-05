# NS3-T14-F05 — Newtypes numéricos fuertes / Strong numeric newtypes

## ES

Un newtype numérico fuerte contiene exactamente un campo numérico fundamental y conserva identidad nominal aunque comparta representación con otro tipo. Construcción y extracción son explícitas; los operadores sólo existen mediante traits declarados entre la misma identidad y aridad const-genérica. No hay conversión dimensional automática.

`repr(transparent)` publica tamaño, alineación y ABI idénticos a la representación y es obligatorio al exportar por FFI. Un tipo interno puede seguir siendo zero-cost sin prometer ABI transparente. Los derives respetan ownership (`Copy` implica `Move`) y no agregan campos ni wrappers de runtime.

Prueba dirigida: `nsc_ns3_t14_strong_numeric_tests` cubre repr transparent, operator traits, const generics, FFI, zero overhead y conversiones explícitas.

## EN

A strong numeric newtype contains exactly one fundamental numeric field and keeps nominal identity even when another type shares its representation. Construction and extraction are explicit; operators exist only through declared traits between the same identity and const-generic arity. There is no automatic dimensional conversion.

`repr(transparent)` publishes the representation's exact size, alignment, and ABI and is mandatory for FFI export. An internal type may remain zero-cost without promising transparent ABI. Derives preserve ownership (`Copy` implies `Move`) and add no fields or runtime wrappers.

Focused test: `nsc_ns3_t14_strong_numeric_tests` covers transparent representation, operator traits, const generics, FFI, zero overhead, and explicit conversions.
