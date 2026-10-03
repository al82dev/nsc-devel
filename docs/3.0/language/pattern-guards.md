# Pattern guards / Guards de patterns

## Español

Un brazo de `match` puede añadir una condición booleana después del pattern estructural:

```nativesharp
match (measurement)
{
    Value(var value) when value > 0 => { Consume(value); },
    Value(var value) => { ReportNonPositive(value); },
    Empty => { ReportEmpty(); }
}
```

El orden es contractual: primero se prueba el pattern, después se crean sus bindings y finalmente
se evalúa el guard. Si el guard es falso, el brazo no se selecciona y la búsqueda continúa en el
siguiente. Un guard nunca se ejecuta cuando falla el tag, literal, rango o estructura que lo
precede; por ello puede usar con seguridad los bindings del pattern.

Los guards aceptan expresiones condicionales booleanas. La asignación y las lambdas no pertenecen
a esta posición: el `=>` termina el guard y abre el cuerpo del brazo. Un resultado no booleano es
un error semántico y `when false` se diagnostica como brazo inalcanzable.

### Cobertura

Un guard dinámico no aporta exhaustividad, aunque el pattern sin guard cubra un caso completo o
sea wildcard. Debe existir otro brazo que cubra el camino donde el guard sea falso. `when true` es
la única prueba estática reconocida en esta etapa y conserva la cobertura normal del pattern.

Los bindings de payload ya exponen el tipo concreto dentro del guard y del cuerpo. El
refinamiento flow-sensitive adicional del mismo nombre por comparaciones de null y por intervalos
enteros requiere el modelo de hechos de flujo posterior; F05 no inventa una segunda tabla de tipos
que el HIR/MIR todavía no pueda conservar.

## English

A `match` arm may place a boolean `when` condition after its structural pattern. Structural
matching runs first, bindings are introduced second, and the guard runs last. A false guard falls
through to the next arm; a guard is never evaluated when its pattern fails.

Dynamic guards do not contribute to exhaustiveness, including guarded wildcard or binding arms.
`when true` is the currently recognized static proof and retains the pattern's ordinary coverage.
`when false` is diagnosed as unreachable. Guards must be boolean conditional expressions; the
arm's `=>` is a delimiter rather than a lambda introducer.

Variant payload bindings are already concretely typed in the guard and arm body. Additional
flow-sensitive facts for null comparisons and integer intervals remain pending until the shared
flow-fact model can carry them through HIR and MIR.
