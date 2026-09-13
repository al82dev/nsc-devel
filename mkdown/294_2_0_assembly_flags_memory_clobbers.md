# 294 — Clobbers de flags y memoria / Flag and memory clobbers

## Resultado / Result

`clobber("cc", "memory")` declara explícitamente efectos sobre condition codes y memoria no
enumerada. El AST conserva categorías `flags` y `memory`, distintas de `register`. /
`clobber("cc", "memory")` explicitly declares condition-code and unenumerated-memory effects. The
AST retains `flags` and `memory` categories distinct from `register`.

## Archivos y funciones / Files and functions

- `include/nativesharp/AST/AstNodeKind.h`
  - Define el contrato de categoría en `InlineAssemblyClobber::auxiliaryText()`.
- `src/Parser/Parser.cpp`
  - Reconoce los dos nombres reservados y asigna una categoría estable a todos los clobbers.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp`
  - Certifica categorías especiales y preservación de la categoría de registros.
- `docs/1.0.1`
  - Actualiza especificación, contratos, mapas, estado, changelog y roadmap.

## Implementación y decisiones / Implementation and design decisions

Los efectos especiales reutilizan la lista y sus reglas de orden, case y duplicados, pero no se
modelan como registros falsos. El lowering posterior podrá emitir `~{cc}` y `~{memory}` a partir de
la categoría semántica. / Special effects reuse list ordering, case, and duplicate rules, but are
not modeled as fake registers. Later lowering can emit `~{cc}` and `~{memory}` from the semantic
category.

## Evidencia enfocada / Focused evidence

```text
nsc.integrated-assembly.grammar: PASS: 39 checks
```

No se ejecutó baseline en este commit. / No baseline was run for this commit.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Clasifica clobbers de flags y memoria en asm / Classify asm flag and memory clobbers`

**Descripción / Description**

`Admite cc y memory como efectos reservados y los separa de registros físicos en el AST. / Accept cc and memory as reserved effects and separate them from physical registers in the AST.`
