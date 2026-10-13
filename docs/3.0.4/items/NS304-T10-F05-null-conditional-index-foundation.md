# NS304-T10-F05 — `?[]` null-conditional index

- **Estado**: PARCIAL (token + parser + READ/WRITE de array nativo) / residual (indexador de usuario)
- **Rama**: development

## Resultado

- **Lexer**: `?[` se lexa como un único token `QuestionBracket` (maximal munch en la familia `?`).
- **Parser**: `obj?[i]` baja como `NullConditionalIndexExpression` (receiver en first(), índice en
  children, con soporte de rangos `..` como el `[...]` ordinario).
- **AST**: nuevo `AstNodeKind::NullConditionalIndexExpression` + nombre público.
- **Sema/HIR/LLVM**: `arr?[i]` (READ) y `arr?[i] = v` (WRITE) sobre array nativo bajan como
  `NullConditionalArrayLoad`/`NullConditionalArrayStore` (null-check + bounds-check + element
  load/store condicional, reutiliza `normalizeNativeIndex`).
- Pruebas: `TP-PAR-537` (parser), `TL-LEX-095/001`, `TA-AST-019..025`, `TL-LANG-345` (READ+WRITE) → **PASS**.

## Residual

- **Indexador de usuario** `obj?[i]` sobre tipos con `this[...]`: sólo array nativo soportado.

