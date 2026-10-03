# Const generics / Parámetros const genéricos

## Español

NativeSharp 3 permite que una plantilla reciba valores conocidos durante compilación. Esos valores
forman parte de la identidad de la especialización y se sustituyen directamente en el AST; no
existe diccionario, boxing ni parámetro oculto en runtime.

```nativesharp
struct Buffer<T, const int N>
{
    public T[N] Data;
    public int Count() { return N; }
}

enum Mode : byte { None = 0, Fast = 2 }

int Select<const Mode M>() { return (int)M; }
int Sum<const (int, bool) Key>()
{
    var (value, enabled) = Key;
    return enabled ? value : 0;
}
```

### Tipos admitidos

- `bool`, `char` y todos los enteros builtin.
- Un enum concreto. Su identidad es nominal: dos enums con los mismos bits no son intercambiables.
- Tuplas estructurales cuyos elementos sean, recursivamente, tipos const admitidos.

Una tupla const es un valor estructural aprobado, no cualquier `struct` del usuario. La restricción
mantiene estable la igualdad, la serialización y el mangling mientras no exista una definición
formal de igualdad estructural para campos, padding y versiones de tipos nominales.

### Argumentos y evaluación

Se admiten literales, valores enum, tuplas y expresiones enteras deterministas cubiertas por el
evaluador constante. Una expresión compuesta dentro de `<...>` se encierra entre paréntesis:

```nativesharp
Buffer<int, (2 + 2)> four;
Select<Mode.Fast>();
Sum<(42, true)>();
```

Llamadas runtime, división por cero, shifts inválidos y formas que el evaluador no puede demostrar
constantes se rechazan. El resultado se comprueba contra el tipo declarado después de evaluarse;
por ejemplo, `(255 + 1)` no cabe en un parámetro `const byte`.

### Identidad, inferencia y límites

La clave contiene el tipo declarado y el valor normalizado. Por ello `4`, `4u` y `(2 + 2)` usados
para un parámetro `const int` comparten una especialización. Los enums incorporan módulo y tipo
declarador; las tuplas usan una codificación recursiva con longitudes. El nombre físico sólo usa
caracteres seguros para símbolos Win64 y un hash estable.

Un parámetro const puede inferirse cuando una firma aporta evidencia exacta, incluido un argumento
const de otro tipo genérico, el tipo esperado exacto o una dimensión fija `T[N]`. Evidencias
contradictorias fallan; el compilador no adivina mediante conversiones arbitrarias. Los límites
generales de monomorfización siguen siendo profundidad 64 y 16384 especializaciones nuevas.

## English

NativeSharp 3 templates may take compile-time values. Supported parameter types are `bool`,
`char`, builtin integers, a concrete nominal enum, and recursively approved structural tuples.
Values are evaluated, range-checked against the declared parameter type, normalized, and embedded
in specialization identity. No runtime dictionary, boxing, or hidden ABI argument is introduced.

Compound expressions inside `<...>` are parenthesized. Runtime calls, undefined constant
arithmetic, and unevaluable forms are rejected. Equal normalized values reuse one specialization;
enum identity includes its declaring module and type, while structural tuples use an unambiguous
recursive encoding. Physical specialization names contain only Win64-safe characters plus a stable
hash.

Exact evidence may infer a const parameter from another closed generic argument, an exact expected
type, or a fixed-array extent. Conflicting evidence fails and inference never searches through
arbitrary conversions. Const instantiations share the normal depth (64) and count (16384) budgets.

