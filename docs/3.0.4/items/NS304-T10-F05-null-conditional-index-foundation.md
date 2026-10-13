# NS304-T10-F05 — `?[]` null-conditional index (fundación léxica/sintáctica)

- **Estado**: PARCIAL (token `?[` + parser + AST) / residual (Sema/HIR/LLVM del READ y WRITE)
- **Rama**: development

## Resultado

- **Lexer**: `?[` se lexa como un único token `QuestionBracket` (maximal munch en la familia `?`).
- **Parser**: `obj?[i]` baja como `NullConditionalIndexExpression` (receiver en first(), índice en
  children, con soporte de rangos `..` como el `[...]` ordinario).
- **AST**: nuevo `AstNodeKind::NullConditionalIndexExpression` + nombre público.
- Pruebas: `TL-LEX-095` (`?[`), `TL-LEX-001` (catálogo), `TA-AST-019..025` (catálogo AST),
  `TP-PAR-537` (parser) → **PASS**.

## Residual

- **Sema/HIR/LLVM**: el `NullConditionalIndexExpression` aún no se analiza ni baja (el READ `obj?[i]`
  y el WRITE `obj?[i] = v` requieren la ruta de indexador null-conditional equivalente a la de
  `?.Field`, hoy solo miembro).
