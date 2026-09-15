# 314 — Indexadores `ref` / Reference-returning indexers

## Resumen / Summary

Un indexador puede seleccionar storage mutable sin copiar su valor:

```nativesharp
public ref int this[int index]
{
    get { return ref slots[index]; }
}

table[3] += 1;
ref int alias = ref table[3];
```

The getter returns an address, so reads load through it and writes target it directly.

## Contrato / Contract

- La declaración escribe `ref` antes de `T` y contiene exclusivamente `get`.
- El getter usa `return ref` y todas las reglas de tipo exacto, mutabilidad y duración del punto 313.
- El resultado admite lectura, asignación, asignación compuesta, incremento y local `ref`.
- Un indexador ordinario conserva su contrato getter/setter y no se vuelve direccionable.
- Overrides e interfaces deben coincidir también en el modo `ref`.
- Receptor e índices se evalúan una vez por llamada; no existe una segunda llamada a setter.
- NSMETA 1.15 conserva el bit del `get_Item` y reconstruye la forma get-only en consumidores.

Reference indexers reuse the pointer ABI of reference-returning methods. They do not own or extend
the selected storage and cannot combine a returned reference with ownership qualifiers.

## Implementación / Implementation

El parser marca el `IndexerDeclaration` y su getter sintético. El modelo semántico propaga
`assignable/addressable` desde `get_Item` cuando su retorno es `ref`, evitando la reescritura
histórica hacia `set_Item`. HIR baja la llamada en contexto de dirección para stores o aliases y
agrega `PointerLoad` sólo al leer el valor. MIR y LLVM reutilizan sin bifurcaciones nuevas los hechos
`referenceResult` y la ABI `ptr` del punto 313.

El materializador de `LibraryReference` emite `ref T this[...] { get ... }` desde NSMETA. La
verificación rechaza bits `ref-return` en properties, setters o registros que no sean métodos.

## Cobertura / Coverage

`tests/unit/Item314ReferenceIndexersTests.cpp` certifica en O0:

- escritura directa, `+=`, incremento, lectura y enlace de local `ref`;
- llamadas directas, virtuales y por interfaz con ejecución real;
- ausencia de setter y rechazo de setter escrito, indexador ordinario no direccionable e
  implementación de interfaz con modo distinto;
- HIR/MIR/LLVM y un productor `.lib` consumido mediante NSMETA 1.15.

La prueba enfocada pasa 1/1. El baseline completo se reserva para el cierre único de la tanda 6.

## Mensaje de commit / Commit message

`Implementa indexadores ref / Implement reference indexers`

`Convierte getters de indexador ref en lvalues get-only de evaluación única y conserva su ABI en
dispatch y NSMETA. / Make ref indexer getters single-evaluation get-only lvalues and preserve their
ABI through dispatch and NSMETA.`
