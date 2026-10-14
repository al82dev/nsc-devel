# NS304-T11-F05 — Ref-like como argumento genérico

- **Estado**: IMPLEMENTADO (constraint `where T : ref-like` + validación; metadata inválida = refinamiento)
- **Rama**: development

## Resultado (constraint + validación — 2026-10-14)

- **Parser** (`parseGenericConstraintClauses`): `ref-like` se reconoce como keyword contextual de 3
  tokens (`ref` `-` `like`) y fija `GenericConstraintClause::requiresRefLike`.
- **Constraint model**: `GenericConstraintViolationKind::RequiresRefLike` + `isRefLikeArgument`
  (struct `borrowed` = vista Span/ReadOnlySpan); el átomo `"ref-like"` entra en la identidad canónica.
- **Clone** (`ProjectGenericModel`): copia `requiresRefLike` a las cláusulas de la especialización.
- **Validación**: `Wrapper<T> where T : ref-like` + `Wrapper<ReadOnlySpan<int>>` compila y corre
  (exit 5); `Wrapper<int>` se rechaza con *"requires a stack-only borrowed view (Span/ReadOnlySpan)"*.
  Registrado `TL-LANG-414` (**PASS**). El field escapable ya lo niega el check de borrowed-view existente.

## Residual (fuera del surface)

- **Negación del USO — verificada como ya cubierta**: el boxing se rechaza por la conversión
  (`ReadOnlySpan<T>` → `object`); el field escapable y el static storage se rechazan por el check de
  borrowed-view existente. La **metadata inválida** (un `ref-like` en NSMETA) queda sin diagnóstico
  explícito — un refinamiento de metadata, no una surface.
