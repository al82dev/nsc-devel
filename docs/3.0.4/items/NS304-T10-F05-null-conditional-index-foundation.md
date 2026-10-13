# NS304-T10-F05 — `?[]` null-conditional index

- **Estado**: PARCIAL (token + parser + READ de array nativo) / residual (WRITE + indexador de usuario)
- **Rama**: development

## Resultado

- **Lexer**: `?[` se lexa como un único token `QuestionBracket` (maximal munch en la familia `?`).
- **Parser**: `obj?[i]` baja como `NullConditionalIndexExpression` (receiver en first(), índice en
  children, con soporte de rangos `..` como el `[...]` ordinario).
- **AST**: nuevo `AstNodeKind::NullConditionalIndexExpression` + nombre público.
- **Sema/HIR/LLVM**: `arr?[i]` sobre array nativo baja como `NullConditionalArrayElement` →
  `NullConditionalArrayLoad` (null-check + bounds-check + element-load condicional, reutiliza
  `normalizeNativeIndex`).
- Pruebas: `TP-PAR-537` (parser), `TL-LEX-095/001`, `TA-AST-019..025`, `TL-LANG-345` (READ) → **PASS**.

## Residual

- **WRITE** `obj?[i] = v`: requiere indexador null-conditional direccionable (hoy el element-load
  null-conditional no es addressable).
- **Indexador de usuario** `obj?[i]` sobre tipos con `this[...]`: sólo array nativo soportado.
