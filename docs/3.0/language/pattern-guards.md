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

### Refinamiento de flujo

Las comparaciones sobre un local o parámetro refinan ese mismo almacenamiento dentro de la rama:
`x != null` demuestra un receptor no nulo, las relaciones con constantes acotan intervalos y el
test de un caso variant demuestra el payload activo. Los hechos atraviesan `&&`, `||` y `!`, pero
un join conserva solamente lo que todas las rutas alcanzables demuestran.

Una asignación al nombre o su paso como argumento `ref`/`out` invalida el refinamiento. Una llamada
ordinaria por valor no puede reasignar ese almacenamiento y no lo invalida. Cuando el hecho basta,
HIR/MIR publican la decisión y LLVM omite únicamente el check redundante; el comportamiento sin
prueba permanece idéntico.

## English

A `match` arm may place a boolean `when` condition after its structural pattern. Structural
matching runs first, bindings are introduced second, and the guard runs last. A false guard falls
through to the next arm; a guard is never evaluated when its pattern fails.

Dynamic guards do not contribute to exhaustiveness, including guarded wildcard or binding arms.
`when true` is the currently recognized static proof and retains the pattern's ordinary coverage.
`when false` is diagnosed as unreachable. Guards must be boolean conditional expressions; the
arm's `=>` is a delimiter rather than a lambda introducer.

### Flow narrowing

Comparisons on a local or parameter refine that same storage inside the branch: `x != null`
proves a non-null receiver, relations against constants bound integer intervals, and a variant
case test proves the active payload. Facts propagate through `&&`, `||`, and `!`; a join retains
only facts proven by every reachable path.

Assignment to the name or passing it through `ref`/`out` invalidates its facts. An ordinary
by-value call cannot reassign the storage and keeps them. HIR/MIR expose each consumed proof and
LLVM removes only the corresponding redundant check; unproven paths keep their original checks.
