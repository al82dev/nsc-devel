# 288 — Lexer y ubicaciones fuente de asm / Assembly lexer and source locations

## Resultado / Result

El contenido de `asm { ... }` ya no atraviesa el lexer de expresiones NativeSharp. Un modo léxico
acotado por los braces externos conserva cada byte del cuerpo y distingue los saltos `CRLF`, `LF` y
`CR`. El AST reconstruye el cuerpo exacto y publica una línea por instrucción con rango que inicia
en su primer carácter significativo. / The contents of `asm { ... }` no longer pass through the
NativeSharp expression lexer. A lexical mode bounded by the outer braces preserves every body byte
and distinguishes CRLF, LF, and CR newlines. The AST reconstructs the exact body and publishes one
record per non-blank instruction line, with a range beginning at its first significant character.

## Archivos y funciones / Files and functions

- `include/nativesharp/Lexer/TokenKind.h`, `include/nativesharp/Lexer/Lexer.h` y
  `src/Lexer/Lexer.cpp`
  - Agregan `AssemblyText` y `AssemblyNewLine`.
  - `Lexer::nextToken()` entra en modo assembly sólo tras el par contextual `asm {`.
  - `Lexer::lexAssemblyBodyToken()` preserva líneas, puntuación y braces internos y devuelve el
    brace externo al parser.
- `include/nativesharp/AST/AstNodeKind.h`, `src/AST/AstNodeKind.cpp` y `src/Parser/Parser.cpp`
  - Agregan `InlineAssemblyLine`, reconstruyen el cuerpo exacto y calculan rangos significativos.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp`
  - Certifica bytes exactos, tokens raw, líneas AST, línea/columna, puntuación ajena al lenguaje,
    braces balanceados, reanudación normal y compatibilidad contextual.
- `docs/1.0.1`
  - Actualiza estado, changelog, especificación, contratos, mapas y roadmap del ítem 288.

## Implementación y decisiones / Implementation and design decisions

El modo se activa en el lexer, no mediante una segunda lectura del archivo en el parser. Así cada
token conserva el `FileId` y offsets originales y los diagnósticos futuros no tendrán que adivinar
columnas desde una cadena normalizada. `asm` sigue siendo contextual: el lexer sólo arma el modo si
el siguiente token real es `{`; comentarios y whitespace intermedios no lo cancelan. / The mode is
activated in the lexer rather than by re-reading the file in the parser. Every token therefore
retains its original `FileId` and offsets, and future diagnostics do not need to guess columns from
a normalized string. `asm` remains contextual: the lexer arms the mode only when the next real token
is `{`; intervening comments and whitespace do not cancel it.

Los tokens raw se dividen por salto físico y no por instrucción semántica. Esta entrega puede
preservar Intel syntax, directivas, comentarios o signos todavía no definidos sin declararlos
válidos. El análisis de operandos comienza en 289. / Raw tokens split on physical newlines, not on
semantic instructions. This delivery can preserve Intel syntax, directives, comments, and
otherwise undefined signs without declaring them valid. Operand analysis begins in item 289.

## Evidencia enfocada / Focused evidence

```text
ctest -R ^nsc.integrated-assembly.grammar$
15 checks passed
```

No se ejecutó baseline en este commit; corresponde al cierre de la tanda 4. / No baseline was run
for this commit; it belongs to the end-of-batch 4 closure.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Preserva texto y ubicaciones fuente de asm / Preserve asm text and source locations`

**Descripción / Description**

`Introduce un modo léxico contextual para cuerpos asm, conserva bytes y saltos físicos exactos y registra cada línea significativa con su rango fuente. / Introduce a contextual lexical mode for asm bodies, preserve exact bytes and physical newlines, and record every significant line with its source range.`
