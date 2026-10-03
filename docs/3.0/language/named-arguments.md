# NativeSharp 3.0 — Argumentos nombrados / Named arguments

## Español

Los argumentos nombrados usan `nombre: expresión`:

```nativesharp
Draw(x: 10, color: red, y: 20);
```

El compilador forma primero el conjunto de sobrecargas por el receptor y el nombre del callable.
Después aplica cada nombre contra los parámetros de cada candidata y descarta las candidatas que
no puedan cubrir la llamada. Un nombre desconocido, un parámetro asignado dos veces o la ausencia
de un parámetro obligatorio hacen que esa candidata no sea viable.

Los argumentos posicionales se pueden mezclar con argumentos nombrados. Cada posicional ocupa el
primer parámetro todavía libre; por eso la regla es determinista incluso después de un argumento
nombrado. Los parámetros omitidos deben tener un valor predeterminado.

Las expresiones se evalúan exactamente una vez y de izquierda a derecha, en el orden escrito. Los
valores ya evaluados se colocan después en el ordinal físico que espera el callable. Cambiar el
orden de los nombres no cambia la firma, el símbolo enlazado ni el ABI de una llamada posicional
equivalente.

El nombre de un parámetro público sí forma parte del contrato de fuente. NSMETA lo conserva para
que un consumidor precompilado pueda escribir la llamada nombrada. Renombrarlo no cambia el ABI
físico, pero rompe el código fuente que usa el nombre anterior; los autores de bibliotecas deben
tratar ese cambio como incompatible y reservarlo para una versión mayor o mantener una API de
compatibilidad.

## English

Named arguments use `name: expression`:

```nativesharp
Draw(x: 10, color: red, y: 20);
```

The compiler first forms the overload set from the receiver and callable name. It then maps each
written name against every candidate's parameters and removes candidates that cannot represent
the call. An unknown name, a parameter assigned twice, or a missing required parameter makes that
candidate non-viable.

Positional and named arguments may be mixed. Each positional argument occupies the first
unassigned parameter, which keeps the rule deterministic even after a named argument. Every
omitted parameter must provide a default value.

Expressions are evaluated exactly once, from left to right, in written source order. The evaluated
values are then placed in the physical parameter ordinals required by the callable. Reordering
names does not change the signature, linked symbol, or ABI of the equivalent positional call.

A public parameter name is part of the source contract. NSMETA preserves it so a precompiled
consumer can use named-call syntax. Renaming the parameter is ABI-neutral but source-breaking for
callers that use the old name; library authors should reserve such a change for a major version or
retain a compatibility API.
