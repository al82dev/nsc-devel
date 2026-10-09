# SelfHost.Lexer — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 4 archivos, 792 líneas, 39170 bytes, 43 declaraciones físicas.
- **Diseño:** 8 contratos y 53 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Convertir fuente normalizada en un stream de tokens con rangos, EOF, errores y dumps deterministas.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.Source;SelfHost.Diagnostics.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000600 | `selfhost/nsc/Lexer/Lexer.ns` | 510 | 27 |
| PF-000601 | `selfhost/nsc/Lexer/TokenDumper.ns` | 58 | 4 |
| PF-000602 | `selfhost/nsc/Lexer/TokenKind.ns` | 187 | 3 |
| PF-000603 | `selfhost/nsc/Lexer/TokenStream.ns` | 37 | 9 |

## 4. Contratos

### 4.1 CSH-LEX-001 — Token vocabulary and stable names

- **Símbolos:** NativeSharp.Compiler.Lexer.Lexer;NativeSharp.Compiler.Lexer.Lexer.Lexer;NativeSharp.Compiler.Lexer.Lexer.Add;NativeSharp.Compiler.Lexer.TokenDumper.AppendEscaped;NativeSharp.Compiler.Lexer.TokenDumper.Dump;NativeSharp.Compiler.Lexer.TokenKind;NativeSharp.Compiler.Lexer.TokenFacts;NativeSharp.Compiler.Lexer.TokenFacts.Name;NativeSharp.Compiler.Lexer.TokenStream.Add;NativeSharp.Compiler.Lexer.TokenStream.Count;NativeSharp.Compiler.Lexer.TokenStream.KindAt;NativeSharp.Compiler.Lexer.TokenStream.BeginAt;NativeSharp.Compiler.Lexer.TokenStream.EndAt;NativeSharp.Compiler.Lexer.TokenStream.ErrorCount;NativeSharp.Compiler.Lexer.TokenStream.AddError.
- **Entradas:** every token enum value and unknown value.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** stable token name; stable token name are coherent and observable.
- **Invariante:** legacy numeric identity and spelling remain stable within frozen implementation.
- **Errores:** unknown value maps to documented fallback.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exhaustive enum/name table.

### 4.2 CSH-LEX-002 — Token stream storage and error count

- **Símbolos:** NativeSharp.Compiler.Lexer.TokenStream;NativeSharp.Compiler.Lexer.TokenStream.TokenStream.
- **Entradas:** token kind begin/end offsets and errors.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** ordered token stream; ordered token stream are coherent and observable.
- **Invariante:** ranges are monotone and count/index tables agree.
- **Errores:** out-of-range access follows current container behavior.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** tokens ranges order and error count.

### 4.3 CSH-LEX-003 — Cursor and character classification

- **Símbolos:** NativeSharp.Compiler.Lexer.Lexer.AtEnd;NativeSharp.Compiler.Lexer.Lexer.Current;NativeSharp.Compiler.Lexer.Lexer.Peek;NativeSharp.Compiler.Lexer.Lexer.Advance;NativeSharp.Compiler.Lexer.Lexer.Match;NativeSharp.Compiler.Lexer.Lexer.IdentifierStart;NativeSharp.Compiler.Lexer.Lexer.IdentifierPart;NativeSharp.Compiler.Lexer.Lexer.DecimalDigit;NativeSharp.Compiler.Lexer.Lexer.HexDigit;NativeSharp.Compiler.Lexer.TokenDumper.HexDigit.
- **Entradas:** source text and cursor.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** classified character or advanced cursor; classified character or advanced cursor are coherent and observable.
- **Invariante:** every successful consume advances and EOF is stable.
- **Errores:** invalid lookahead remains within sentinel behavior.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** cursor trace and classification truth table.

### 4.4 CSH-LEX-004 — Trivia and comments

- **Símbolos:** NativeSharp.Compiler.Lexer.Lexer.SkipLineComment;NativeSharp.Compiler.Lexer.Lexer.SkipBlockComment;NativeSharp.Compiler.Lexer.Lexer.SkipTrivia.
- **Entradas:** whitespace and comment text.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** next significant position; next significant position are coherent and observable.
- **Invariante:** all accepted trivia is skipped once without consuming following token.
- **Errores:** unterminated block comment increments lexer error.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** final cursor error count and next token.

### 4.5 CSH-LEX-005 — Identifiers and keywords

- **Símbolos:** NativeSharp.Compiler.Lexer.Lexer.Keyword;NativeSharp.Compiler.Lexer.Lexer.LexIdentifier.
- **Entradas:** identifier-shaped text.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** identifier or exact keyword token; identifier or exact keyword token are coherent and observable.
- **Invariante:** keyword recognition is exact and identifier boundaries are preserved.
- **Errores:** none declared beyond invalid source character.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** kind lexeme and range.

### 4.6 CSH-LEX-006 — Numeric literals

- **Símbolos:** NativeSharp.Compiler.Lexer.Lexer.LexNumber.
- **Entradas:** decimal hexadecimal floating suffix and malformed forms.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** numeric token and range; numeric token and range are coherent and observable.
- **Invariante:** scanner consumes only the maximal valid numeric candidate.
- **Errores:** malformed prefix/suffix increments error without stalling.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** token kind text range and next cursor.

### 4.7 CSH-LEX-007 — Character string raw and interpolated literals

- **Símbolos:** NativeSharp.Compiler.Lexer.Lexer.DecodeEscape;NativeSharp.Compiler.Lexer.Lexer.LexCharacter;NativeSharp.Compiler.Lexer.Lexer.LexString;NativeSharp.Compiler.Lexer.Lexer.LexRawString;NativeSharp.Compiler.Lexer.Lexer.LexInterpolatedString.
- **Entradas:** quoted literal text including escapes and termination errors.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** literal/interpolation token sequence; literal/interpolation token sequence are coherent and observable.
- **Invariante:** delimiter escape and interpolation boundaries are preserved.
- **Errores:** invalid escape or missing terminator increments error and makes progress.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** decoded value token sequence ranges and error count.

### 4.8 CSH-LEX-008 — Punctuation inline assembly and full lexing

- **Símbolos:** NativeSharp.Compiler.Lexer.Lexer.LexPunctuation;NativeSharp.Compiler.Lexer.Lexer.LexAssemblyBody;NativeSharp.Compiler.Lexer.Lexer.Next;NativeSharp.Compiler.Lexer.Lexer.LexAll;NativeSharp.Compiler.Lexer.TokenDumper.
- **Entradas:** complete source buffer.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** EOF-terminated token stream and deterministic dump; EOF-terminated token stream and deterministic dump are coherent and observable.
- **Invariante:** Next always advances or emits EOF and LexAll terminates.
- **Errores:** unknown punctuation/body termination increments error.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** token sequence EOF range dump and errors.

## 5. Estructuras, estados y algoritmos

Las estructuras concretas son las declaraciones trazadas en `inventory/symbols.csv`. Los algoritmos se dividen por contrato para impedir que una prueba de integración oculte una falla de cursor, identidad, grafo, ownership, ABI o publicación. Los builders conservan estado sólo dentro de su owner; los modelos aceptados se publican después de sus verificadores.

## 6. Invariantes y transiciones

El estado inicial, las mutaciones admitidas, la publicación y el fallo se prueban por contrato. Los contratos de valor son puros; los de builder, grafo, pipeline, proceso o emisión comprueban transiciones y ausencia de éxito parcial.

## 7. Ownership y lifetime

Los tipos `owned`, `raii`, handles, arenas, listas y punteros se caracterizan según la implementación congelada. Una prueba no debe forzar un feature de ownership ajeno al contrato ni convertir una observación actual en política futura del lenguaje.

## 8. Complejidad y límites

Las búsquedas lineales, recorridos de árboles/grafos, cierres y pipelines se ejercitan con vacío, caso nominal y escala. La Fase 1 no promete cotas no declaradas: cualquier recorrido recursivo vigente queda como riesgo observable y contradicción, no como refactor autorizado.

## 9. Errores y recuperación

El oracle identifica categoría, propietario, progreso y publicación. Parser/Lexer deben avanzar o terminar; builders y pipelines detienen consumidores; diagnósticos conservan identidad; emisores no publican éxito si un productor falla.

## 10. Integración

La integración directa es: SelfHost.Source;SelfHost.Diagnostics. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 4 archivos, 43 declaraciones, 8 contratos y 53 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
