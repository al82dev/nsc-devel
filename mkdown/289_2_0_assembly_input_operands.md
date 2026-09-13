# 289 — Operandos de entrada de asm / Assembly input operands

## Resultado / Result

`asm { ... } in(expr0, expr1);` declara entradas posicionales que conservan el AST completo de cada
expresión, un ordinal estable y el orden de evaluación escrito. Los placeholders `$0`, `$1`, etc.
quedan reservados para esas posiciones. Las expresiones ya participan en name binding aunque la
emisión assembly continúe bloqueada hasta los ítems de lowering. / `asm { ... }
in(expr0, expr1);` declares positional inputs retaining each complete expression AST, a stable
ordinal, and authored evaluation order. `$0`, `$1`, and subsequent placeholders are reserved for
those positions. Expressions already participate in name binding even though assembly emission
remains blocked until the lowering items.

## Archivos y funciones / Files and functions

- `include/nativesharp/AST/AstNodeKind.h` y `src/AST/AstNodeKind.cpp`
  - Agregan `InlineAssemblyInputOperand`; `first()` contiene la expresión e `integerValue()` el
    ordinal de entrada.
- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`
  - `parseInlineAssemblyInputClause()` exige una lista no vacía, conserva orden y requiere `;`.
- `src/Sema/SemanticAnalyzer.cpp` y `src/Frontend/ProjectMethodBodyModel.cpp`
  - Analizan cada expresión de entrada antes de emitir la frontera temporal de lowering.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp`
  - Certifica estructura, ordinales, binding, cláusula vacía, semicolon y convivencia con texto raw.
- `docs/1.0.1`
  - Actualiza EBNF, especificación, contratos, mapas, estado, changelog y roadmap.

## Implementación y decisiones / Implementation and design decisions

La cláusula va después del bloque para que el lexer assembly termine exactamente en el brace
externo y el parser de expresiones ordinario retome el control sin un lexer híbrido. Un bloque sin
operandos conserva literalmente `asm {}`; sólo la variante con cláusulas requiere semicolon. /
The clause follows the block so assembly lexing ends exactly at the outer brace and the ordinary
expression parser resumes without a hybrid lexer. A block without operands remains literally
`asm {}`; only the clause-bearing form requires a semicolon.

Las entradas son posicionales porque esa identidad es la que consume LLVM inline asm y evita
inventar nombres duplicados entre el lenguaje y la plantilla. El constraint general de registro se
mantiene implícito hasta que el ítem 291 haga explícita y validable esa elección. / Inputs are
positional because LLVM inline assembly consumes that identity and it avoids inventing duplicate
names between the language and template. The general-register constraint remains implicit until
item 291 makes that choice explicit and validated.

## Evidencia enfocada / Focused evidence

```text
ctest -R ^nsc.integrated-assembly.grammar$
19 checks passed
```

No se ejecutó baseline en este commit; corresponde al cierre de la tanda 4. / No baseline was run
for this commit; it belongs to the end-of-batch 4 closure.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Agrega operandos posicionales de entrada a asm / Add positional asm input operands`

**Descripción / Description**

`Incorpora cláusulas in no vacías, ordinales AST estables y binding semántico de expresiones sin adelantar constraints ni lowering. / Add non-empty in clauses, stable AST ordinals, and semantic expression binding without prematurely implementing constraints or lowering.`
