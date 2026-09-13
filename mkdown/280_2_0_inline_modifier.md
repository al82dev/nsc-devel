# 280 — Modificador `inline` / `inline` modifier

## Resumen / Summary

`inline` ya es una palabra clave de método concreto. Su política preferida atraviesa parser,
modelo de miembros, HIR y MIR, aparece en dumps y se baja a `inlinehint` de LLVM sin eliminar la
definición fuera de línea.

`inline` is now a concrete-method keyword. Its preferred policy crosses the parser, member model,
HIR, and MIR, appears in dumps, and lowers to LLVM `inlinehint` without removing the out-of-line
definition.

## Archivos y funciones / Files and functions

- `include/nativesharp/Lexer/TokenKind.h`, `src/Lexer/*` — token reservado y nombre estable.
- `include/nativesharp/Parser/Parser.h`, `src/Parser/Parser.cpp` — lectura, duplicados, ubicación
  válida y conservación AST.
- `ProjectMemberModel`, `ProjectOopHirModel`, `ProjectOopMirModel` y sus dumpers — transportan y
  hacen observable `InliningPolicy::Preferred`.
- `src/LLVM/ProjectOopLlvmLowerer.cpp` — agrega `inlinehint` a definiciones preferidas.
- `tests/unit/InliningModifierTests.cpp` — evidencia positiva, negativa e IR verificable.
- `docs/1.0.1/` — gramática, especificación, contratos, mapas, libro, estado y roadmap.

## Implementación y decisiones / Implementation and decisions

La preferencia vive en el callable, no en el call site. Se conserva explícitamente en cada frontera
interna para que partición, genéricos y metadata puedan consumir una sola decisión en entregas
posteriores. LLVM decide rentabilidad y legalidad; `inlinehint` no equivale a `alwaysinline`.

The preference belongs to the callable rather than an individual call site. It is preserved at
every internal boundary so partitioning, generics, and metadata can later consume one decision.
LLVM retains profitability and legality authority; `inlinehint` is not `alwaysinline`.

Se limita a métodos ordinarios con cuerpo. Properties y operadores generan callables internos, pero
aceptar el modificador sobre el contenedor exigiría definir propagación a accessors o símbolos
sintéticos; hacerlo implícitamente produciría una semántica sorpresiva. Esas formas se rechazan.

The modifier is limited to ordinary methods with bodies. Properties and operators synthesize
callables, but accepting it on their container would require an explicit accessor/synthetic-symbol
propagation rule; implicit behavior would be surprising, so those forms are rejected.

## Evidencia dirigida / Focused evidence

Dos métodos —static e instance— producen política preferida, mientras `Main` permanece automático.
El módulo LLVM verifica y contiene `inlinehint`. Fields, properties y métodos abstractos fallan con
diagnóstico que nombra `inline`.

Two methods—static and instance—produce preferred policy while `Main` remains automatic. LLVM IR
verifies and contains `inlinehint`. Fields, properties, and abstract methods fail with a diagnostic
naming `inline`.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Incorpora el modificador inline preferido / Add the preferred inline modifier`

**Descripción / Description**

`Transporta inline desde el lexer hasta LLVM como una preferencia verificable, conserva la definición fuera de línea y rechaza ubicaciones sin cuerpo de método. / Carries inline from lexer to LLVM as a verifiable preference, preserves the out-of-line definition, and rejects placements without a method body.`
