# SelfHost.Source — documento reconstructivo

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo contradicciones y límites marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión auditada:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Universo:** 2 archivos, 421 líneas, 15216 bytes, 39 declaraciones físicas.
- **Diseño:** 8 contratos y 61 casos futuros; no se implementaron ni ejecutaron pruebas.

## 2. Propósito y frontera

Poseer buffers normalizados y ejecutar la transformación bootstrap de fuente condicional en compile time.

No se atribuyen a este componente decisiones de un productor anterior ni resultados de un consumidor posterior. La frontera de error conserva al productor exacto.

## 3. Entradas, salidas y dependencias

- **Dependencias:** SelfHost.Basic;NativeSharp.Core.IO.
- **Entradas:** las entradas concretas aparecen por contrato; ninguna prueba futura debe sustituirlas por un proxy textual si el contrato es estructural.
- **Salidas:** valores, modelos, diagnósticos, reportes o artefactos nombrados por contrato.
- **Errores:** se verifican por categoría, frontera propietaria y ausencia de publicación falsa; no por puntuación incidental de un mensaje.

### Archivos auditados

| ID | Ruta | Líneas | Declaraciones |
|---|---|---:|---:|
| PF-000616 | `selfhost/nsc/Source/ComptimeProcessor.ns` | 299 | 22 |
| PF-000617 | `selfhost/nsc/Source/SourceBuffer.ns` | 122 | 17 |

## 4. Contratos

### 4.1 CSH-SRC-001 — Compile-time expression cursor and primitives

- **Símbolos:** NativeSharp.Compiler.Source.ComptimeExpression;NativeSharp.Compiler.Source.ComptimeExpression.ComptimeExpression;NativeSharp.Compiler.Source.ComptimeExpression.Space;NativeSharp.Compiler.Source.ComptimeExpression.Digit;NativeSharp.Compiler.Source.ComptimeExpression.Alpha;NativeSharp.Compiler.Source.ComptimeExpression.SkipSpace;NativeSharp.Compiler.Source.ComptimeExpression.Take;NativeSharp.Compiler.Source.ComptimeProcessor.Space;NativeSharp.Compiler.Source.SourceBuffer.Id;NativeSharp.Compiler.Source.SourceBuffer.Path;NativeSharp.Compiler.Source.SourceBuffer.Text;NativeSharp.Compiler.Source.SourceBuffer.RawText;NativeSharp.Compiler.Source.SourceBuffer.RawLength;NativeSharp.Compiler.Source.SourceBuffer.Length;NativeSharp.Compiler.Source.SourceBuffer.LineCount.
- **Entradas:** directive expression text and cursor.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** classified/consumed tokens; classified/consumed tokens are coherent and observable.
- **Invariante:** successful consumption advances and whitespace skipping terminates.
- **Errores:** unexpected text leaves expression invalid.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** cursor trace and consumed characters.

### 4.2 CSH-SRC-002 — Compile-time primary and unary evaluation

- **Símbolos:** NativeSharp.Compiler.Source.ComptimeExpression.ParsePrimary;NativeSharp.Compiler.Source.ComptimeExpression.ParseUnary.
- **Entradas:** parenthesized literal/identifier/unary expression.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** integer value and validity; integer value and validity are coherent and observable.
- **Invariante:** operator binding and parentheses preserve evaluation order.
- **Errores:** missing operand/closer marks evaluation invalid.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** value validity and final cursor.

### 4.3 CSH-SRC-003 — Compile-time precedence evaluation

- **Símbolos:** NativeSharp.Compiler.Source.ComptimeExpression.ParseMultiply;NativeSharp.Compiler.Source.ComptimeExpression.ParseAdd;NativeSharp.Compiler.Source.ComptimeExpression.ParseShift;NativeSharp.Compiler.Source.ComptimeExpression.ParseBitwiseAnd;NativeSharp.Compiler.Source.ComptimeExpression.ParseBitwiseXor;NativeSharp.Compiler.Source.ComptimeExpression.ParseBitwiseOr;NativeSharp.Compiler.Source.ComptimeExpression.Evaluate.
- **Entradas:** integer expression.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** evaluated integer and success; evaluated integer and success are coherent and observable.
- **Invariante:** precedence and associativity follow the declared ladder.
- **Errores:** division/shift/malformed expression reports failure without partial truth.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** value success and full-input consumption.

### 4.4 CSH-SRC-004 — Conditional-source marker recognition

- **Símbolos:** NativeSharp.Compiler.Source.ComptimeProcessor.IdentifierByte;NativeSharp.Compiler.Source.ComptimeProcessor.StartsAt;NativeSharp.Compiler.Source.ComptimeProcessor.StartsWord.
- **Entradas:** source bytes and marker position.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** marker/word match result; marker/word match result are coherent and observable.
- **Invariante:** word matching respects identifier boundaries.
- **Errores:** out-of-range position is a nonmatch.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** truth table and cursor invariance.

### 4.5 CSH-SRC-005 — Conditional-source transformation

- **Símbolos:** NativeSharp.Compiler.Source.ComptimeProcessor;NativeSharp.Compiler.Source.ComptimeProcessor.Transform.
- **Entradas:** source text and compile-time symbols.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** transformed source and first error; transformed source and first error are coherent and observable.
- **Invariante:** included/excluded regions preserve line attribution and nesting state.
- **Errores:** malformed directive nesting or expression returns stable failure.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** output bytes line structure and diagnostic.

### 4.6 CSH-SRC-006 — Diagnostic path normalization

- **Símbolos:** NativeSharp.Compiler.Source.DiagnosticPath;NativeSharp.Compiler.Source.DiagnosticPath.Normalize.
- **Entradas:** Windows or portable path.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** normalized diagnostic path; normalized diagnostic path are coherent and observable.
- **Invariante:** equivalent separators/prefixes yield stable presentation.
- **Errores:** none declared.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** exact path vectors independent of cwd.

### 4.7 CSH-SRC-007 — Source line-ending normalization and mapping

- **Símbolos:** NativeSharp.Compiler.Source.SourceBuffer.NormalizeLineEndings;NativeSharp.Compiler.Source.SourceBuffer.BuildLineStarts;NativeSharp.Compiler.Source.SourceBuffer.GetLineColumn.
- **Entradas:** raw text with CR LF CRLF and offsets.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** normalized LF text line starts and line/column; normalized LF text line starts and line/column are coherent and observable.
- **Invariante:** raw/normalized lengths and every offset remain attributable.
- **Errores:** out-of-range offset follows documented terminal mapping.
- **Efectos/lifetime:** None beyond the owned value/model mutation.
- **Oracle:** text line-start table and exhaustive offset map.

### 4.8 CSH-SRC-008 — Source loading identity and content hash

- **Símbolos:** NativeSharp.Compiler.Source.SourceBuffer;NativeSharp.Compiler.Source.SourceBuffer.SourceBuffer;NativeSharp.Compiler.Source.SourceBuffer.Load;NativeSharp.Compiler.Source.SourceBuffer.LoadFrontend;NativeSharp.Compiler.Source.SourceBuffer.ContentHash.
- **Entradas:** FileId path and file/text bytes.
- **Precondiciones:** All producer inputs named by the contract are present and admitted.
- **Salidas/postcondiciones:** owned SourceBuffer or failure; owned SourceBuffer or failure are coherent and observable.
- **Invariante:** path ID raw text normalized text and stable hash describe one source.
- **Errores:** read/encoding failure returns no accepted buffer.
- **Efectos/lifetime:** May mutate the owned model or emit the named report/artifact only after producer success.
- **Oracle:** fields bytes hash line count and error.

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

La integración directa es: SelfHost.Basic;NativeSharp.Core.IO. El plan de pruebas usa dobles sólo para fronteras externas; para contratos de lenguaje se prefieren modelos reales mínimos y, en la fase de harness, archivos `.ns` externos.

## 11. Contradicciones y límites de autoridad

Las contradicciones globales aplicables están en `CONTRADICTIONS.md`: residuos de edición/metadata, recursión y frontera LLVM-C. Se conservan sin cambios porque esta fase es sólo documental.

## 12. Receta de reconstrucción

1. Recrear primero los tipos y vocabularios inventariados. 2. Implementar cada contrato en el orden del documento. 3. Mantener las dependencias y fronteras de error declaradas. 4. Publicar modelos sólo tras verificar invariantes. 5. Implementar los casos del test plan sin depender de la batería histórica. 6. Contrastar el resultado con los oracles estructurales antes de integración.

## 13. Dictamen

Los 2 archivos, 39 declaraciones, 8 contratos y 61 casos están trazados. Resultado documental: `RECONSTRUIBLE`; no significa suite implementada, ejecución ni cobertura.
