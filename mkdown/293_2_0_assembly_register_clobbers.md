# 293 — Clobbers de registros de asm / Assembly register clobbers

## Resultado / Result

NativeSharp acepta `clobber("rax", "r11")` después de los operandos asm. Cada entrada conserva un
ordinal estable y un nombre canónico en minúsculas; duplicados y conflictos con registros fijos se
rechazan. / NativeSharp accepts `clobber("rax", "r11")` after asm operands. Every entry retains a
stable ordinal and canonical lowercase name; duplicates and fixed-register conflicts are rejected.

## Archivos y funciones / Files and functions

- `include/nativesharp/AST/AstNodeKind.h` y `src/AST/AstNodeKind.cpp`
  - Agregan `InlineAssemblyClobber` con nombre y ordinal explícitos.
- `include/nativesharp/Diagnostics/DiagnosticId.h`
  - Agrega `InvalidInlineAssemblyClobber`.
- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`
  - `parseInlineAssemblyClobberClause()` analiza la lista no vacía.
  - Canonicaliza case, detecta duplicados y cruza nombres con constraints `{register}`.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp`
  - Certifica orden, canonicalización, duplicados, conflicto fijo, reserva y cláusula vacía.
- `docs/1.0.1`
  - Actualiza EBNF, especificación, contratos, mapas, estado, changelog y roadmap.

## Implementación y decisiones / Implementation and design decisions

El clobber se guarda como nodo y no como string agregado en la sentencia. Así cada nombre mantiene
su ubicación y ordinal para diagnósticos, MIR y PDB posteriores. / A clobber is stored as its own
node rather than an aggregate statement string, preserving location and ordinal for later
diagnostics, MIR, and PDB work.

La comparación es case-insensitive porque los nombres físicos de registros no adquieren identidad
distinta por spelling. La lista no acepta `cc` ni `memory` todavía: el item 294 les asigna efectos
semánticos diferentes a un registro físico. / Comparison is case-insensitive because physical
register identity does not change with spelling. `cc` and `memory` remain excluded until item 294
assigns their distinct semantic effects.

## Evidencia enfocada / Focused evidence

```text
nsc.integrated-assembly.grammar: PASS: 38 checks
```

No se ejecutó baseline en este commit. / No baseline was run for this commit.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Agrega clobbers explícitos de registros a asm / Add explicit register clobbers to asm`

**Descripción / Description**

`Conserva clobbers ordenados y canonicalizados y rechaza duplicados o conflictos con operandos fijos. / Preserve ordered canonical clobbers and reject duplicates or fixed-operand conflicts.`
