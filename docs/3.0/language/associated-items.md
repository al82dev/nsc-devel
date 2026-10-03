# Tipos y constantes asociados / Associated types and constants

## Español

### Declaración e implementación

Un `trait` de edition 3 puede declarar tipos y constantes asociados:

```nativesharp
trait IIterator
{
    type Item;
    const int BatchSize = 16;
}
```

Un bloque `impl` define cada requisito que no tenga valor predeterminado. La definición pertenece
al par coherente `Trait for Type`; no añade campos, storage, vtable, RTTI ni registro runtime.

```nativesharp
class Counter { }

impl IIterator for Counter
{
    type Item = int;
}
```

Los defaults de un trait genérico se sustituyen con sus argumentos antes de normalizar una
proyección. Por ejemplo, `trait IValue<T> { type Item = T; }` implementado como
`impl IValue<int> for Number` fija `Number.Item` a `int`.

### Proyecciones y constraints

`T.Item` es una proyección de tipo. El solver la normaliza después de conocer el tipo concreto de
`T`; también admite cadenas como `T.Item.Element`. Una cláusula de igualdad usa `==`:

```nativesharp
private static int Read<T>()
    where T : IIterator
    where T.Item == int
{
    return T.BatchSize;
}
```

`T.Name` en una expresión sustituye una constante asociada comprobada en compile time. Su tipo
debe ser exactamente el declarado por el trait y su inicializador debe ser constante.

### Reglas deterministas

- omitir un requisito sin default es error;
- dos definiciones directas del mismo nombre son error;
- defaults heredados que compiten exigen una definición explícita;
- una proyección sin definición informa tanto la proyección como el dueño concreto;
- los ciclos de normalización se rechazan antes de monomorfizar;
- las definiciones y requisitos públicos viajan en NSMETA 1.29; un escritor anterior no puede
  descartarlos silenciosamente.

Los tipos asociados son entidades exclusivamente estáticas. Tras la normalización, MIR, ABI y PDB
observan el tipo concreto resultante; no existe un objeto runtime separado que el depurador deba
mostrar. Una constante asociada se conserva como constante, no como local ficticio.

## English

### Declaration and implementation

An edition 3 `trait` may declare associated types and constants:

```nativesharp
trait IIterator
{
    type Item;
    const int BatchSize = 16;
}
```

An `impl` block defines every requirement that has no default. The definition belongs to the
coherent `Trait for Type` pair; it adds no field, storage, vtable, RTTI, or runtime registry.

```nativesharp
class Counter { }

impl IIterator for Counter
{
    type Item = int;
}
```

Defaults from a generic trait are substituted with its arguments before projection
normalization. For example, `trait IValue<T> { type Item = T; }` implemented as
`impl IValue<int> for Number` fixes `Number.Item` to `int`.

### Projections and constraints

`T.Item` is a type projection. The solver normalizes it after the concrete type of `T` is known;
chains such as `T.Item.Element` are supported as well. Equality clauses use `==`:

```nativesharp
private static int Read<T>()
    where T : IIterator
    where T.Item == int
{
    return T.BatchSize;
}
```

`T.Name` in an expression substitutes an associated constant checked at compile time. Its type
must exactly match the trait declaration and its initializer must be constant.

### Deterministic rules

- omitting a requirement without a default is an error;
- two direct definitions with the same name are an error;
- competing inherited defaults require an explicit definition;
- an unresolved projection reports both the projection and its concrete owner;
- normalization cycles are rejected before monomorphization;
- public requirements and definitions cross modules in NSMETA 1.29; an older writer cannot erase
  them silently.

Associated types are compile-time-only entities. After normalization, MIR, ABI, and PDB observe
the resulting concrete type; there is no separate runtime object for a debugger to display. An
associated constant remains a constant rather than a fabricated local.
