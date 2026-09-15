# 317 — Sintaxis de placement construction / Placement construction syntax

## Resultado / Outcome

NativeSharp reconoce una forma inequívoca para construir en storage aportado por el programa:

```nativesharp
Widget* result = new (storage) Widget(42);
```

El parser crea `PlacementConstructionExpression`; `first()` conserva `storage`, `typeSyntax()`
conserva `Widget` y `children()` conserva los argumentos en orden. La lista `()` es obligatoria
incluso para el constructor sin parámetros.

NativeSharp now recognizes `new (storage) T(arguments)` as a distinct placement-construction
expression. Destination storage, constructed type, and ordered arguments have independent AST
positions, leaving no token-pattern recovery for the semantic and lowering stages.

## Frontera deliberada / Deliberate boundary

Este punto sólo fija gramática y representación. Ambas rutas semánticas emiten
`PlacementConstructionNotAvailable` si la expresión intenta compilarse. Así no se confunde una
sintaxis aceptada con ejecución disponible, no se reserva memoria implícita y no se alcanza HIR
antes de que el punto 318 defina:

- tipo y alineación exigidos al destino;
- selección y accesibilidad del constructor;
- evaluación única y orden de efectos;
- tipo del resultado y lowering HIR/MIR/LLVM;
- relación con ownership y destrucción posterior.

Ordinary `new T(args)`, native-array allocation and raw `new T` retain their existing AST forms and
behavior. Placement syntax neither allocates nor lowers in this delivery.

## Cobertura / Coverage

`tests/unit/Item317PlacementConstructionSyntaxTests.cpp` comprueba:

- nodo AST dedicado y rango completo;
- destino separado del tipo y los argumentos;
- expresión compuesta como destino;
- conservación de `new T(args)` ordinario;
- rechazo de placement sin lista de argumentos;
- diagnóstico semántico explícito antes de HIR.

La regresión adyacente incluye las suites generales de parser y constructores. No se ejecuta un
programa placement en O0 todavía porque hacerlo adelantaría exactamente el lowering del punto 318.

## Mensaje de commit / Commit message

`Introduce la sintaxis de placement construction / Introduce placement construction syntax`

`Reconoce new (storage) T(args) con nodo AST propio, estructura estable y una frontera diagnóstica
explícita hasta su lowering. / Recognize new (storage) T(args) with a dedicated AST node, stable
structure, and an explicit diagnostic boundary until lowering is available.`
