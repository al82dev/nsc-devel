# NS304-T09-F05 — Collection expressions (fundación léxica/sintáctica)

- **Estado**: PARCIAL (token/AST/parser de `[a, b, c]`) / residual (Sema/type-inference + spreads + builder)
- **Rama**: development

## Resultado

- **AST**: nuevo `AstNodeKind::CollectionExpression` (elementos en children, orden de fuente).
- **Parser**: `[a, b, c]` en posición de expresión primaria se reconoce como collection expression
  (`parseCollectionExpression`); los atributos `[...]` siguen viviendo en posición de declaración.
- Pruebas: `TP-PAR-539` (parser), `TA-AST-019..025` (catálogo AST actualizado) → **PASS**.

## Residual

- **Sema/type-inference**: inferir el tipo de colección objetivo (array/span/builder) y bajar la
  construcción sin heap implícito.
- **Spreads** (`..x`) y **builder protocol**: fuera de esta fundación.
