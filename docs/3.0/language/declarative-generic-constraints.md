# Declarative generic constraints / Constraints genéricos declarativos

## Español

Una cláusula `where` limita los argumentos con los que se puede materializar una plantilla:

```nativesharp
int Measure<T>(T value)
    where T : unmanaged, copy, move, new(), notnull
{
    return 42;
}

T Choose<T, U>(T left, U right) where T == U
{
    return left;
}

int Width<const int N>() where N == 4
{
    return N;
}
```

### Catálogo

| Constraint | Obligación |
|---|---|
| `class` | Tipo referencia: clase, interfaz, delegado, `string` o arreglo dinámico. |
| `struct` | Tipo valor: primitivo no `void`, enum, struct, puntero o arreglo inline. |
| Clase base | El argumento es esa clase o deriva de ella. |
| Interfaz o trait | El argumento implementa ese contrato nominal. |
| `new()` | Constructor público sin parámetros, explícito o implícito; todo tipo valor lo satisface. |
| `copy` | Su almacenamiento puede copiarse; un agregado se comprueba recursivamente. |
| `move` | Su representación puede transferirse. No significa `move-only`. |
| `lifetime` | El tipo participa en seguimiento explícito de vida: referencia, puntero, arreglo dinámico, delegado, ownership explícito o vista prestada. |
| `unmanaged` | El almacenamiento completo es unmanaged de forma recursiva. |
| `notnull` | La representación no puede contener el valor nulo. Actualmente lo demuestran tipos valor y arreglos inline; no existe todavía una anotación de referencia no nula. |
| `where T == U` | Ambos parámetros se sustituyen por la misma identidad de tipo. |
| `where N == valor` | El parámetro const coincide con el valor normalizado y su tipo const declarado. |

`class` o una clase base concreta es un constraint primario y debe aparecer primero. `struct` debe
aparecer primero salvo cuando la propia cláusula es contradictoria, caso en que se informa la
contradicción causal. `unmanaged` implica semánticamente almacenamiento de valor, pero puede
combinarse con `struct`; la normalización elimina diferencias de orden y repeticiones semánticas.
Una repetición textual se rechaza para que el contrato no esconda errores de edición.

Las restricciones no eligen una sobrecarga por sí solas. Primero se determina qué firmas son
aplicables; después se descartan las especializaciones que incumplen su `where`. El diagnóstico
de una candidata descartada sólo se publica si ninguna candidata válida atiende la llamada.

## English

A `where` clause limits the arguments that may materialize a template. NativeSharp 3 supports
nominal bounds (`class`, a base class, interface or trait), representation bounds (`struct`,
`unmanaged`, `notnull`), operation bounds (`new()`, `copy`, `move`), tracked-lifetime bounds, and
type or constant equality.

Constraints are normalized into a sorted, length-delimited identity. Source order therefore does
not alter specialization-cache or incremental-build identity. Textual duplicates and impossible
representation combinations are diagnosed at the declaration; concrete obligation failures are
reported at the instantiation with the template module and the offending argument.

Inferred and explicit arguments follow the same checks. Overload resolution defers a rejected
candidate's constraint diagnostic until it knows whether another applicable candidate succeeds.
The complete contract is cloned into specializations and preserved through precompiled NativeSharp
metadata.

`lifetime` currently describes types whose representation participates in the existing ownership
or reference-lifetime model; it does not introduce named lifetime parameters. `notnull` currently
accepts representations proven non-null by construction; nullable/non-null reference annotations
remain a separate language feature.
