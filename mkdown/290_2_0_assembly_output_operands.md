# 290 — Operandos de salida de asm / Assembly output operands

## Resultado / Result

NativeSharp acepta `asm { ... } out(destinos) in(entradas);`. Los outputs ocupan una cláusula
ordenada anterior a inputs, tienen ordinales `$outN` independientes y sólo admiten lvalues
modificables. El análisis los trata como escritura, no como lectura del valor previo. / NativeSharp
accepts `asm { ... } out(targets) in(inputs);`. Outputs occupy an ordered clause before inputs, use
independent `$outN` ordinals, and only accept modifiable lvalues. Analysis treats them as writes,
not reads of a prior value.

## Archivos y funciones / Files and functions

- `include/nativesharp/AST/AstNodeKind.h` y `src/AST/AstNodeKind.cpp`
  - Agregan `InlineAssemblyOutputOperand` con destino en `first()` y ordinal en `integerValue()`.
- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`
  - `parseInlineAssemblyOutputClause()` analiza una lista no vacía antes de `in`.
- `src/Sema/SemanticAnalyzer.cpp` y `src/Frontend/ProjectMethodBodyModel.cpp`
  - Exigen destino modificable y activan la ruta de write-target/definite assignment.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp`
  - Certifica orden, ordinal, coexistencia con inputs, cláusula vacía y literal no asignable.
- `docs/1.0.1`
  - Actualiza EBNF, especificación, contratos, mapas, estado, changelog y roadmap.

## Implementación y decisiones / Implementation and design decisions

Outputs e inputs mantienen espacios ordinales separados. Esto permite agregar o quitar destinos sin
cambiar el significado de `$0` en una plantilla de sólo entrada y hace visible la dirección de
datos en el propio placeholder. / Outputs and inputs keep separate ordinal spaces. This allows
adding or removing targets without changing `$0` in an input-only template and makes data direction
visible in the placeholder itself.

Se usa la validación existente de lvalue y el mismo marcador de write target que una asignación
simple. No se sintetiza una lectura ni un valor ficticio para inicializar el destino. / Existing
lvalue validation and the same write-target marker as simple assignment are reused. No read or
dummy initialization value is synthesized for the destination.

## Evidencia enfocada / Focused evidence

```text
ctest -R ^nsc.integrated-assembly.grammar$
22 checks passed
```

No se ejecutó baseline en este commit; corresponde al cierre de la tanda 4. / No baseline was run
for this commit; it belongs to the end-of-batch 4 closure.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Agrega destinos posicionales de salida a asm / Add positional asm output targets`

**Descripción / Description**

`Incorpora cláusulas out ordenadas, placeholders $outN y validación de lvalues con semántica de escritura y asignación definida. / Add ordered out clauses, $outN placeholders, and lvalue validation with write and definite-assignment semantics.`
