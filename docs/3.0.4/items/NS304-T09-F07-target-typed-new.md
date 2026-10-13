# NS304-T09-F07 — Target-typed construction sin alterar overload selection

- **Estado**: PARCIAL (`new()` sin argumentos) / residual (`new(args)` target-typed)
- **Rama**: development

## Resultado

- **Parser** (`parsePostfixExpression`): `new()` (paréntesis vacío) se reconoce como
  `ObjectCreationExpression` con marker `target-typed` (TypeSyntax nulo).
- **Sema** (`ProjectMethodBodyModel`): `activeTargetTypeKey_` expone el tipo objetivo (del
  `analyzeAssignment` o del declarador de variable) al `new()`; el site de construcción se registra
  con `targetTypeKey`/`targetTypeOrdinal` inferidos.
- **Construction model** (`ProjectConstructionModel`): permite resolver un site target-typed (sin
  TypeSyntax) cuando `targetTypeOrdinal` ya está resuelto.
- Prueba `TL-LANG-339`: `Point p = new(); p.X + p.Y = 7` → **PASS**.

## Residual

- `new(args)` **target-typed con argumentos**: ambiguo con la construcción `new (storage) T(args)`
  (placement); requiere decidir la desambiguación y el threading del tipo objetivo a los argumentos.
