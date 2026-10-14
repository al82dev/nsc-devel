# NS304-T11-F07 — Ref fields, scoped y summaries

- **Estado**: AUDITADO (lifetime/escape nominal existen; ref fields/scoped intermodular pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar ref fields, scoped y summaries*.
Plan T11-F07 (DeepSeekDocs): *completar definite assignment, mutabilidad, reborrow y escape de ref
fields; publicar summary intermodular suficiente: orígenes de retorno, relación entre parámetros,
`scoped` y lifetime; consumer sin fuentes rechaza el mismo escape que producer*.

Estado real:

- `lifetimeParameters`/`parameterEscapeSummary` ya existen en los símbolos (lifetime/escape nominal).
- Ref fields (campos de referencia) + `scoped`: sin modelado de reborrow/escape específico.
- Summary intermodular: sin publicación de orígenes de retorno/`scoped` para ref fields.

## Gap

Falta el modelado de ref fields (definite assignment, mutabilidad, reborrow, escape) + `scoped`, y el
summary intermodular (orígenes de retorno + relación entre parámetros + `scoped`/lifetime) para que el
consumer rechace el mismo escape sin fuentes.

## Nota

F07 depende de F05 (ref-like) + el lifetime/escape ya existente. No es una surface nueva; es el
completar del escape safety intermodular de ref fields.
