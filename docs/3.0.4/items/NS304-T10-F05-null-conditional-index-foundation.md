# NS304-T10-F05 — `?[]` null-conditional index

- **Estado**: CERRADO (array nativo READ/WRITE + indexador de usuario `obj?[i]`)
- **Rama**: development

## Resultado

- **Lexer**: `?[` se lexa como un único token `QuestionBracket` (maximal munch en la familia `?`).
- **Parser**: `obj?[i]` baja como `NullConditionalIndexExpression` (receiver en first(), índice en
  children, con soporte de rangos `..` como el `[...]` ordinario).
- **AST**: nuevo `AstNodeKind::NullConditionalIndexExpression` + nombre público.
- **Sema/HIR/LLVM**: `arr?[i]` (READ) y `arr?[i] = v` (WRITE) sobre array nativo bajan como
  `NullConditionalArrayLoad`/`NullConditionalArrayStore`.
- **Indexador de usuario**: `obj?[i]` sobre un tipo con `this[...]` resuelve `get_Item`/`set_Item`
  (`stage38AccessorCandidates` + `resolveSyntheticCall`) y baja por `lowerResolvedCall(...,
  nullConditional=true)`: receptor evaluado **una vez**, null-check + llamada al accessor + PHI.
- Pruebas: `TP-PAR-537`, `TL-LEX-095/001`, `TA-AST-019..025`, `TL-LANG-345` (array READ+WRITE) y
  `TL-LANG-402` (indexador usuario `b?[3]` non-null=6, null=0) → **PASS**.

