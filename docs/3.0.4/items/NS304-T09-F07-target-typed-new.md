# NS304-T09-F07 — Target-typed construction sin alterar overload selection

- **Estado**: CERRADO (`new()` y `new(args)`)
- **Rama**: development

## Resultado

- **Parser** (`parsePostfixExpression`): `new()` y `new(args)` se reconocen como
  `ObjectCreationExpression` con marker `target-typed` (TypeSyntax nulo). `new(args)` se desambigua
  de `new (storage) T(args)` (placement) con lookahead: si tras el `)` que cierra viene un tipo, es
  placement.
- **Sema** (`ProjectMethodBodyModel`): `activeTargetTypeKey_` expone el tipo objetivo (del
  `analyzeAssignment` o del declarador de variable) al `new()`; el site de construcción se registra
  con `targetTypeKey`/`targetTypeOrdinal` inferidos.
- **Construction model** (`ProjectConstructionModel`): permite resolver un site target-typed (sin
  TypeSyntax) cuando `targetTypeOrdinal` ya está resuelto.
- Prueba `TL-LANG-339`: `Point p = new(3, 4); p.X + p.Y = 7` → **PASS**.
