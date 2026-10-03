# Bidirectional generic inference / Inferencia genérica bidireccional

## Español

NativeSharp 3 combina evidencia de argumentos con un tipo esperado exacto. El solver es local,
ordenado y terminante: nunca prueba cadenas de conversiones para adivinar un argumento genérico.

```nativesharp
T DefaultOf<T>() { return default(T); }
T Identity<T>(T value) { return value; }

int direct = DefaultOf();
int nested = Identity(DefaultOf());
(int, int) pair = (DefaultOf(), DefaultOf());
```

### Orden del algoritmo

1. Recoge de abajo hacia arriba tipos que la expresión ya conoce: literales, locales, casts,
   arreglos, tipos genéricos cerrados y tuples estructurales.
2. Unifica el retorno de cada candidata con el tipo esperado exacto, si existe.
3. Si una única plantilla visible ya tiene suficiente evidencia para concretar un parámetro,
   propaga ese tipo hacia una llamada anidada, una rama condicional o un elemento tuple.
4. Filtra candidatas inviables, elige la forma estructural más específica y materializa mediante
   la caché genérica ordinaria.

La profundidad contextual usa el límite normativo de 64. No existe búsqueda cíclica ni un conjunto
de conversiones creciente.

### Lambdas, method groups, variants y const parameters

Una lambda explícitamente tipada puede aportar evidencia a un parámetro delegate genérico. Una
lambda sin tipos y un method group no inventan un tipo propio: se aceptan cuando el resto de la
llamada determina un único delegate destino; de lo contrario siguen siendo ambiguos. Tuples se
unifican elemento por elemento. Variants y otros tipos nominales se comparan por su forma genérica
exacta. Los const parameters se infieren desde marcadores const normalizados, incluida una
dimensión fija.

### Desambiguación

`long x = Identity(1)` no cambia `T=int` a `T=long` mediante conversión. Escriba
`Identity<long>(1)` cuando esa sea la intención. Los argumentos explícitos saltan la inferencia,
pero conservan las comprobaciones de aridad, constraints y conversiones semánticas normales.

Un overload no genérico cuyo retorno exacto no puede satisfacer el contexto no desactiva la
inferencia; su posición en el archivo tampoco cambia el resultado. Si dos destinos siguen siendo
viables e incompatibles, el compilador exige argumentos o tipos de lambda explícitos.

## English

NativeSharp 3 combines bottom-up argument evidence with one exact expected type. It first gathers
types already known by the expression, then unifies a candidate return type with the context. When
one visible template has made a parameter fully concrete, that type may flow into a nested call,
conditional branch, or tuple element. The contextual walk is bounded to depth 64 and never tries
arbitrary conversion chains.

Explicitly typed lambdas can contribute evidence through a generic delegate signature. Untyped
lambdas and method groups have no standalone type; they require the remaining evidence to produce
one delegate target. Tuples unify structurally, variants remain nominal, and normalized const
arguments participate in exact unification.

Conflicting evidence fails deterministically. Use explicit `<...>` arguments as the escape hatch;
ordinary semantic conversion and constraint checks still apply afterward. Irrelevant non-generic
overloads cannot change the result merely by being added or reordered.
