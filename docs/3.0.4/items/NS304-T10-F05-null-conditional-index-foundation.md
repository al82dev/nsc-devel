# NS304-T10-F05 — `?[]` null-conditional index

- **Estado**: PARCIAL (token + parser + READ/WRITE de array nativo) / residual (indexador de usuario)
- **Rama**: development

## Resultado

- **Lexer**: `?[` se lexa como un único token `QuestionBracket` (maximal munch en la familia `?`).
- **Parser**: `obj?[i]` baja como `NullConditionalIndexExpression` (receiver en first(), índice en
  children, con soporte de rangos `..` como el `[...]` ordinario).
- **AST**: nuevo `AstNodeKind::NullConditionalIndexExpression` + nombre público.
- **Sema/HIR/LLVM**: `arr?[i]` (READ) y `arr?[i] = v` (WRITE) sobre array nativo bajan como
  `NullConditionalArrayLoad`/`NullConditionalArrayStore`. El receptor se evalúa una vez y el
  índice, bounds-check y RHS del store existen sólo en la rama no nula; load/store devuelven un
  PHI con null/valor efectivo.
- Pruebas: `TP-PAR-537` (parser), `TL-LEX-095/001`, `TA-AST-019..025`, `TL-LANG-345` (READ+WRITE,
  contadores de receptor/índice/RHS y resultados null/no-null) → **PASS** (2026-10-13).

## Residual

- **Indexador de usuario** `obj?[i]` sobre tipos con `this[...]`: sólo array nativo soportado.
  Verificación 2026-10-14: el indexador ordinario `b[3]` sobre `class Box { int this[int i] }` SÍ
  funciona (exit 6); `b?[3]` da `null-conditional index access ('obj?[i]') is only supported on
  native arrays`. El tramo es: Sema detecta el indexador de usuario (receptor + índice → `get_`/`set_`
  del `this[...]`) + HIR/LLVM emite el null-check y la llamada al accessor (análogo al array nativo,
  pero con un member ordinal en lugar de bounds-check de array).

