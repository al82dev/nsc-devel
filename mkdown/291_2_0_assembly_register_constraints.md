# 291 — Restricciones de registros de asm / Assembly register constraints

## Resultado / Result

NativeSharp permite anotar cada operando como `expresión : "constraint"`. Las entradas aceptan
registro general o fijo; las salidas distinguen escritura y lectura-escritura. La sintaxis anterior
sin anotación permanece válida mediante defaults normalizados. / NativeSharp allows every operand
to be annotated as `expression : "constraint"`. Inputs accept a general or fixed register;
outputs distinguish write-only from read-write. Previous unannotated syntax remains valid through
normalized defaults.

## Archivos y funciones / Files and functions

- `include/nativesharp/AST/AstNodeKind.h`
  - Documenta `auxiliaryText()` como constraint normalizado y `flag()` como marca explícita.
- `include/nativesharp/Diagnostics/DiagnosticId.h`
  - Agrega `InvalidInlineAssemblyConstraint` para errores de contrato, no fallos internos.
- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`
  - `parseInlineAssemblyRegisterConstraint()` analiza, normaliza y valida dirección y estructura.
  - `parseInlineAssemblyInputClause()` y `parseInlineAssemblyOutputClause()` adjuntan el contrato.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp`
  - Certifica defaults, constraints explícitos generales/fijos, lectura-escritura y rechazos.
- `docs/1.0.1`
  - Actualiza EBNF, especificación, contratos, mapas, estado, changelog y roadmap.

## Implementación y decisiones / Implementation and design decisions

Los defaults `r` y `=r` mantienen compatibles los programas de los items 289 y 290 y dan al AST
una representación uniforme: ningún consumidor posterior necesita interpretar un string vacío. La
marca explícita se conserva por separado para diagnósticos, dumps y evolución del ABI. / Defaults
`r` and `=r` keep items 289 and 290 source-compatible and give the AST one uniform representation:
no later consumer needs to interpret an empty string. Explicitness remains separate for diagnostics,
dumps, and ABI evolution.

El parser valida sólo el vocabulario independiente del target: clase general, registro fijo y
dirección input/output. No afirma que `rax`, `xmm0` u otro nombre exista para la CPU seleccionada;
esa responsabilidad corresponde al item 297. / The parser validates only target-independent
vocabulary: general class, fixed register, and input/output direction. It does not claim that a
specific name exists for the selected CPU; item 297 owns that decision.

## Evidencia enfocada / Focused evidence

```text
nsc.integrated-assembly.grammar: PASS: 28 checks
nsc.stage60.language-contract: passed
nsc.stage61.compiler-hardening: passed
```

No se ejecutó baseline en este commit; corresponde al cierre de la tanda 4. / No baseline was run
for this commit; it belongs to the end-of-batch 4 closure.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Agrega restricciones explícitas de registros a asm / Add explicit asm register constraints`

**Descripción / Description**

`Normaliza constraints de entrada y salida, admite registros generales y fijos y diagnostica formas o direcciones inválidas. / Normalize input and output constraints, support general and fixed registers, and diagnose invalid forms or directions.`
